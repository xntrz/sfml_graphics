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
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Drawable.hpp>
#include <SFML/Graphics/Shader.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/VertexArray.hpp>
#include <SFML/Graphics/VertexBuffer.hpp>
#include <SFML/Graphics/GLCheck.hpp>
#include <SFML/System/Mutex.hpp>
#include <SFML/System/Lock.hpp>
#include <SFML/System/Err.hpp>
#include <cassert>
#include <iostream>
#include <algorithm>
#include <map>


namespace
{
    // Mutex to protect ID generation and our context-RenderTarget-map
    sf::Mutex mutex;

    // Last active render target id
    sf::Uint64 lastActiveId = 0;

    
    // Unique identifier, used for identifying RenderTargets when
    // tracking the currently active RenderTarget within a given context
    inline sf::Uint64 getUniqueId()
    {
        sf::Lock lock(mutex);

        static sf::Uint64 id = 1; // start at 1, zero is "no RenderTarget"

        return id++;
    }


    // Check if a RenderTarget with the given ID is active in the current context
    inline bool isActive(sf::Uint64 id)
    {
        return (lastActiveId == id);
    }

    
    // Convert an sf::BlendMode::Factor constant to the corresponding OpenGL constant.
    inline sf::Uint32 factorToGlConstant(sf::BlendMode::Factor blendFactor)
    {
        switch (blendFactor)
        {
            case sf::BlendMode::Zero:             return GL_ZERO;
            case sf::BlendMode::One:              return GL_ONE;
            case sf::BlendMode::SrcColor:         return GL_SRC_COLOR;
            case sf::BlendMode::OneMinusSrcColor: return GL_ONE_MINUS_SRC_COLOR;
            case sf::BlendMode::DstColor:         return GL_DST_COLOR;
            case sf::BlendMode::OneMinusDstColor: return GL_ONE_MINUS_DST_COLOR;
            case sf::BlendMode::SrcAlpha:         return GL_SRC_ALPHA;
            case sf::BlendMode::OneMinusSrcAlpha: return GL_ONE_MINUS_SRC_ALPHA;
            case sf::BlendMode::DstAlpha:         return GL_DST_ALPHA;
            case sf::BlendMode::OneMinusDstAlpha: return GL_ONE_MINUS_DST_ALPHA;
        }

        sf::err() << "Invalid value for sf::BlendMode::Factor! Fallback to sf::BlendMode::Zero." << std::endl;
#if defined(SFML_DEBUG)
        assert(false);
#endif        
        return GL_ZERO;
    }


    // Convert an sf::BlendMode::BlendEquation constant to the corresponding OpenGL constant.
    inline sf::Uint32 equationToGlConstant(sf::BlendMode::Equation blendEquation)
    {
        switch (blendEquation)
        {
            case sf::BlendMode::Add:             return GL_FUNC_ADD;
            case sf::BlendMode::Subtract:        return GL_FUNC_SUBTRACT;
            case sf::BlendMode::ReverseSubtract: return GL_FUNC_REVERSE_SUBTRACT;
            case sf::BlendMode::Min:             return GL_MIN;
            case sf::BlendMode::Max:             return GL_MAX;
        }

        sf::err() << "Invalid value for sf::BlendMode::Equation! Fallback to sf::BlendMode::Add." << std::endl;
#if defined(SFML_DEBUG)
        assert(false);
#endif        
        return GL_FUNC_ADD;
    }


    class SfmlRenderPipeline
    {
    private:
		static const std::size_t MAX_VERTEX = (256 * 256);

    public:
        SfmlRenderPipeline();
        ~SfmlRenderPipeline();
        void applyCurrentView(sf::View& view);
        void applyCurrentTransform(const sf::Transform& transform);
        void preDraw(const sf::Texture* texture, const sf::Shader* shader);
        void postDraw(const sf::Texture* texture, const sf::Shader* shader);

        void drawVertices(const sf::Vertex*   vertices,
                          sf::PrimitiveType   type,
                          std::size_t         firstVertex,
                          std::size_t         vertexCount,
                          const sf::Texture*  texture,
                          const sf::Shader*   shader);

        void drawVertexBuffer(const sf::VertexBuffer& vertexBuffer,
                              std::size_t             firstVertex,
                              std::size_t             vertexCount,
                              const sf::Texture*      texture,
                              const sf::Shader*       shader);

