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
#include <SFML/Graphics/VertexBuffer.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Vertex.hpp>
#include <SFML/Graphics/GLCheck.hpp>
#include <SFML/System/Mutex.hpp>
#include <SFML/System/Lock.hpp>
#include <SFML/System/Err.hpp>
#include <cstring>

namespace
{
    sf::Mutex isAvailableMutex;

    GLenum usageToGlEnum(sf::VertexBuffer::Usage usage)
    {
        switch (usage)
        {
            case sf::VertexBuffer::Static:  return GL_STATIC_DRAW;
            case sf::VertexBuffer::Dynamic: return GL_DYNAMIC_DRAW;
            default:                        return GL_STREAM_DRAW;
        }
    }
}


namespace sf
{
////////////////////////////////////////////////////////////
VertexBuffer::VertexBuffer() :
m_vao          (0),
m_vbo          (0),
m_size         (0),
m_primitiveType(Points),
m_usage        (Stream)
{
}


////////////////////////////////////////////////////////////
VertexBuffer::VertexBuffer(PrimitiveType type) :
m_vao          (0),
m_vbo          (0),
m_size         (0),
m_primitiveType(type),
m_usage        (Stream)
{
}


////////////////////////////////////////////////////////////
VertexBuffer::VertexBuffer(VertexBuffer::Usage usage) :
m_vao          (0),
m_vbo          (0),
m_size         (0),
m_primitiveType(Points),
m_usage        (usage)
{
}


////////////////////////////////////////////////////////////
VertexBuffer::VertexBuffer(PrimitiveType type, VertexBuffer::Usage usage) :
m_vao          (0),
m_vbo          (0),
m_size         (0),
m_primitiveType(type),
m_usage        (usage)
{
}


////////////////////////////////////////////////////////////
VertexBuffer::VertexBuffer(const VertexBuffer& copy) :
m_vao          (0),
m_vbo          (0),
m_size         (0),
m_primitiveType(copy.m_primitiveType),
m_usage        (copy.m_usage)
{
    if (copy.m_vbo && copy.m_size)
    {
        if (!create(copy.m_size))
        {
            err() << "Could not create vertex buffer for copying" << std::endl;
            return;
        }

        if (!update(copy))
            err() << "Could not copy vertex buffer" << std::endl;
    }
}


////////////////////////////////////////////////////////////
VertexBuffer::~VertexBuffer()
{
    if (m_vbo)
    {
        glCheck(glDeleteBuffers(1, &m_vbo));
    }
}


////////////////////////////////////////////////////////////
bool VertexBuffer::create(std::size_t vertexCount)
{
    if (!m_vao)
        glCheck(SFML_GL_EXT_glGenVertexArrays(1, &m_vao));

    if (!m_vao)
    {
        err() << "Could not create vertex array, generation failed" << std::endl;
        return false;
    }

    if (!m_vbo)
        glCheck(glGenBuffers(1, &m_vbo));

    if (!m_vbo)
    {
        err() << "Could not create vertex buffer, generation failed" << std::endl;
        return false;
    }

    glCheck(SFML_GL_EXT_glBindVertexArray(m_vao));

    glCheck(glBindBuffer(GL_ARRAY_BUFFER, m_vbo));
    glCheck(glBufferData(GL_ARRAY_BUFFER, sizeof(sf::Vertex) * vertexCount, 0, usageToGlEnum(m_usage)));

    glCheck(glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(sf::Vertex), (void*)0));
    glCheck(glEnableVertexAttribArray(0));

    glCheck(glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(sf::Vertex), (void*)sizeof(sf::Vertex::position)));
    glCheck(glEnableVertexAttribArray(1));

    glCheck(glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(sf::Vertex), (void*)(sizeof(sf::Vertex::position) + sizeof(sf::Vertex::color))));
    glCheck(glEnableVertexAttribArray(2));

    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));
    glCheck(SFML_GL_EXT_glBindVertexArray(0));

    m_size = vertexCount;

    return true;
}


////////////////////////////////////////////////////////////
std::size_t VertexBuffer::getVertexCount() const
{
    return m_size;
}


////////////////////////////////////////////////////////////
bool VertexBuffer::update(const Vertex* vertices)
{
    return update(vertices, m_size, 0);
}


