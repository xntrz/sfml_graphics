////////////////////////////////////////////////////////////
//
// SFML - Simple and Fast Multimedia Library
// Copyright (C) 2007-2018 Laurent Gomila (laurent@sfml-dev.org)
//
// This software is provided 'as-is', without any express or implied warranty.
// In no event will the authors be held liable for any damages arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it freely,
// subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented;
//    you must not claim that you wrote the original software.
//    If you use this software in a product, an acknowledgment
//    in the product documentation would be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such,
//    and must not be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source distribution.
//
////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Graphics/Shader.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/Transform.hpp>
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/GLCheck.hpp>
#include <SFML/System/Mutex.hpp>
#include <SFML/System/Lock.hpp>
#include <SFML/System/Err.hpp>
#include <fstream>
#include <vector>


#if defined(SFML_SYSTEM_MACOS) || defined(SFML_SYSTEM_IOS)

    #define castToGlHandle(x) reinterpret_cast<unsigned int>(static_cast<ptrdiff_t>(x))
    #define castFromGlHandle(x) static_cast<unsigned int>(reinterpret_cast<ptrdiff_t>(x))

#else

    #define castToGlHandle(x) (x)
    #define castFromGlHandle(x) (x)

#endif

namespace
{
    sf::Mutex maxTextureUnitsMutex;
    sf::Mutex isAvailableMutex;

    GLint checkMaxTextureUnits()
    {
        GLint maxUnits = 0;
        glCheck(glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &maxUnits));

        return maxUnits;
    }

    // Retrieve the maximum number of texture units available
    GLint getMaxTextureUnits()
    {
        // TODO: Remove this lock when it becomes unnecessary in C++11
        sf::Lock lock(maxTextureUnitsMutex);

        static GLint maxUnits = checkMaxTextureUnits();

        return maxUnits;
    }

    // Read the contents of a file into an array of char
    bool getFileContents(const char* filename, std::vector<char>& buffer)
    {
        std::ifstream file(filename, std::ios_base::binary);
        if (file)
        {
            file.seekg(0, std::ios_base::end);
            std::streamsize size = file.tellg();
            if (size > 0)
            {
                file.seekg(0, std::ios_base::beg);
                buffer.resize(static_cast<std::size_t>(size));
                file.read(&buffer[0], size);
            }
            buffer.push_back('\0');
            return true;
        }
        else
        {
            return false;
        }
    }

    // Transforms an array of 2D vectors into a contiguous array of scalars
    template <typename T>
    std::vector<T> flatten(const sf::Vector2<T>* vectorArray, std::size_t length)
    {
        const std::size_t vectorSize = 2;

        std::vector<T> contiguous(vectorSize * length);
        for (std::size_t i = 0; i < length; ++i)
        {
            contiguous[vectorSize * i]     = vectorArray[i].x;
            contiguous[vectorSize * i + 1] = vectorArray[i].y;
        }

        return contiguous;
    }

    // Transforms an array of 3D vectors into a contiguous array of scalars
    template <typename T>
    std::vector<T> flatten(const sf::Vector3<T>* vectorArray, std::size_t length)
    {
        const std::size_t vectorSize = 3;

        std::vector<T> contiguous(vectorSize * length);
        for (std::size_t i = 0; i < length; ++i)
        {
            contiguous[vectorSize * i]     = vectorArray[i].x;
            contiguous[vectorSize * i + 1] = vectorArray[i].y;
            contiguous[vectorSize * i + 2] = vectorArray[i].z;
        }

        return contiguous;
    }

    // Transforms an array of 4D vectors into a contiguous array of scalars
    template <typename T>
    std::vector<T> flatten(const sf::priv::Vector4<T>* vectorArray, std::size_t length)
    {
        const std::size_t vectorSize = 4;

        std::vector<T> contiguous(vectorSize * length);
        for (std::size_t i = 0; i < length; ++i)
        {
            contiguous[vectorSize * i]     = vectorArray[i].x;
            contiguous[vectorSize * i + 1] = vectorArray[i].y;
            contiguous[vectorSize * i + 2] = vectorArray[i].z;
            contiguous[vectorSize * i + 3] = vectorArray[i].w;
        }

        return contiguous;
    }
}


