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
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/GLCheck.hpp>
#include <SFML/Graphics/TextureSaver.hpp>
#include <SFML/System/Mutex.hpp>
#include <SFML/System/Lock.hpp>
#include <SFML/System/Err.hpp>
#include <cassert>
#include <cstring>


namespace
{
    sf::Mutex idMutex;
    sf::Mutex maximumSizeMutex;

    // Thread-safe unique identifier generator,
    // is used for states cache (see RenderTarget)
    sf::Uint64 getUniqueId()
    {
        sf::Lock lock(idMutex);

        static sf::Uint64 id = 1; // start at 1, zero is "no texture"

        return id++;
    }
}


namespace sf
{
////////////////////////////////////////////////////////////
Texture::Texture() :
m_size         (0, 0),
m_actualSize   (0, 0),
m_texture      (0),
m_isSmooth     (false),
m_sRgb         (false),
m_isRepeated   (false),
m_pixelsFlipped(false),
m_fboAttachment(false),
m_hasMipmap    (false),
m_cacheId      (getUniqueId())
{
    
}


////////////////////////////////////////////////////////////
Texture::Texture(const Texture& copy) :
m_size         (0, 0),
m_actualSize   (0, 0),
m_texture      (0),
m_isSmooth     (copy.m_isSmooth),
m_sRgb         (copy.m_sRgb),
m_isRepeated   (copy.m_isRepeated),
m_pixelsFlipped(false),
m_fboAttachment(false),
m_hasMipmap    (false),
m_cacheId      (getUniqueId())
{
    if (copy.m_texture)
    {
        if (create(copy.getSize().x, copy.getSize().y))
        {
            update(copy);

            // Force an OpenGL flush, so that the texture will appear updated
            // in all contexts immediately (solves problems in multi-threaded apps)
            glCheck(glFlush());
        }
        else
        {
            err() << "Failed to copy texture, failed to create new texture" << std::endl;
        }
    }
}


////////////////////////////////////////////////////////////
Texture::~Texture()
{
    // Destroy the OpenGL texture
    if (m_texture)
    {
        GLuint texture = static_cast<GLuint>(m_texture);
        glCheck(glDeleteTextures(1, &texture));
    }
}


////////////////////////////////////////////////////////////
bool Texture::create(unsigned int width, unsigned int height)
{
    // Check if texture parameters are valid before creating it
    if ((width == 0) || (height == 0))
    {
        err() << "Failed to create texture, invalid size (" << width << "x" << height << ")" << std::endl;
        return false;
    }

    // Make sure that extensions are initialized
    priv::ensureExtensionsInit();

    // Compute the internal texture dimensions depending on NPOT textures support
	printf("size before: %d %d\n", width, height);
    Vector2u actualSize(getValidSize(width), getValidSize(height));
	printf("size after: %d %d\n", actualSize.x, actualSize.y);

    // Check the maximum texture size
    unsigned int maxSize = getMaximumSize();
    if ((actualSize.x > maxSize) || (actualSize.y > maxSize))
    {
        err() << "Failed to create texture, its internal size is too high "
              << "(" << actualSize.x << "x" << actualSize.y << ", "
              << "maximum is " << maxSize << "x" << maxSize << ")"
              << std::endl;
        return false;
    }

    // All the validity checks passed, we can store the new texture settings
    m_size.x        = width;
    m_size.y        = height;
    m_actualSize    = actualSize;
    m_pixelsFlipped = false;
    m_fboAttachment = false;

    // Create the OpenGL texture if it doesn't exist yet
    if (!m_texture)
    {
        GLuint texture;
        glCheck(glGenTextures(1, &texture));
        m_texture = static_cast<unsigned int>(texture);
    }

    // Make sure that the current texture binding will be preserved
    priv::TextureSaver save;

    static bool textureEdgeClamp = true;

    if (!m_isRepeated && !textureEdgeClamp)
    {
        static bool warned = false;

        if (!warned)
        {
            err() << "OpenGL extension SGIS_texture_edge_clamp unavailable" << std::endl;
            err() << "Artifacts may occur along texture edges" << std::endl;
            err() << "Ensure that hardware acceleration is enabled if available" << std::endl;

            warned = true;
        }
    }

	static bool textureSrgb = (GLAD_GL_EXT_texture_sRGB > 0);

    if (m_sRgb && !textureSrgb)
    {
        static bool warned = false;

        if (!warned)
        {
#ifndef SFML_OPENGL_ES
            err() << "OpenGL extension EXT_texture_sRGB unavailable" << std::endl;
#else
            err() << "OpenGL ES extension EXT_sRGB unavailable" << std::endl;
#endif
            err() << "Automatic sRGB to linear conversion disabled" << std::endl;

            warned = true;
        }

        m_sRgb = false;
    }

    // Initialize the texture
    glCheck(glBindTexture(GL_TEXTURE_2D, m_texture));
    glCheck(glTexImage2D(GL_TEXTURE_2D, 0, (m_sRgb ? GL_SRGB8_ALPHA8 : GL_RGBA), m_actualSize.x, m_actualSize.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL));
	glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, m_isRepeated ? GL_REPEAT : (textureEdgeClamp ? GL_CLAMP_TO_EDGE : GL_CLAMP)));
    glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, m_isRepeated ? GL_REPEAT : (textureEdgeClamp ? GL_CLAMP_TO_EDGE : GL_CLAMP)));
    glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, m_isSmooth ? GL_LINEAR : GL_NEAREST));
    glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, m_isSmooth ? GL_LINEAR : GL_NEAREST));
	glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0));
    m_cacheId = getUniqueId();

    m_hasMipmap = false;

    return true;
}


