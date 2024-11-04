#include <cstdio>
#include <cassert>

#include <SDL2/SDL.h>


#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif


#include "SFML/Graphics.hpp"


//
//  "stolen" from raylib xD (https://github.com/raysan5/raylib/blob/master/src/rtextures.c#L947)
//
sf::Image GenImageChecked(int width, int height, int checksX, int checksY, sf::Color col1, sf::Color col2)
{
    sf::Color* pixels = reinterpret_cast<sf::Color*>(std::malloc(width * height * sizeof(sf::Color)));

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            if ((x / checksX + y / checksY) % 2 == 0) pixels[y * width + x] = col1;
            else pixels[y * width + x] = col2;
        };
    };

    sf::Image image;
    image.create(width, height, reinterpret_cast<const sf::Uint8*>(pixels));

    std::free(pixels);

    return image;
}


int main(int argc, char** argv)
{
    //
    //  init SDL2, currently we are needed for events (event loop) and video (sdl2 gl)
    //
    if (SDL_Init(SDL_INIT_EVENTS | SDL_INIT_VIDEO) < 0)
    {
        printf("Init SDL failed! %s\n", SDL_GetError());
        return -1;
    };

    //
    //  init SDL2 window
    //
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);

    SDL_Window* window = SDL_CreateWindow("SFML EMSCRIPTEN SDL2",
                                          SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED,
                                          800,
                                          600,
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE);
    assert(window);
    if (!window)
    {
        printf("Init SDL window failed! %s\n", SDL_GetError());
        return -1;
    };

    //
    //  create SDL2 OPENGL context
    //
    SDL_GLContext glctx = SDL_GL_CreateContext(window);
    assert(glctx);
    if (!glctx)
    {
        printf("Init SDL OPENGL context failed! %s\n", SDL_GetError());
        return -1;
    };

    SDL_GL_MakeCurrent(window, glctx);
    SDL_ShowWindow(window);

    //
    //  now we can freely use sfml-graphics module  
    //
    sf::RenderWindow rw(800, 600);
    rw.onCreate(); // WARNING: required to update initial view from render window size

    sf::Image imgChecker = GenImageChecked(800, 600, 32, 32, sf::Color(90, 90, 90, 255), sf::Color(130, 130, 130, 255));
    sf::Texture texChecker;
    texChecker.loadFromImage(imgChecker);

    sf::Sprite spr;
    spr.setTexture(texChecker);

    sf::View camera2d;
    camera2d.setSize({ 800.0f, 600.0f });
    camera2d.setCenter({ 800.0f * 0.5f, 600.0f * 0.5f });
    rw.setView(camera2d);

    sf::RectangleShape rc;
    rc.setSize({ 32.0f, 32.0f });
    rc.setOrigin({ 32.0f * 0.5f, 32.0f * 0.5f });
    rc.setPosition({ 800.0f * 0.5f, 600.0f * 0.5f });
    rc.setFillColor(sf::Color::Red);
    
    sf::CircleShape cs;
    cs.setRadius(52.0f);
    cs.setOrigin({ 52.0f * 0.5f, 52.0f * 0.5f });
    cs.setPosition({ 32.0f, 32.0f });
    cs.setFillColor(sf::Color::Yellow);
    cs.setOutlineThickness(2.0f);
    cs.setOutlineColor(sf::Color::White);

    sf::RenderTexture rt;
    rt.create(800, 600); // WARNING: antialiasing not tested yet
    rt.setView(camera2d);    

    bool quit = false;
    while (!quit)
    {
        SDL_Event event = { 0 };
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            case SDL_QUIT:
                quit = true;
                break;

            case SDL_WINDOWEVENT:
                {
                    switch (event.window.event)
                    {
                    case SDL_WINDOWEVENT_RESIZED:
                    case SDL_WINDOWEVENT_SIZE_CHANGED:
                        rw.onResize(event.window.data1, event.window.data2);
                        break;

                    default:
                        break;
                    };
                }
                break;

            default:
                break;
            };
        };

        rt.clear(sf::Color::Black);
        rt.draw(rc);
        rt.draw(cs);
        rt.display();
        
        rw.clear(sf::Color::Black);
        rw.draw(spr);
        rw.draw(rc);
        rw.draw(cs);

        sf::RectangleShape rtRc;
        rtRc.setTexture(&rt.getTexture());
        rtRc.setSize({ 256.0f, 256.0f });
        rtRc.setPosition({ 0.0f, 256.0f });

        rw.draw(rtRc);

        SDL_GL_SwapWindow(window);
    };

    SDL_Quit();

    return 0;
};