namespace sf
{
////////////////////////////////////////////////////////////
Shader::CurrentTextureType Shader::CurrentTexture;


////////////////////////////////////////////////////////////
struct Shader::UniformBinder : private NonCopyable
{
    ////////////////////////////////////////////////////////////
    /// \brief Constructor: set up state before uniform is set
    ///
    ////////////////////////////////////////////////////////////
    UniformBinder(Shader& shader, const char* name) :
    savedProgram(0),
    currentProgram(castToGlHandle(shader.m_shaderProgram)),
    location(-1)
    {
        if (currentProgram)
        {
            // Enable program object
            glCheck(glGetIntegerv(GL_CURRENT_PROGRAM, reinterpret_cast<int*>(&savedProgram)));
            if (currentProgram != savedProgram)
                glCheck(glUseProgram(currentProgram));

            // Store uniform location for further use outside constructor
            location = shader.getUniformLocation(name);
        }
    }

    ////////////////////////////////////////////////////////////
    /// \brief Destructor: restore state after uniform is set
    ///
    ////////////////////////////////////////////////////////////
    ~UniformBinder()
    {
        // Disable program object
        if (currentProgram && (currentProgram != savedProgram))
            glCheck(glUseProgram(savedProgram));
    }

    unsigned int         savedProgram;   ///< Handle to the previously active program object
    unsigned int         currentProgram; ///< Handle to the program object of the modified sf::Shader instance
    GLint                location;       ///< Uniform location, used by the surrounding sf::Shader code
};


////////////////////////////////////////////////////////////
Shader::Shader() :
m_shaderProgram (0),
m_currentTexture(-1),
m_textures      (),
m_uniforms      ()
{
    
}


////////////////////////////////////////////////////////////
Shader::~Shader()
{
    // Destroy effect program
    if (m_shaderProgram)
        glCheck(glDeleteProgram(castToGlHandle(m_shaderProgram)));
}


////////////////////////////////////////////////////////////
bool Shader::loadFromFile(const char* filename, Type type)
{
    // Read the file
    std::vector<char> shader;
    if (!getFileContents(filename, shader))
    {
        err() << "Failed to open shader file \"" << filename << "\"" << std::endl;
        return false;
    }

    // Compile the shader program
    if (type == Vertex)
        return compile(&shader[0], NULL, NULL);
    else if (type == Geometry)
        return compile(NULL, &shader[0], NULL);
    else
        return compile(NULL, NULL, &shader[0]);
}


////////////////////////////////////////////////////////////
bool Shader::loadFromFile(const char* vertexShaderFilename, const char* fragmentShaderFilename)
{
    // Read the vertex shader file
    std::vector<char> vertexShader;
    if (!getFileContents(vertexShaderFilename, vertexShader))
    {
        err() << "Failed to open vertex shader file \"" << vertexShaderFilename << "\"" << std::endl;
        return false;
    }

    // Read the fragment shader file
    std::vector<char> fragmentShader;
    if (!getFileContents(fragmentShaderFilename, fragmentShader))
    {
        err() << "Failed to open fragment shader file \"" << fragmentShaderFilename << "\"" << std::endl;
        return false;
    }

    // Compile the shader program
    return compile(&vertexShader[0], NULL, &fragmentShader[0]);
}


////////////////////////////////////////////////////////////
bool Shader::loadFromFile(const char* vertexShaderFilename, const char* geometryShaderFilename, const char* fragmentShaderFilename)
{
    // Read the vertex shader file
    std::vector<char> vertexShader;
    if (!getFileContents(vertexShaderFilename, vertexShader))
    {
        err() << "Failed to open vertex shader file \"" << vertexShaderFilename << "\"" << std::endl;
        return false;
    }

    // Read the geometry shader file
    std::vector<char> geometryShader;
    if (!getFileContents(geometryShaderFilename, geometryShader))
    {
        err() << "Failed to open geometry shader file \"" << geometryShaderFilename << "\"" << std::endl;
        return false;
    }

    // Read the fragment shader file
    std::vector<char> fragmentShader;
    if (!getFileContents(fragmentShaderFilename, fragmentShader))
    {
        err() << "Failed to open fragment shader file \"" << fragmentShaderFilename << "\"" << std::endl;
        return false;
    }

    // Compile the shader program
    return compile(&vertexShader[0], &geometryShader[0], &fragmentShader[0]);
}


////////////////////////////////////////////////////////////
bool Shader::loadFromMemory(const char* shader, Type type)
{
    // Compile the shader program
    if (type == Vertex)
        return compile(shader, NULL, NULL);
    else if (type == Geometry)
        return compile(NULL, shader, NULL);
    else
        return compile(NULL, NULL, shader);
}


////////////////////////////////////////////////////////////
bool Shader::loadFromMemory(const char* vertexShader, const char* fragmentShader)
{
    // Compile the shader program
    return compile(vertexShader, NULL, fragmentShader);
}


////////////////////////////////////////////////////////////
bool Shader::loadFromMemory(const char* vertexShader, const char* geometryShader, const char* fragmentShader)
{
    // Compile the shader program
    return compile(vertexShader, geometryShader, fragmentShader);
}


////////////////////////////////////////////////////////////
bool Shader::loadFromStream(InputStream& stream, Type type)
{
    return false;
}


////////////////////////////////////////////////////////////
bool Shader::loadFromStream(InputStream& vertexShaderStream, InputStream& fragmentShaderStream)
{
    return false;
}


////////////////////////////////////////////////////////////
bool Shader::loadFromStream(InputStream& vertexShaderStream, InputStream& geometryShaderStream, InputStream& fragmentShaderStream)
{
    return false;
}


////////////////////////////////////////////////////////////
void Shader::setUniform(const char* name, float x)
{
    UniformBinder binder(*this, name);
    if (binder.location != -1)
        glCheck(glUniform1f(binder.location, x));
}


////////////////////////////////////////////////////////////
void Shader::setUniform(const char* name, const Glsl::Vec2& v)
{
    UniformBinder binder(*this, name);
    if (binder.location != -1)
        glCheck(glUniform2f(binder.location, v.x, v.y));
}


////////////////////////////////////////////////////////////
void Shader::setUniform(const char* name, const Glsl::Vec3& v)
{
    UniformBinder binder(*this, name);
    if (binder.location != -1)
        glCheck(glUniform3f(binder.location, v.x, v.y, v.z));
}


////////////////////////////////////////////////////////////
void Shader::setUniform(const char* name, const Glsl::Vec4& v)
{
    UniformBinder binder(*this, name);
    if (binder.location != -1)
        glCheck(glUniform4f(binder.location, v.x, v.y, v.z, v.w));
}


////////////////////////////////////////////////////////////
void Shader::setUniform(const char* name, int x)
{
    UniformBinder binder(*this, name);
    if (binder.location != -1)
        glCheck(glUniform1i(binder.location, x));
}


////////////////////////////////////////////////////////////
void Shader::setUniform(const char* name, const Glsl::Ivec2& v)
{
    UniformBinder binder(*this, name);
    if (binder.location != -1)
        glCheck(glUniform2i(binder.location, v.x, v.y));
}


////////////////////////////////////////////////////////////
void Shader::setUniform(const char* name, const Glsl::Ivec3& v)
{
    UniformBinder binder(*this, name);
    if (binder.location != -1)
        glCheck(glUniform3i(binder.location, v.x, v.y, v.z));
}


////////////////////////////////////////////////////////////
void Shader::setUniform(const char* name, const Glsl::Ivec4& v)
{
    UniformBinder binder(*this, name);
    if (binder.location != -1)
        glCheck(glUniform4i(binder.location, v.x, v.y, v.z, v.w));
}


////////////////////////////////////////////////////////////
void Shader::setUniform(const char* name, bool x)
{
    setUniform(name, static_cast<int>(x));
}


////////////////////////////////////////////////////////////
void Shader::setUniform(const char* name, const Glsl::Bvec2& v)
{
    setUniform(name, Glsl::Ivec2(v));
}


////////////////////////////////////////////////////////////
void Shader::setUniform(const char* name, const Glsl::Bvec3& v)
{
    setUniform(name, Glsl::Ivec3(v));
}


////////////////////////////////////////////////////////////
void Shader::setUniform(const char* name, const Glsl::Bvec4& v)
{
    setUniform(name, Glsl::Ivec4(v));
}


////////////////////////////////////////////////////////////
void Shader::setUniform(const char* name, const Glsl::Mat3& matrix)
{
    UniformBinder binder(*this, name);
    if (binder.location != -1)
        glCheck(glUniformMatrix3fv(binder.location, 1, GL_FALSE, matrix.array));
}


////////////////////////////////////////////////////////////
void Shader::setUniform(const char* name, const Glsl::Mat4& matrix)
{
    UniformBinder binder(*this, name);
    if (binder.location != -1)
        glCheck(glUniformMatrix4fv(binder.location, 1, GL_FALSE, matrix.array));
}


////////////////////////////////////////////////////////////
void Shader::setUniform(const char* name, const Texture& texture)
{
    if (m_shaderProgram)
    {
        // Find the location of the variable in the shader
        int location = getUniformLocation(name);
        if (location != -1)
        {
            // Store the location -> texture mapping
            TextureTable::iterator it = m_textures.find(location);
            if (it == m_textures.end())
            {
                // New entry, make sure there are enough texture units
                GLint maxUnits = getMaxTextureUnits();
                if (m_textures.size() + 1 >= static_cast<std::size_t>(maxUnits))
                {
                    err() << "Impossible to use texture \"" << name << "\" for shader: all available texture units are used" << std::endl;
                    return;
                }

                m_textures[location] = &texture;
            }
            else
            {
                // Location already used, just replace the texture
                it->second = &texture;
            }
        }
    }
}


////////////////////////////////////////////////////////////
void Shader::setUniform(const char* name, CurrentTextureType)
{
    if (m_shaderProgram)
    {
        // Find the location of the variable in the shader
        m_currentTexture = getUniformLocation(name);
    }
}


////////////////////////////////////////////////////////////
void Shader::setUniformArray(const char* name, const float* scalarArray, std::size_t length)
{
    UniformBinder binder(*this, name);
    if (binder.location != -1)
        glCheck(glUniform1fv(binder.location, static_cast<GLsizei>(length), scalarArray));
}


////////////////////////////////////////////////////////////
void Shader::setUniformArray(const char* name, const Glsl::Vec2* vectorArray, std::size_t length)
{
    std::vector<float> contiguous = flatten(vectorArray, length);

    UniformBinder binder(*this, name);
    if (binder.location != -1)
        glCheck(glUniform2fv(binder.location, static_cast<GLsizei>(length), &contiguous[0]));
}


////////////////////////////////////////////////////////////
void Shader::setUniformArray(const char* name, const Glsl::Vec3* vectorArray, std::size_t length)
{
    std::vector<float> contiguous = flatten(vectorArray, length);

    UniformBinder binder(*this, name);
    if (binder.location != -1)
        glCheck(glUniform3fv(binder.location, static_cast<GLsizei>(length), &contiguous[0]));
}


////////////////////////////////////////////////////////////
void Shader::setUniformArray(const char* name, const Glsl::Vec4* vectorArray, std::size_t length)
{
    std::vector<float> contiguous = flatten(vectorArray, length);

    UniformBinder binder(*this, name);
    if (binder.location != -1)
        glCheck(glUniform4fv(binder.location, static_cast<GLsizei>(length), &contiguous[0]));
}


////////////////////////////////////////////////////////////
void Shader::setUniformArray(const char* name, const Glsl::Mat3* matrixArray, std::size_t length)
{
    const std::size_t matrixSize = 3 * 3;

    std::vector<float> contiguous(matrixSize * length);
    for (std::size_t i = 0; i < length; ++i)
        priv::copyMatrix(matrixArray[i].array, matrixSize, &contiguous[matrixSize * i]);

    UniformBinder binder(*this, name);
    if (binder.location != -1)
        glCheck(glUniformMatrix3fv(binder.location, static_cast<GLsizei>(length), GL_FALSE, &contiguous[0]));
}


////////////////////////////////////////////////////////////
void Shader::setUniformArray(const char* name, const Glsl::Mat4* matrixArray, std::size_t length)
{
    const std::size_t matrixSize = 4 * 4;

    std::vector<float> contiguous(matrixSize * length);
    for (std::size_t i = 0; i < length; ++i)
        priv::copyMatrix(matrixArray[i].array, matrixSize, &contiguous[matrixSize * i]);

    UniformBinder binder(*this, name);
    if (binder.location != -1)
        glCheck(glUniformMatrix4fv(binder.location, static_cast<GLsizei>(length), GL_FALSE, &contiguous[0]));
}


////////////////////////////////////////////////////////////
void Shader::setParameter(const char* name, float x)
{
    setUniform(name, x);
}


////////////////////////////////////////////////////////////
void Shader::setParameter(const char* name, float x, float y)
{
    setUniform(name, Glsl::Vec2(x, y));
}


////////////////////////////////////////////////////////////
void Shader::setParameter(const char* name, float x, float y, float z)
{
    setUniform(name, Glsl::Vec3(x, y, z));
}


////////////////////////////////////////////////////////////
void Shader::setParameter(const char* name, float x, float y, float z, float w)
{
    setUniform(name, Glsl::Vec4(x, y, z, w));
}


////////////////////////////////////////////////////////////
void Shader::setParameter(const char* name, const Vector2f& v)
{
    setUniform(name, v);
}


////////////////////////////////////////////////////////////
void Shader::setParameter(const char* name, const Vector3f& v)
{
    setUniform(name, v);
}


////////////////////////////////////////////////////////////
void Shader::setParameter(const char* name, const Color& color)
{
    setUniform(name, Glsl::Vec4(color));
}


////////////////////////////////////////////////////////////
void Shader::setParameter(const char* name, const Transform& transform)
{
    setUniform(name, Glsl::Mat4(transform));
}


////////////////////////////////////////////////////////////
void Shader::setParameter(const char* name, const Texture& texture)
{
    setUniform(name, texture);
}


////////////////////////////////////////////////////////////
void Shader::setParameter(const char* name, CurrentTextureType)
{
    setUniform(name, CurrentTexture);
}


////////////////////////////////////////////////////////////
unsigned int Shader::getNativeHandle() const
{
    return m_shaderProgram;
}


////////////////////////////////////////////////////////////
void Shader::setAttributes(const std::vector<std::string>& attributes)
{
    m_attributes = attributes;
};


////////////////////////////////////////////////////////////
void Shader::bind(const Shader* shader)
{
    if (shader && shader->m_shaderProgram)
    {
        // Enable the program
        glCheck(glUseProgram(castToGlHandle(shader->m_shaderProgram)));

        // Bind the textures
        shader->bindTextures();

        // Bind the current texture
        if (shader->m_currentTexture != -1)
            glCheck(glUniform1i(shader->m_currentTexture, 0));
    }
    else
    {
        // Bind no shader
        glCheck(glUseProgram(0));
    }
}


////////////////////////////////////////////////////////////
bool Shader::isGeometryAvailable()
{
    Lock lock(isAvailableMutex);

    static bool checked = false;
    static bool available = false;

    if (!checked)
    {
        checked = true;

        // Make sure that extensions are initialized
        sf::priv::ensureExtensionsInit();

		available = (GLAD_GL_ARB_geometry_shader4 > 0);
    }

    return available;
}


////////////////////////////////////////////////////////////
bool Shader::compile(const char* vertexShaderCode, const char* geometryShaderCode, const char* fragmentShaderCode)
{
    // Make sure we can use geometry shaders
    if (geometryShaderCode && !isGeometryAvailable())
    {
        err() << "Failed to create a shader: your system doesn't support geometry shaders "
              << "(you should test Shader::isGeometryAvailable() before trying to use geometry shaders)" << std::endl;
        return false;
    }

    // Destroy the shader if it was already created
    if (m_shaderProgram)
    {
        glCheck(glDeleteProgram(castToGlHandle(m_shaderProgram)));
        m_shaderProgram = 0;
    }

    // Reset the internal state
    m_currentTexture = -1;
    m_textures.clear();
    m_uniforms.clear();

    // Create the program
    unsigned int shaderProgram;
    glCheck(shaderProgram = glCreateProgram());

    // Create the vertex shader if needed
    if (vertexShaderCode)
    {
        // Create and compile the shader
        unsigned int vertexShader;
        glCheck(vertexShader = glCreateShader(GL_VERTEX_SHADER));
        glCheck(glShaderSource(vertexShader, 1, &vertexShaderCode, NULL));
        glCheck(glCompileShader(vertexShader));

        // Check the compile log
        GLint success;
        glCheck(glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success));
        if (success == GL_FALSE)
        {
            char log[1024];
            glGetShaderInfoLog(vertexShader, sizeof(log), NULL, log);
            err() << "Failed to compile vertex shader:" << std::endl
                  << log << std::endl;
            glCheck(glDeleteShader(vertexShader));
            glCheck(glDeleteProgram(shaderProgram));
            return false;
        }

        // Attach the shader to the program, and delete it (not needed anymore)
        glCheck(glAttachShader(shaderProgram, vertexShader));
        glCheck(glDeleteShader(vertexShader));
    }

