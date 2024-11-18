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

#ifndef SFML_GLEXTENSIONS_HPP
#define SFML_GLEXTENSIONS_HPP

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Config.hpp>
#include <SFML/glad.h>


#if defined(SFML_OPENGL_ES)
#define SFML_GL_EXT_glGenVertexArrays       glGenVertexArraysOES
#define SFML_GL_EXT_glBindVertexArray       glBindVertexArrayOES
#define SFML_GL_EXT_glDeleteVertexArrays    glDeleteVertexArraysOES
#else
#define SFML_GL_EXT_glGenVertexArrays       glGenVertexArrays
#define SFML_GL_EXT_glBindVertexArray       glBindVertexArray
#define SFML_GL_EXT_glDeleteVertexArrays    glDeleteVertexArrays
#endif


namespace sf
{
namespace priv
{

////////////////////////////////////////////////////////////
/// \brief Make sure that extensions are initialized
///
////////////////////////////////////////////////////////////
void ensureExtensionsInit();

} // namespace priv

} // namespace sf


#endif // SFML_GLEXTENSIONS_HPP
