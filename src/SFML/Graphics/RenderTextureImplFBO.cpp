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
#include <SFML/Graphics/RenderTextureImplFBO.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/GLCheck.hpp>
#include <SFML/System/Mutex.hpp>
#include <SFML/System/Lock.hpp>
#include <SFML/System/Err.hpp>
#include <utility>
#include <set>


namespace sf
{
namespace priv
{
////////////////////////////////////////////////////////////
RenderTextureImplFBO::RenderTextureImplFBO() :
m_frameBufferId             (0),
m_multisampleFrameBufferId  (0),
m_depthStencilBuffer        (0),
m_colorBuffer               (0),
m_width                     (0),
m_height                    (0),
m_textureId                 (0),
m_multisample               (false),
m_stencil                   (false)
{
 
}


////////////////////////////////////////////////////////////
RenderTextureImplFBO::~RenderTextureImplFBO()
{
    // Destroy the color buffer
    if (m_colorBuffer)
    {
        GLuint colorBuffer = static_cast<GLuint>(m_colorBuffer);
        glCheck(glDeleteRenderbuffers(1, &colorBuffer));
    }

    // Destroy the depth/stencil buffer
    if (m_depthStencilBuffer)
    {
        GLuint depthStencilBuffer = static_cast<GLuint>(m_depthStencilBuffer);
        glCheck(glDeleteRenderbuffers(1, &depthStencilBuffer));
    }

    // Destroy the framebuffer
    if (m_frameBufferId)
    {
        GLuint frameBuffer = static_cast<GLuint>(m_frameBufferId);
        glCheck(glDeleteFramebuffers(1, &frameBuffer));
    }

    // Destroy the multisample framebuffer
    if (m_multisampleFrameBufferId)
    {
        GLuint multisampleFrameBufferId = static_cast<GLuint>(m_multisampleFrameBufferId);
        glCheck(glDeleteFramebuffers(1, &multisampleFrameBufferId));
    }
}


////////////////////////////////////////////////////////////
unsigned int RenderTextureImplFBO::getMaximumAntialiasingLevel()
{
    GLint samples = 0;

#ifndef SFML_OPENGL_ES

    glCheck(glGetIntegerv(GL_MAX_SAMPLES, &samples));

#endif

    return static_cast<unsigned int>(samples);
}


////////////////////////////////////////////////////////////
void RenderTextureImplFBO::unbind()
{
    glCheck(glBindFramebuffer(GL_FRAMEBUFFER, 0));
}


////////////////////////////////////////////////////////////
bool RenderTextureImplFBO::create(unsigned int width, unsigned int height, unsigned int textureId, const ContextSettings& settings)
{
    // Store the dimensions
    m_width = width;
    m_height = height;

    {
        // Make sure that extensions are initialized
        priv::ensureExtensionsInit();

        if (settings.antialiasingLevel && !(GLAD_GL_EXT_framebuffer_multisample && GLAD_GL_EXT_framebuffer_blit))
            return false;

        if (settings.stencilBits && !GLAD_GL_EXT_packed_depth_stencil)
            return false;

#ifndef SFML_OPENGL_ES

        // Check if the requested anti-aliasing level is supported
        if (settings.antialiasingLevel)
        {
            GLint samples = 0;
            glCheck(glGetIntegerv(GL_MAX_SAMPLES, &samples));

            if (settings.antialiasingLevel > static_cast<unsigned int>(samples))
            {
                err() << "Impossible to create render texture (unsupported anti-aliasing level)";
                err() << " Requested: " << settings.antialiasingLevel << " Maximum supported: " << samples << std::endl;
                return false;
            }
        }

#endif // SFML_OPENGL_ES


        if (!settings.antialiasingLevel)
        {
            // Create the depth/stencil buffer if requested
            if (settings.stencilBits)
            {

#ifndef SFML_OPENGL_ES

                GLuint depthStencil = 0;
                glCheck(glGenRenderbuffers(1, &depthStencil));
                m_depthStencilBuffer = static_cast<unsigned int>(depthStencil);
                if (!m_depthStencilBuffer)
                {
                    err() << "Impossible to create render texture (failed to create the attached depth/stencil buffer)" << std::endl;
                    return false;
                }
                glCheck(glBindRenderbuffer(GL_RENDERBUFFER, m_depthStencilBuffer));
                glCheck(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height));

#else // SFML_OPENGL_ES

                err() << "Impossible to create render texture (failed to create the attached depth/stencil buffer)" << std::endl;
                return false;

#endif // SFML_OPENGL_ES

                m_stencil = true;

            }
            else if (settings.depthBits)
            {
                GLuint depthStencil = 0;
                glCheck(glGenRenderbuffers(1, &depthStencil));
                m_depthStencilBuffer = static_cast<unsigned int>(depthStencil);
                if (!m_depthStencilBuffer)
                {
                    err() << "Impossible to create render texture (failed to create the attached depth buffer)" << std::endl;
                    return false;
                }
                glCheck(glBindRenderbuffer(GL_RENDERBUFFER, m_depthStencilBuffer));
                glCheck(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height));
            }
        }
        else
        {

#ifndef SFML_OPENGL_ES

            // Create the multisample color buffer
            GLuint color = 0;
            glCheck(glGenRenderbuffers(1, &color));
            m_colorBuffer = static_cast<unsigned int>(color);
            if (!m_colorBuffer)
            {
                err() << "Impossible to create render texture (failed to create the attached multisample color buffer)" << std::endl;
                return false;
            }
            glCheck(glBindRenderbuffer(GL_RENDERBUFFER, m_colorBuffer));
            glCheck(glRenderbufferStorageMultisample(GL_RENDERBUFFER, settings.antialiasingLevel, GL_RGBA, width, height));

            // Create the multisample depth/stencil buffer if requested
            if (settings.stencilBits)
            {
                GLuint depthStencil = 0;
                glCheck(glGenRenderbuffers(1, &depthStencil));
                m_depthStencilBuffer = static_cast<unsigned int>(depthStencil);
                if (!m_depthStencilBuffer)
                {
                    err() << "Impossible to create render texture (failed to create the attached multisample depth/stencil buffer)" << std::endl;
                    return false;
                }
                glCheck(glBindRenderbuffer(GL_RENDERBUFFER, m_depthStencilBuffer));
                glCheck(glRenderbufferStorageMultisample(GL_RENDERBUFFER, settings.antialiasingLevel, GL_DEPTH24_STENCIL8, width, height));

                m_stencil = true;
            }
            else if (settings.depthBits)
            {
                GLuint depthStencil = 0;
                glCheck(glGenRenderbuffers(1, &depthStencil));
                m_depthStencilBuffer = static_cast<unsigned int>(depthStencil);
                if (!m_depthStencilBuffer)
                {
                    err() << "Impossible to create render texture (failed to create the attached multisample depth buffer)" << std::endl;
                    return false;
                }
                glCheck(glBindRenderbuffer(GL_RENDERBUFFER, m_depthStencilBuffer));
                glCheck(glRenderbufferStorageMultisample(GL_RENDERBUFFER, settings.antialiasingLevel, GL_DEPTH_COMPONENT, width, height));
            }

#else // SFML_OPENGL_ES

            err() << "Impossible to create render texture (failed to create the multisample render buffers)" << std::endl;
            return false;

#endif // SFML_OPENGL_ES

            m_multisample = true;

        }
    }