        void drawPrimitives(sf::PrimitiveType   type,
                            std::size_t         firstVertex,
                            std::size_t         vertexCount);

    private:
        sf::Transform   m_matProj;
        sf::Transform   m_matModelView;
        sf::Shader      m_shader;
        unsigned int    m_shaderId;
        int             m_locTexture0;
        int             m_locViewProj;
        int             m_locTexFlipped;
        int             m_locUseTexture;
        unsigned int    m_vao;
        unsigned int    m_vbo;
        unsigned int    m_cacheTextureId;
        bool            m_cacheTextureFlipped;       
		bool            m_cacheTextureUse;
    };

    
    SfmlRenderPipeline::SfmlRenderPipeline()
    : m_matProj()
    , m_matModelView()
    , m_shader()
    , m_shaderId(0)
    , m_locTexture0(-1)
    , m_locViewProj(-1)
    , m_locTexFlipped(-1)
    , m_locUseTexture(-1)
    , m_vao(0)
    , m_vbo(0)
    , m_cacheTextureId(0)
    , m_cacheTextureFlipped(false)
	, m_cacheTextureUse(false)
    {
        const char* vertexShaderSource =
            "#version 100                                           \n"
            "precision mediump float;                               \n"
            "uniform mat4 aViewProj;	                            \n"
            "uniform bool bTexFlip;                                 \n"
            "attribute vec2 aPos;                                   \n"
            "attribute vec4 aColor;			                        \n"
            "attribute vec2 aTexCoord;		                        \n"
            "varying vec4 oColor;			                        \n"
            "varying vec2 oTexCoord;		                        \n"
            "void main()                                            \n"
            "{                                                      \n"
            "   oColor = aColor;                                    \n"
            "   oTexCoord = aTexCoord;                              \n"
            "   if (bTexFlip)                                       \n"
            "       oTexCoord.y = 1.0 - oTexCoord.y;                \n"
            "                                                       \n"
            "   gl_Position = aViewProj * vec4(aPos.xy, 0.0, 1.0);  \n"
            "}\n\0";

		const char* fragmentShaderSource =
			"#version 100                                                   \n"
			"precision mediump float;                                       \n"
            "uniform sampler2D Texture0;                                    \n"
            "uniform bool bUseTexture;                                      \n"
            "varying vec4 oColor;                                           \n"
			"varying vec2 oTexCoord;                                        \n"
			"void main()                                                    \n"
            "{                                                              \n"
            "   if (bUseTexture)                                            \n"
            "       gl_FragColor = texture2D(Texture0, oTexCoord) * oColor; \n"
            "   else                                                        \n"
            "       gl_FragColor = oColor;                                  \n"
            "}\n\0";

        // order should match to sf::Vertex
        m_shader.setAttributes({ "aPos", "aColor", "aTexCoord" });

        bool bStatus = m_shader.loadFromMemory(vertexShaderSource, fragmentShaderSource);
        assert(bStatus == true);

        // we will use native handle for native glX functions to avoid alot of template calls of shader class
        m_shaderId = m_shader.getNativeHandle();

        glCheck(m_locTexture0 = glGetUniformLocation(m_shaderId, "Texture0"));
        assert(m_locTexture0 != -1);

        glCheck(m_locViewProj = glGetUniformLocation(m_shaderId, "aViewProj"));
        assert(m_locViewProj != -1);

        glCheck(m_locTexFlipped = glGetUniformLocation(m_shaderId, "bTexFlip"));
        assert(m_locTexFlipped != -1);

        glCheck(m_locUseTexture = glGetUniformLocation(m_shaderId, "bUseTexture"));
        assert(m_locUseTexture != -1);    

        // now create vertices buffer
        glCheck(SFML_GL_EXT_glGenVertexArrays(1, &m_vao));
        glCheck(glGenBuffers(1, &m_vbo));

        glCheck(SFML_GL_EXT_glBindVertexArray(m_vao));

        glCheck(glBindBuffer(GL_ARRAY_BUFFER, m_vbo));
        glCheck(glBufferData(GL_ARRAY_BUFFER, sizeof(sf::Vertex) * MAX_VERTEX, 0, GL_STREAM_DRAW));

        glCheck(glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(sf::Vertex), (void*)0));
        glCheck(glEnableVertexAttribArray(0));