////////////////////////////////////////////////////////////
bool Texture::loadFromFile(const std::string& filename, const IntRect& area)
{
    Image image;
    return image.loadFromFile(filename) && loadFromImage(image, area);
}


////////////////////////////////////////////////////////////
bool Texture::loadFromMemory(const void* data, std::size_t size, const IntRect& area)
{
    Image image;
    return image.loadFromMemory(data, size) && loadFromImage(image, area);
}


////////////////////////////////////////////////////////////
bool Texture::loadFromStream(InputStream& stream, const IntRect& area)
{
    Image image;
    return image.loadFromStream(stream) && loadFromImage(image, area);
}


////////////////////////////////////////////////////////////
bool Texture::loadFromImage(const Image& image, const IntRect& area)
{
    // Retrieve the image size
    int width = static_cast<int>(image.getSize().x);
    int height = static_cast<int>(image.getSize().y);

    // Load the entire image if the source area is either empty or contains the whole image
    if (area.width == 0 || (area.height == 0) ||
       ((area.left <= 0) && (area.top <= 0) && (area.width >= width) && (area.height >= height)))
    {
        // Load the entire image
        if (create(image.getSize().x, image.getSize().y))
        {
            update(image);

            return true;
        }
        else
        {
            return false;
        }
    }
    else
    {
        // Load a sub-area of the image

        // Adjust the rectangle to the size of the image
        IntRect rectangle = area;
        if (rectangle.left   < 0) rectangle.left = 0;
        if (rectangle.top    < 0) rectangle.top  = 0;
        if (rectangle.left + rectangle.width > width)  rectangle.width  = width - rectangle.left;
        if (rectangle.top + rectangle.height > height) rectangle.height = height - rectangle.top;

        // Create the texture and upload the pixels
        if (create(rectangle.width, rectangle.height))
        {
            // Make sure that the current texture binding will be preserved
            priv::TextureSaver save;

            // Copy the pixels to the texture, row by row
            const Uint8* pixels = image.getPixelsPtr() + 4 * (rectangle.left + (width * rectangle.top));
            glCheck(glBindTexture(GL_TEXTURE_2D, m_texture));
            for (int i = 0; i < rectangle.height; ++i)
            {
                glCheck(glTexSubImage2D(GL_TEXTURE_2D, 0, 0, i, rectangle.width, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixels));
                pixels += 4 * width;
            }

            glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, m_isSmooth ? GL_LINEAR : GL_NEAREST));
            m_hasMipmap = false;

            // Force an OpenGL flush, so that the texture will appear updated
            // in all contexts immediately (solves problems in multi-threaded apps)
            glCheck(glFlush());

            return true;
        }
        else
        {
            return false;
        }
    }
}


////////////////////////////////////////////////////////////
Vector2u Texture::getSize() const
{
    return m_size;
}


////////////////////////////////////////////////////////////
Image Texture::copyToImage() const
{
    // Easy case: empty texture
    if (!m_texture)
        return Image();

    // Make sure that the current texture binding will be preserved
    priv::TextureSaver save;

    // Create an array of pixels
    std::vector<Uint8> pixels(m_size.x * m_size.y * 4);

#if defined (SFML_OPENGL_ES) || defined(SFML_SYSTEM_EMSCRIPTEN)

    // OpenGL ES doesn't have the glGetTexImage function, the only way to read
    // from a texture is to bind it to a FBO and use glReadPixels
    GLuint frameBuffer = 0;
    glCheck(glGenFramebuffers(1, &frameBuffer));
    if (frameBuffer)
    {
        GLint previousFrameBuffer;
        glCheck(glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFrameBuffer));

        glCheck(glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer));
        glCheck(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texture, 0));
        glCheck(glReadPixels(0, 0, m_size.x, m_size.y, GL_RGBA, GL_UNSIGNED_BYTE, &pixels[0]));
        glCheck(glDeleteFramebuffers(1, &frameBuffer));

        glCheck(glBindFramebuffer(GL_FRAMEBUFFER, previousFrameBuffer));
    }