    // Create the geometry shader if needed
    if (geometryShaderCode)
    {
        // Create and compile the shader
        unsigned int geometryShader = glCreateShader(GL_GEOMETRY_SHADER);
        glCheck(glShaderSource(geometryShader, 1, &geometryShaderCode, NULL));
        glCheck(glCompileShader(geometryShader));

        // Check the compile log
        GLint success;
        glCheck(glGetShaderiv(geometryShader, GL_COMPILE_STATUS, &success));
        if (success == GL_FALSE)
        {
            char log[1024];
			log[0] = '\0';

            glGetShaderInfoLog(geometryShader, sizeof(log), NULL, log);
            err() << "Failed to compile geometry shader:" << std::endl
                  << log << std::endl;
            glCheck(glDeleteShader(geometryShader));
            glCheck(glDeleteProgram(shaderProgram));
            return false;
        }

        // Attach the shader to the program, and delete it (not needed anymore)
        glCheck(glAttachShader(shaderProgram, geometryShader));
        glCheck(glDeleteShader(geometryShader));
    }

    // Create the fragment shader if needed
    if (fragmentShaderCode)
    {
        // Create and compile the shader
        unsigned int fragmentShader;
        glCheck(fragmentShader = glCreateShader(GL_FRAGMENT_SHADER));
        glCheck(glShaderSource(fragmentShader, 1, &fragmentShaderCode, NULL));
        glCheck(glCompileShader(fragmentShader));

        // Check the compile log
        GLint success;
        glCheck(glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success));
        if (success == GL_FALSE)
        {
            char log[1024];
			log[0] = '\0';

            glGetShaderInfoLog(fragmentShader, sizeof(log), NULL, log);
            err() << "Failed to compile fragment shader:" << std::endl
                  << log << std::endl;
            glCheck(glDeleteShader(fragmentShader));
            glCheck(glDeleteProgram(shaderProgram));
            return false;
        }