        glCheck(glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(sf::Vertex), (void*)sizeof(sf::Vertex::position)));
        glCheck(glEnableVertexAttribArray(1));

        glCheck(glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(sf::Vertex), (void*)(sizeof(sf::Vertex::position) + sizeof(sf::Vertex::color))));
        glCheck(glEnableVertexAttribArray(2));

        glCheck(SFML_GL_EXT_glBindVertexArray(0));
    };


    SfmlRenderPipeline::~SfmlRenderPipeline()
    {
        glCheck(SFML_GL_EXT_glBindVertexArray(0));
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

        if (m_vbo)
        {
            glDeleteBuffers(1, &m_vbo);
            m_vbo = 0;
        };

        if (m_vao)
        {
            SFML_GL_EXT_glDeleteVertexArrays(1, &m_vao);
            m_vao = 0;
        };
    };


    void SfmlRenderPipeline::applyCurrentView(sf::View& view)
    {
	    m_matProj = view.getTransform();
    };


    void SfmlRenderPipeline::applyCurrentTransform(const sf::Transform& transform)
    {
        m_matModelView = transform;
    };


	void SfmlRenderPipeline::preDraw(const sf::Texture* texture, const sf::Shader* shader)
    {
        // if shader is passed execute user-defined pipeline for sf::Vertex
        if (shader)
        {
            shader->bind(shader);
            return;
        };

        GLint id = 0;

        // check for current shader        
        glCheck(glGetIntegerv(GL_CURRENT_PROGRAM, &id));
        if (static_cast<unsigned int>(id) != m_shaderId)
            glCheck(glUseProgram(m_shaderId));

		if (texture)
		{
			// update tex cache
			unsigned int textureId = texture->getNativeHandle();
			if (m_cacheTextureId != textureId)
			{
				m_cacheTextureId = textureId;

				glCheck(glGetIntegerv(GL_TEXTURE_BINDING_2D, &id));
				if (static_cast<unsigned int>(id) != m_cacheTextureId)
				{
					glActiveTexture(GL_TEXTURE0 + 0);
					glBindTexture(GL_TEXTURE_2D, textureId);
					glUniform1i(m_locTexture0, 0);
				};
			};

			// update flip cache
			if (texture->isFlipped())
			{
				if (!m_cacheTextureFlipped)
				{
					glUniform1i(m_locTexFlipped, static_cast<int>(true));
					m_cacheTextureFlipped = true;
				};
			}
			else
			{
				if (m_cacheTextureFlipped)
				{
					glUniform1i(m_locTexFlipped, static_cast<int>(false));
					m_cacheTextureFlipped = false;
				};
			};

			if (!m_cacheTextureUse)
			{
				glUniform1i(m_locUseTexture, static_cast<int>(true));
				m_cacheTextureUse = true;
			};
		}
		else
		{
			if (m_cacheTextureUse)
			{
				glUniform1i(m_locUseTexture, static_cast<int>(false));
				m_cacheTextureUse = false;
			};

			m_cacheTextureId = 0;
		};

		glUniformMatrix4fv(m_locViewProj, 1, GL_FALSE, (m_matProj *m_matModelView).getMatrix());			
	};


    void SfmlRenderPipeline::postDraw(const sf::Texture* texture, const sf::Shader* shader)
    {
        if (texture && texture->isAttachedToFBO())
            texture->bind(nullptr);

        if (shader)
            shader->bind(nullptr);
    };


    void SfmlRenderPipeline::drawVertices(const sf::Vertex*   vertices,
                                          sf::PrimitiveType   type,
                                          std::size_t         firstVertex,
                                          std::size_t         vertexCount,
                                          const sf::Texture*  texture,
                                          const sf::Shader*   shader)
    {
        preDraw(texture, shader);
        
		GLint id = 0;

        // check for current vertex array
        glCheck(glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &id));
        if (static_cast<unsigned int>(id) != m_vao)
            glCheck(SFML_GL_EXT_glBindVertexArray(m_vao));

        // check for current vertex buffer
        glCheck(glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &id));
        if (static_cast<unsigned int>(id) != m_vbo)
            glCheck(glBindBuffer(GL_ARRAY_BUFFER, m_vbo));

        // copy vertices
        glCheck(glBufferSubData(GL_ARRAY_BUFFER, 0, vertexCount * sizeof(sf::Vertex), vertices));

        // draw call
        drawPrimitives(type, firstVertex, vertexCount);

        postDraw(texture, shader);
    };

    
    void SfmlRenderPipeline::drawVertexBuffer(const sf::VertexBuffer& vertexBuffer,
                                              std::size_t             firstVertex,
                                              std::size_t             vertexCount,
                                              const sf::Texture*      texture,
                                              const sf::Shader*       shader)
    {
        preDraw(texture, shader);

        sf::VertexBuffer::bind(&vertexBuffer);

        drawPrimitives(vertexBuffer.getPrimitiveType(), firstVertex, vertexCount);

        sf::VertexBuffer::bind(nullptr);

        postDraw(texture, shader);
    };


    
    void SfmlRenderPipeline::drawPrimitives(sf::PrimitiveType   type,
                                            std::size_t         firstVertex,
                                            std::size_t         vertexCount)
    {
        static const GLenum modes [] = { GL_POINTS,     GL_LINES,           GL_LINE_STRIP,
                                         GL_TRIANGLES,  GL_TRIANGLE_STRIP,  GL_TRIANGLE_FAN, GL_QUADS };
        GLenum mode = modes[type];

        glCheck(glDrawArrays(mode, static_cast<GLint>(firstVertex), static_cast<GLsizei>(vertexCount)));
    };


    // pipeline ref count
    int pipelineRefCount = 0;

    
    // pipeline instance
    SfmlRenderPipeline* pipeline = nullptr;

    
    // creates new sfml render pipeline if not exist
    inline void pipelineCreate()
    {
        if (pipelineRefCount++ == 0)
        {
            pipeline = new SfmlRenderPipeline;
        };
    };


    // destroys sfml render pipelne if refs end
    inline void pipelineDestroy()
    {
        if (--pipelineRefCount == 0)
        {
            delete pipeline;
            pipeline = nullptr;
        };
    };
}