#else

    if ((m_size == m_actualSize) && !m_pixelsFlipped)
    {
        // Texture is not padded nor flipped, we can use a direct copy
        glCheck(glBindTexture(GL_TEXTURE_2D, m_texture));
        glCheck(glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, &pixels[0]));
    }
    else
    {
        // Texture is either padded or flipped, we have to use a slower algorithm

        // All the pixels will first be copied to a temporary array
        std::vector<Uint8> allPixels(m_actualSize.x * m_actualSize.y * 4);
        glCheck(glBindTexture(GL_TEXTURE_2D, m_texture));
        glCheck(glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, &allPixels[0]));

        // Then we copy the useful pixels from the temporary array to the final one
        const Uint8* src = &allPixels[0];
        Uint8* dst = &pixels[0];
        int srcPitch = m_actualSize.x * 4;
        int dstPitch = m_size.x * 4;

        // Handle the case where source pixels are flipped vertically
        if (m_pixelsFlipped)
        {
            src += srcPitch * (m_size.y - 1);
            srcPitch = -srcPitch;
        }

        for (unsigned int i = 0; i < m_size.y; ++i)
        {
            std::memcpy(dst, src, dstPitch);
            src += srcPitch;
            dst += dstPitch;
        }
    }

#endif // SFML_OPENGL_ES

    // Create the image
    Image image;
    image.create(m_size.x, m_size.y, &pixels[0]);

    return image;
}


////////////////////////////////////////////////////////////
void Texture::update(const Uint8* pixels)
{
    // Update the whole texture
    update(pixels, m_size.x, m_size.y, 0, 0);
}


////////////////////////////////////////////////////////////
void Texture::update(const Uint8* pixels, unsigned int width, unsigned int height, unsigned int x, unsigned int y)
{
#if defined(SFML_DEBUG)
    assert(x + width <= m_size.x);
    assert(y + height <= m_size.y);
#endif
    
    if (pixels && m_texture)
    {
        // Make sure that the current texture binding will be preserved
        priv::TextureSaver save;

        // Copy pixels from the given array to the texture
        glCheck(glBindTexture(GL_TEXTURE_2D, m_texture));
        glCheck(glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels));
        glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, m_isSmooth ? GL_LINEAR : GL_NEAREST));
        m_hasMipmap = false;
        m_pixelsFlipped = false;
        m_cacheId = getUniqueId();

        // Force an OpenGL flush, so that the texture data will appear updated
        // in all contexts immediately (solves problems in multi-threaded apps)
        glCheck(glFlush());
    }
}


////////////////////////////////////////////////////////////
void Texture::update(const Texture& texture)
{
    // Update the whole texture
    update(texture, 0, 0);
}