    // Save our texture ID in order to be able to attach it to an FBO at any time
    m_textureId = textureId;

#ifndef SFML_OPENGL_ES

    // Save the current bindings so we can restore them after we are done
    GLint readFramebuffer = 0;
    GLint drawFramebuffer = 0;

    glCheck(glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFramebuffer));
    glCheck(glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFramebuffer));

    if (createFrameBuffer())
    {
        // Restore previously bound framebuffers
        glCheck(glBindFramebuffer(GL_READ_FRAMEBUFFER, readFramebuffer));
        glCheck(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFramebuffer));

        return true;
    }

#else

    // Save the current binding so we can restore them after we are done
    GLint frameBuffer = 0;

    glCheck(glGetIntegerv(GL_FRAMEBUFFER_BINDING, &frameBuffer));

    if (createFrameBuffer())
    {
        // Restore previously bound framebuffer
        glCheck(glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer));

        return true;
    }

#endif

    return false;
}


////////////////////////////////////////////////////////////
bool RenderTextureImplFBO::createFrameBuffer()
{
    // Create the framebuffer object
    GLuint frameBuffer = 0;
    glCheck(glGenFramebuffers(1, &frameBuffer));

    if (!frameBuffer)
    {
        err() << "Impossible to create render texture (failed to create the frame buffer object)" << std::endl;
        return false;
    }
    glCheck(glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer));
    
    // Link the depth/stencil renderbuffer to the frame buffer
    if (!m_multisample && m_depthStencilBuffer)
    {
        glCheck(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_depthStencilBuffer));