namespace sf
{
////////////////////////////////////////////////////////////
RenderTarget::RenderTarget() :
m_defaultView(),
m_view(),
m_cache(),
m_id(0)
{
    sf::priv::ensureExtensionsInit();
    m_cache.glStatesSet = false;
    pipelineCreate();
}


////////////////////////////////////////////////////////////
RenderTarget::~RenderTarget()
{
    pipelineDestroy();
}


////////////////////////////////////////////////////////////
void RenderTarget::clear(const Color& color)
{
    if (isActive(m_id) || setActive(true))
    {
        lastActiveId = m_id;

        glCheck(glClearColor(color.r / 255.f, color.g / 255.f, color.b / 255.f, color.a / 255.f));
        glCheck(glClear(GL_COLOR_BUFFER_BIT));
    }
}


////////////////////////////////////////////////////////////
void RenderTarget::setView(const View& view)
{
    m_view = view;
    m_cache.viewChanged = true;
}


////////////////////////////////////////////////////////////
const View& RenderTarget::getView() const
{
    return m_view;
}


////////////////////////////////////////////////////////////
const View& RenderTarget::getDefaultView() const
{
    return m_defaultView;
}


////////////////////////////////////////////////////////////
IntRect RenderTarget::getViewport(const View& view) const
{
    float width = static_cast<float>(getSize().x);
    float height = static_cast<float>(getSize().y);
    const FloatRect& viewport = view.getViewport();

    return IntRect(static_cast<int>(0.5f + width  * viewport.left),
                   static_cast<int>(0.5f + height * viewport.top),
                   static_cast<int>(0.5f + width  * viewport.width),
                   static_cast<int>(0.5f + height * viewport.height));
}


////////////////////////////////////////////////////////////
Vector2f RenderTarget::mapPixelToCoords(const Vector2i& point) const
{
    return mapPixelToCoords(point, getView());
}


////////////////////////////////////////////////////////////
Vector2f RenderTarget::mapPixelToCoords(const Vector2i& point, const View& view) const
{
    // First, convert from viewport coordinates to homogeneous coordinates
    Vector2f normalized;
    IntRect viewport = getViewport(view);
    normalized.x = -1.f + 2.f * (point.x - viewport.left) / viewport.width;
    normalized.y =  1.f - 2.f * (point.y - viewport.top)  / viewport.height;

    // Then transform by the inverse of the view matrix
    return view.getInverseTransform().transformPoint(normalized);
}


////////////////////////////////////////////////////////////
Vector2i RenderTarget::mapCoordsToPixel(const Vector2f& point) const
{
    return mapCoordsToPixel(point, getView());
}


////////////////////////////////////////////////////////////
Vector2i RenderTarget::mapCoordsToPixel(const Vector2f& point, const View& view) const
{
    // First, transform the point by the view matrix
    Vector2f normalized = view.getTransform().transformPoint(point);

    // Then convert to viewport coordinates
    Vector2i pixel;
    IntRect viewport = getViewport(view);
    pixel.x = static_cast<int>((normalized.x  + 1.f) / 2.f * viewport.width  + viewport.left);
    pixel.y = static_cast<int>((-normalized.y + 1.f) / 2.f * viewport.height + viewport.top);

    return pixel;
}


////////////////////////////////////////////////////////////
void RenderTarget::draw(const Drawable& drawable, const RenderStates& states)
{
    drawable.draw(*this, states);
}


////////////////////////////////////////////////////////////
void RenderTarget::draw(const Vertex*       vertices,
                        std::size_t         vertexCount,
                        PrimitiveType       type,
                        const RenderStates& states)
{
    // Nothing to draw?
    if (!vertices || (vertexCount == 0))
        return;

    if (isActive(m_id) || setActive(true))
    {
        setupDraw(states);

        pipeline->drawVertices(vertices, type, 0, vertexCount, states.texture, states.shader);

        cleanupDraw(states);
    }
}


////////////////////////////////////////////////////////////
void RenderTarget::draw(const VertexBuffer& vertexBuffer, const RenderStates& states)
{
    draw(vertexBuffer, 0, vertexBuffer.getVertexCount(), states);
}


////////////////////////////////////////////////////////////
void RenderTarget::draw(const VertexBuffer& vertexBuffer,
                        std::size_t         firstVertex,
                        std::size_t         vertexCount,
                        const RenderStates& states)
{
    // Sanity check
    if (firstVertex > vertexBuffer.getVertexCount())
        return;

    // Clamp vertexCount to something that makes sense
    vertexCount = std::min(vertexCount, vertexBuffer.getVertexCount() - firstVertex);

    // Nothing to draw?
    if (!vertexCount)
        return;

    if (isActive(m_id) || setActive(true))
    {
        setupDraw(states);

        pipeline->drawVertexBuffer(vertexBuffer, firstVertex, vertexCount, states.texture, states.shader);

        cleanupDraw(states);
    }
}


////////////////////////////////////////////////////////////
bool RenderTarget::setActive(bool active)
{
    m_cache.enable = false;
    return true;
}


////////////////////////////////////////////////////////////
void RenderTarget::pushGLStates()
{
    resetGLStates();
}


////////////////////////////////////////////////////////////
void RenderTarget::popGLStates()
{

}


////////////////////////////////////////////////////////////
void RenderTarget::resetGLStates()
{
    // Workaround for states not being properly reset on
    // macOS unless a context switch really takes place
#if defined(SFML_SYSTEM_MACOS)
    setActive(false);
#endif

    if (isActive(m_id) || setActive(true))
    {
        // Make sure that extensions are initialized
        priv::ensureExtensionsInit();

        // Define the default OpenGL states
        glCheck(glDisable(GL_CULL_FACE));
        glCheck(glDisable(GL_DEPTH_TEST));
        glCheck(glEnable(GL_BLEND));
        m_cache.glStatesSet = true;

        // Apply the default SFML states
        applyBlendMode(BlendAlpha);

        glCheck(VertexBuffer::bind(NULL));
        
        // Set the default view
        setView(getView());

        m_cache.enable = true;
    }
}


////////////////////////////////////////////////////////////
void RenderTarget::initialize()
{
    // Setup the default and current views
    m_defaultView.reset(FloatRect(0, 0, static_cast<float>(getSize().x), static_cast<float>(getSize().y)));
    m_view = m_defaultView;

    // Set GL states only on first draw, so that we don't pollute user's states
    m_cache.glStatesSet = false;

    // Generate a unique ID for this RenderTarget to track
    // whether it is active within a specific context
    m_id = getUniqueId();
}


////////////////////////////////////////////////////////////
void RenderTarget::applyCurrentView()
{
    // Set the viewport
    IntRect viewport = getViewport(m_view);
    int top = getSize().y - (viewport.top + viewport.height);
    glCheck(glViewport(viewport.left, top, viewport.width, viewport.height));

    pipeline->applyCurrentView(m_view);

    m_cache.viewChanged = false;
}


////////////////////////////////////////////////////////////
void RenderTarget::applyBlendMode(const BlendMode& mode)
{
    // Apply the blend mode, falling back to the non-separate versions if necessary
    if (GLAD_GL_EXT_blend_func_separate)
    {
        glCheck(glBlendFuncSeparate(
            factorToGlConstant(mode.colorSrcFactor), factorToGlConstant(mode.colorDstFactor),
            factorToGlConstant(mode.alphaSrcFactor), factorToGlConstant(mode.alphaDstFactor)));
    }
    else
    {
        glCheck(glBlendFunc(
            factorToGlConstant(mode.colorSrcFactor),
            factorToGlConstant(mode.colorDstFactor)));
    }

    if (GLAD_GL_EXT_blend_minmax && GLAD_GL_EXT_blend_subtract)
    {
        if (GLAD_GL_EXT_blend_equation_separate)
        {
            glCheck(glBlendEquationSeparate(
                equationToGlConstant(mode.colorEquation),
                equationToGlConstant(mode.alphaEquation)));
        }
        else
        {
            glCheck(glBlendEquation(equationToGlConstant(mode.colorEquation)));
        }
    }
    else if ((mode.colorEquation != BlendMode::Add) || (mode.alphaEquation != BlendMode::Add))
    {
        static bool warned = false;

        if (!warned)
        {
            err() << "OpenGL extension EXT_blend_minmax and/or EXT_blend_subtract unavailable" << std::endl;
            err() << "Selecting a blend equation not possible" << std::endl;
            err() << "Ensure that hardware acceleration is enabled if available" << std::endl;

            warned = true;
        }
    }

    m_cache.lastBlendMode = mode;
}


////////////////////////////////////////////////////////////
void RenderTarget::applyTransform(const Transform& transform)
{
    pipeline->applyCurrentTransform(transform);
}


////////////////////////////////////////////////////////////
void RenderTarget::setupDraw(const RenderStates& states)
{
    // First set the persistent OpenGL states if it's the very first call
    if (!m_cache.glStatesSet)
        resetGLStates();

    applyTransform(states.transform);

    // Apply the view
    if (!m_cache.enable || m_cache.viewChanged)
        applyCurrentView();

    // Apply the blend mode
    if (!m_cache.enable || (states.blendMode != m_cache.lastBlendMode))
        applyBlendMode(states.blendMode);
}


////////////////////////////////////////////////////////////
void RenderTarget::cleanupDraw(const RenderStates& states)
{
    // Re-enable the cache at the end of the draw if it was disabled
    m_cache.enable = true;
}

} // namespace sf


////////////////////////////////////////////////////////////
// Render states caching strategies
//
// * View
//   If SetView was called since last draw, the projection
//   matrix is updated. We don't need more, the view doesn't
//   change frequently.
//
// * Transform
//   The transform matrix is usually expensive because each
//   entity will most likely use a different transform. This can
//   lead, in worst case, to changing it every 4 vertices.
//   To avoid that, when the vertex count is low enough, we
//   pre-transform them and therefore use an identity transform
//   to render them.
//
// * Blending mode
//   Since it overloads the == operator, we can easily check
//   whether any of the 6 blending components changed and,
//   thus, whether we need to update the blend mode.
//
// * Texture
//   Storing the pointer or OpenGL ID of the last used texture
//   is not enough; if the sf::Texture instance is destroyed,
//   both the pointer and the OpenGL ID might be recycled in
//   a new texture instance. We need to use our own unique
//   identifier system to ensure consistent caching.
//
// * Shader
//   Shaders are very hard to optimize, because they have
//   parameters that can be hard (if not impossible) to track,
//   like matrices or textures. The only optimization that we
//   do is that we avoid setting a null shader if there was
//   already none for the previous draw.
//
////////////////////////////////////////////////////////////