////////////////////////////////////////////////////////////
void Texture::update(const Texture& texture, unsigned int x, unsigned int y)
{
#if defined(SFML_DEBUG)
    assert(x + texture.m_size.x <= m_size.x);
    assert(y + texture.m_size.y <= m_size.y);
#endif
    
    if (!m_texture || !texture.m_texture)
        return;

#ifndef SFML_OPENGL_ES

    {
        // Make sure that extensions are initialized
        priv::ensureExtensionsInit();
    }

    if (GLAD_GL_EXT_framebuffer_object && GLAD_GL_EXT_framebuffer_blit)
    {
        // Save the current bindings so we can restore them after we are done
        GLint readFramebuffer = 0;
        GLint drawFramebuffer = 0;

        glCheck(glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFramebuffer));
        glCheck(glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFramebuffer));

        // Create the framebuffers
        GLuint sourceFrameBuffer = 0;
        GLuint destFrameBuffer = 0;
        glCheck(glGenFramebuffers(1, &sourceFrameBuffer));
        glCheck(glGenFramebuffers(1, &destFrameBuffer));

        if (!sourceFrameBuffer || !destFrameBuffer)
        {
            err() << "Cannot copy texture, failed to create a frame buffer object" << std::endl;
            return;
        }

        // Link the source texture to the source frame buffer
        glCheck(glBindFramebuffer(GL_READ_FRAMEBUFFER, sourceFrameBuffer));
        glCheck(glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture.m_texture, 0));

        // Link the destination texture to the destination frame buffer
        glCheck(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, destFrameBuffer));
        glCheck(glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texture, 0));

        // A final check, just to be sure...
        GLenum sourceStatus;
        glCheck(sourceStatus = glCheckFramebufferStatus(GL_READ_FRAMEBUFFER));

        GLenum destStatus;
        glCheck(destStatus = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER));

        if ((sourceStatus == GL_FRAMEBUFFER_COMPLETE) && (destStatus == GL_FRAMEBUFFER_COMPLETE))
        {
            // Blit the texture contents from the source to the destination texture
            glCheck(glBlitFramebuffer(
                0, texture.m_pixelsFlipped ? texture.m_size.y : 0, texture.m_size.x, texture.m_pixelsFlipped ? 0 : texture.m_size.y, // Source rectangle, flip y if source is flipped
                x, y, x + texture.m_size.x, y + texture.m_size.y, // Destination rectangle
                GL_COLOR_BUFFER_BIT, GL_NEAREST
            ));
        }
        else
        {
            err() << "Cannot copy texture, failed to link texture to frame buffer" << std::endl;
        }

        // Restore previously bound framebuffers
        glCheck(glBindFramebuffer(GL_READ_FRAMEBUFFER, readFramebuffer));
        glCheck(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFramebuffer));

        // Delete the framebuffers
        glCheck(glDeleteFramebuffers(1, &sourceFrameBuffer));
        glCheck(glDeleteFramebuffers(1, &destFrameBuffer));

        // Make sure that the current texture binding will be preserved
        priv::TextureSaver save;

        // Set the parameters of this texture
        glCheck(glBindTexture(GL_TEXTURE_2D, m_texture));
        glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, m_isSmooth ? GL_LINEAR : GL_NEAREST));
        m_hasMipmap = false;
        m_pixelsFlipped = false;
        m_cacheId = getUniqueId();

        // Force an OpenGL flush, so that the texture data will appear updated
        // in all contexts immediately (solves problems in multi-threaded apps)
        glCheck(glFlush());

        return;
    }

#endif // SFML_OPENGL_ES

    update(texture.copyToImage(), x, y);
}


////////////////////////////////////////////////////////////
void Texture::update(const Image& image)
{
    // Update the whole texture
    update(image.getPixelsPtr(), image.getSize().x, image.getSize().y, 0, 0);
}


////////////////////////////////////////////////////////////
void Texture::update(const Image& image, unsigned int x, unsigned int y)
{
    update(image.getPixelsPtr(), image.getSize().x, image.getSize().y, x, y);
}


////////////////////////////////////////////////////////////
void Texture::setSmooth(bool smooth)
{
    if (smooth != m_isSmooth)
    {
        m_isSmooth = smooth;

        if (m_texture)
        {
            // Make sure that the current texture binding will be preserved
            priv::TextureSaver save;

            glCheck(glBindTexture(GL_TEXTURE_2D, m_texture));
            glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, m_isSmooth ? GL_LINEAR : GL_NEAREST));

            if (m_hasMipmap)
            {
                glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, m_isSmooth ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_LINEAR));
            }
            else
            {
                glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, m_isSmooth ? GL_LINEAR : GL_NEAREST));
            }
        }
    }
}


////////////////////////////////////////////////////////////
bool Texture::isSmooth() const
{
    return m_isSmooth;
}


////////////////////////////////////////////////////////////
void Texture::setSrgb(bool sRgb)
{
    m_sRgb = sRgb;
}


////////////////////////////////////////////////////////////
bool Texture::isSrgb() const
{
    return m_sRgb;
}


////////////////////////////////////////////////////////////
void Texture::setRepeated(bool repeated)
{
    if (repeated != m_isRepeated)
    {
        m_isRepeated = repeated;

        if (m_texture)
        {
            // Make sure that the current texture binding will be preserved
            priv::TextureSaver save;

            static bool textureEdgeClamp = true;

            if (!m_isRepeated && !textureEdgeClamp)
            {
                static bool warned = false;

                if (!warned)
                {
                    err() << "OpenGL extension SGIS_texture_edge_clamp unavailable" << std::endl;
                    err() << "Artifacts may occur along texture edges" << std::endl;
                    err() << "Ensure that hardware acceleration is enabled if available" << std::endl;

                    warned = true;
                }
            }

            glCheck(glEnable(GL_TEXTURE_2D));
            glCheck(glBindTexture(GL_TEXTURE_2D, m_texture));
            glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, m_isRepeated ? GL_REPEAT : (textureEdgeClamp ? GL_CLAMP_TO_EDGE : GL_CLAMP)));
            glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, m_isRepeated ? GL_REPEAT : (textureEdgeClamp ? GL_CLAMP_TO_EDGE : GL_CLAMP)));
            glCheck(glDisable(GL_TEXTURE_2D));
        }
    }
}