#ifndef SFML_OPENGL_ES

        if (m_stencil)
        {
            glCheck(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_depthStencilBuffer));
        }

#endif

    }

    // Link the texture to the frame buffer
    glCheck(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_textureId, 0));

    // A final check, just to be sure...
    GLenum status;
    glCheck(status = glCheckFramebufferStatus(GL_FRAMEBUFFER));
    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        glCheck(glBindFramebuffer(GL_FRAMEBUFFER, 0));
        glCheck(glDeleteFramebuffers(1, &frameBuffer));
        err() << "Impossible to create render texture (failed to link the target texture to the frame buffer)" << std::endl;
        return false;
    }

    m_frameBufferId = frameBuffer;

#ifndef SFML_OPENGL_ES

    if (m_multisample)
    {
        // Create the multisample framebuffer object
        GLuint multisampleFrameBuffer = 0;
        glCheck(glGenFramebuffers(1, &multisampleFrameBuffer));

        if (!multisampleFrameBuffer)
        {
            err() << "Impossible to create render texture (failed to create the multisample frame buffer object)" << std::endl;
            return false;
        }
        glCheck(glBindFramebuffer(GL_FRAMEBUFFER, multisampleFrameBuffer));

        // Link the multisample color buffer to the frame buffer
        glCheck(glBindRenderbuffer(GL_RENDERBUFFER, m_colorBuffer));
        glCheck(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, m_colorBuffer));

        // Link the depth/stencil renderbuffer to the frame buffer
        if (m_depthStencilBuffer)
        {
            glCheck(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_depthStencilBuffer));

            if (m_stencil)
            {
                glCheck(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_depthStencilBuffer));
            }
        }

        // A final check, just to be sure...
        glCheck(status = glCheckFramebufferStatus(GL_FRAMEBUFFER));
        if (status != GL_FRAMEBUFFER_COMPLETE)
        {
            glCheck(glBindFramebuffer(GL_FRAMEBUFFER, 0));
            glCheck(glDeleteFramebuffers(1, &multisampleFrameBuffer));
            err() << "Impossible to create render texture (failed to link the render buffers to the multisample frame buffer)" << std::endl;
            return false;
        }

        m_multisampleFrameBufferId = multisampleFrameBuffer;
    }

#endif

    return true;
}


////////////////////////////////////////////////////////////
bool RenderTextureImplFBO::activate(bool active)
{
    // Unbind the FBO if requested
    if (!active)
    {
        glCheck(glBindFramebuffer(GL_FRAMEBUFFER, 0));
    }
    else
    {
		if (m_frameBufferId)
		{
            glCheck(glBindFramebuffer(GL_FRAMEBUFFER, m_frameBufferId));
			return true;
		}
    }

    return createFrameBuffer();
}


////////////////////////////////////////////////////////////
void RenderTextureImplFBO::updateTexture(unsigned int)
{
    // If multisampling is enabled, we need to resolve by blitting
    // from our FBO with multisample renderbuffer attachments
    // to our FBO to which our target texture is attached

#ifndef SFML_OPENGL_ES

    // In case of multisampling, make sure both FBOs
    // are already available within the current context
    if (m_multisample && m_width && m_height && activate(true))
    {
		// Set up the blit target (draw framebuffer) and blit (from the read framebuffer, our multisample FBO)
		glCheck(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_frameBufferId));
		glCheck(glBlitFramebuffer(0, 0, m_width, m_height, 0, 0, m_width, m_height, GL_COLOR_BUFFER_BIT, GL_NEAREST));
		glCheck(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_multisampleFrameBufferId));
    }

#endif // SFML_OPENGL_ES

}

} // namespace priv

} // namespace sf