////////////////////////////////////////////////////////////
bool VertexBuffer::update(const Vertex* vertices, std::size_t vertexCount, unsigned int offset)
{
    // Sanity checks
    if (!m_vbo)
        return false;

    if (!vertices)
        return false;

    if (offset && (offset + vertexCount > m_size))
        return false;

	glCheck(SFML_GL_EXT_glBindVertexArray(m_vao));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, m_vbo));

    // Check if we need to resize or orphan the buffer
    if (vertexCount >= m_size)
    {
        glCheck(glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * vertexCount, 0, usageToGlEnum(m_usage)));

        m_size = vertexCount;
    }

    glCheck(glBufferSubData(GL_ARRAY_BUFFER, sizeof(Vertex) * offset, sizeof(Vertex) * vertexCount, vertices));

    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));
	glCheck(SFML_GL_EXT_glBindVertexArray(0));

    return true;
}


////////////////////////////////////////////////////////////
bool VertexBuffer::update(const VertexBuffer& vertexBuffer)
{
#ifdef SFML_OPENGL_ES

    return false;

#else

    if (!m_vbo || !vertexBuffer.m_vbo)
        return false;

    // Make sure that extensions are initialized
    sf::priv::ensureExtensionsInit();

    if (GLAD_GL_ARB_copy_buffer)
    {
        glCheck(glBindBuffer(GL_COPY_READ_BUFFER, vertexBuffer.m_vbo));
        glCheck(glBindBuffer(GL_COPY_WRITE_BUFFER, m_vbo));

        glCheck(glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, sizeof(Vertex) * vertexBuffer.m_size));

        glCheck(glBindBuffer(GL_COPY_WRITE_BUFFER, 0));
        glCheck(glBindBuffer(GL_COPY_READ_BUFFER, 0));

        return true;
    }

    glCheck(glBindBuffer(GL_ARRAY_BUFFER, m_vbo));
    glCheck(glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * vertexBuffer.m_size, 0, usageToGlEnum(m_usage)));

    void* destination = 0;
    glCheck(destination = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY));

    glCheck(glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer.m_vbo));

    void* source = 0;
    glCheck(source = glMapBuffer(GL_ARRAY_BUFFER, GL_READ_ONLY));

    std::memcpy(destination, source, sizeof(Vertex) * vertexBuffer.m_size);

    GLboolean sourceResult = GL_FALSE;
    glCheck(sourceResult = glUnmapBuffer(GL_ARRAY_BUFFER));

    glCheck(glBindBuffer(GL_ARRAY_BUFFER, m_vbo));

    GLboolean destinationResult = GL_FALSE;
    glCheck(destinationResult = glUnmapBuffer(GL_ARRAY_BUFFER));

    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

    if ((sourceResult == GL_FALSE) || (destinationResult == GL_FALSE))
        return false;

    return true;

#endif // SFML_OPENGL_ES
}


////////////////////////////////////////////////////////////
VertexBuffer& VertexBuffer::operator =(const VertexBuffer& right)
{
    VertexBuffer temp(right);

    swap(temp);

    return *this;
}


////////////////////////////////////////////////////////////
void VertexBuffer::swap(VertexBuffer& right)
{
    std::swap(m_size,          right.m_size);
    std::swap(m_vao,           right.m_vao);
    std::swap(m_vbo,           right.m_vbo);
    std::swap(m_primitiveType, right.m_primitiveType);
    std::swap(m_usage,         right.m_usage);
}


////////////////////////////////////////////////////////////
unsigned int VertexBuffer::getNativeHandleArray() const
{
    return m_vao;
}


////////////////////////////////////////////////////////////
unsigned int VertexBuffer::getNativeHandleBuffer() const
{
    return m_vbo;
}


////////////////////////////////////////////////////////////
void VertexBuffer::bind(const VertexBuffer* vertexBuffer)
{
    glCheck(SFML_GL_EXT_glBindVertexArray(vertexBuffer ? vertexBuffer->m_vao : 0));
}


////////////////////////////////////////////////////////////
void VertexBuffer::setPrimitiveType(PrimitiveType type)
{
    m_primitiveType = type;
}


////////////////////////////////////////////////////////////
PrimitiveType VertexBuffer::getPrimitiveType() const
{
    return m_primitiveType;
}


////////////////////////////////////////////////////////////
void VertexBuffer::setUsage(VertexBuffer::Usage usage)
{
    m_usage = usage;
}


////////////////////////////////////////////////////////////
VertexBuffer::Usage VertexBuffer::getUsage() const
{
    return m_usage;
}


////////////////////////////////////////////////////////////
void VertexBuffer::draw(RenderTarget& target, RenderStates states) const
{
    if (m_vbo && m_size)
        target.draw(*this, 0, m_size, states);
}

} // namespace sf