////////////////////////////////////////////////////////////
bool Texture::isRepeated() const
{
    return m_isRepeated;
}


////////////////////////////////////////////////////////////
bool Texture::isFlipped() const
{
    return m_pixelsFlipped;
};


////////////////////////////////////////////////////////////
bool Texture::isAttachedToFBO() const
{
    return m_fboAttachment;
};


////////////////////////////////////////////////////////////
bool Texture::generateMipmap()
{
    if (!m_texture)
        return false;

    // Make sure that extensions are initialized
    priv::ensureExtensionsInit();

    if (!GLAD_GL_EXT_framebuffer_object)
        return false;

    // Make sure that the current texture binding will be preserved
    priv::TextureSaver save;

    glCheck(glBindTexture(GL_TEXTURE_2D, m_texture));
    glCheck(glGenerateMipmap(GL_TEXTURE_2D));
    glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, m_isSmooth ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_LINEAR));

    m_hasMipmap = true;

    return true;
}


////////////////////////////////////////////////////////////
void Texture::invalidateMipmap()
{
    if (!m_hasMipmap)
        return;

    // Make sure that the current texture binding will be preserved
    priv::TextureSaver save;

    glCheck(glBindTexture(GL_TEXTURE_2D, m_texture));
    glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, m_isSmooth ? GL_LINEAR : GL_NEAREST));

    m_hasMipmap = false;
}


////////////////////////////////////////////////////////////
void Texture::bind(const Texture* texture, CoordinateType coordinateType)
{
    if (texture && texture->m_texture)
    {
        // Bind the texture
        glCheck(glBindTexture(GL_TEXTURE_2D, texture->m_texture));
    }
    else
    {
        // Bind no texture
        glCheck(glBindTexture(GL_TEXTURE_2D, 0));
    }
}


////////////////////////////////////////////////////////////
unsigned int Texture::getMaximumSize()
{
    Lock lock(maximumSizeMutex);

    static bool checked = false;
    static GLint size = 0;

    if (!checked)
    {
        checked = true;

        glCheck(glGetIntegerv(GL_MAX_TEXTURE_SIZE, &size));
    }

    return static_cast<unsigned int>(size);
}


////////////////////////////////////////////////////////////
Texture& Texture::operator =(const Texture& right)
{
    Texture temp(right);

    swap(temp);

    return *this;
}


////////////////////////////////////////////////////////////
void Texture::swap(Texture& right)
{
    std::swap(m_size,          right.m_size);
    std::swap(m_actualSize,    right.m_actualSize);
    std::swap(m_texture,       right.m_texture);
    std::swap(m_isSmooth,      right.m_isSmooth);
    std::swap(m_sRgb,          right.m_sRgb);
    std::swap(m_isRepeated,    right.m_isRepeated);
    std::swap(m_pixelsFlipped, right.m_pixelsFlipped);
    std::swap(m_fboAttachment, right.m_fboAttachment);
    std::swap(m_hasMipmap,     right.m_hasMipmap);

    m_cacheId = getUniqueId();
    right.m_cacheId = getUniqueId();
}


////////////////////////////////////////////////////////////
unsigned int Texture::getNativeHandle() const
{
    return m_texture;
}


////////////////////////////////////////////////////////////
unsigned int Texture::getValidSize(unsigned int size)
{
    bool bEnable = false;

#if defined(SFML_SYSTEM_ANDROID)
    bEnable = (GLAD_GL_OES_texture_npot > 0);
#elif defined(SFML_SYSTEM_EMSCRIPTEN)
    bEnable = true;
#elif defined(SFML_SYSTEM_WINDOWS)
    bEnable = (GLAD_GL_ARB_texture_non_power_of_two > 0);
#elif 
#error Unknown platform
#endif

    if (bEnable)
    {
        // If hardware supports NPOT textures, then just return the unmodified size
        return size;
    }
    else
    {
        // If hardware doesn't support NPOT textures, we calculate the nearest power of two
        unsigned int powerOfTwo = 1;
        while (powerOfTwo < size)
            powerOfTwo *= 2;

        return powerOfTwo;
    }
}

} // namespace sf