        // Attach the shader to the program, and delete it (not needed anymore)
        glCheck(glAttachShader(shaderProgram, fragmentShader));
        glCheck(glDeleteShader(fragmentShader));
    }

    // bind attributes
	GLuint loc = 0;
    for (auto& attribute : m_attributes)
        glBindAttribLocation(shaderProgram, loc++, attribute.c_str());

    // Link the program
    glCheck(glLinkProgram(shaderProgram));

    // Check the link log
    GLint success;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (success == GL_FALSE)
    {
        char log[1024];
        glGetProgramInfoLog(shaderProgram, sizeof(log), NULL, log);
        err() << "Failed to link shader:" << std::endl
              << log << std::endl;
        glCheck(glDeleteProgram(shaderProgram));
        return false;
    }

    m_shaderProgram = castFromGlHandle(shaderProgram);

    // Force an OpenGL flush, so that the shader will appear updated
    // in all contexts immediately (solves problems in multi-threaded apps)
    glCheck(glFlush());

    return true;
}


////////////////////////////////////////////////////////////
void Shader::bindTextures() const
{
    TextureTable::const_iterator it = m_textures.begin();
    for (std::size_t i = 0; i < m_textures.size(); ++i)
    {
        GLint index = static_cast<GLsizei>(i + 1);
        glCheck(glUniform1i(it->first, index));
        glCheck(glActiveTexture(GL_TEXTURE0 + index));
        Texture::bind(it->second);
        ++it;
    }

    // Make sure that the texture unit which is left active is the number 0
    glCheck(glActiveTexture(GL_TEXTURE0));
}


////////////////////////////////////////////////////////////
int Shader::getUniformLocation(const char* name)
{
    // Check the cache
    UniformTable::const_iterator it = m_uniforms.find(name);
    if (it != m_uniforms.end())
    {
        // Already in cache, return it
        return it->second;
    }
    else
    {
        // Not in cache, request the location from OpenGL
        int location = glGetUniformLocation(castToGlHandle(m_shaderProgram), name);
        m_uniforms.insert(std::make_pair(name, location));

        if (location == -1)
            err() << "Uniform \"" << name << "\" not found in shader" << std::endl;

        return location;
    }
}

} // namespace sf


