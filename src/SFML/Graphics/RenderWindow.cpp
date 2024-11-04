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
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/GLCheck.hpp>
#include <SFML/Graphics/RenderTextureImplFBO.hpp>
#include <SFML/Graphics/TextureSaver.hpp>


namespace sf
{
////////////////////////////////////////////////////////////
RenderWindow::RenderWindow(int w, int h)
: m_screenSize(Vector2u(w, h))
{
    // Nothing to do
}


////////////////////////////////////////////////////////////
RenderWindow::~RenderWindow()
{
    // Nothing to do
}


////////////////////////////////////////////////////////////
Vector2u RenderWindow::getSize() const
{
    return Vector2u(static_cast<Int32>(m_screenSize.x),
                    static_cast<Int32>(m_screenSize.y));
}


////////////////////////////////////////////////////////////
bool RenderWindow::setActive(bool active)
{
    bool result = RenderTarget::setActive(active);

    // If FBOs are available, make sure none are bound when we
    // try to draw to the default framebuffer of the RenderWindow
    if (active && result)
    {
        priv::RenderTextureImplFBO::unbind();

        return true;
    }

    return result;
}


////////////////////////////////////////////////////////////
Image RenderWindow::toImage()
{
    Vector2u windowSize = getSize();

    Texture texture;
    texture.create(windowSize.x, windowSize.y);
    toTexture(texture);

    return texture.copyToImage();
}


////////////////////////////////////////////////////////////
void RenderWindow::toTexture(const Texture& texture)
{
    toTexture(texture, 0, 0);
};


////////////////////////////////////////////////////////////
void RenderWindow::toTexture(const Texture& texture, unsigned int x, unsigned int y)
{
#if defined(SFML_DEBUG)
    assert((x + getSize().x) <= texture.getSize().x);
    assert((y + getSize().y) <= texture.getSize().y);
#endif
    
    if (texture.getNativeHandle())// && setActive(true))
    {
        // Make sure that the current texture binding will be preserved
        priv::TextureSaver save;

        // Copy pixels from the back-buffer to the texture
        glCheck(glBindTexture(GL_TEXTURE_2D, texture.getNativeHandle()));
        glCheck(glCopyTexSubImage2D(GL_TEXTURE_2D, 0, x, y, 0, 0, getSize().x, getSize().y));
        glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, texture.isSmooth() ? GL_LINEAR : GL_NEAREST));

        // Force an OpenGL flush, so that the texture will appear updated
        // in all contexts immediately (solves problems in multi-threaded apps)
        glCheck(glFlush());
    };
};


////////////////////////////////////////////////////////////
void RenderWindow::onCreate()
{
    // Just initialize the render target part
    RenderTarget::initialize();
}


////////////////////////////////////////////////////////////
void RenderWindow::onResize(int w, int h)
{
    // update window size
    m_screenSize = Vector2i(w, h);

    // Update the current view (recompute the viewport, which is stored in relative coordinates)
    setView(getView());
}

} // namespace sf
