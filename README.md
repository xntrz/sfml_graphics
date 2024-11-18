**sfml_graphics**\
Standalone `Graphics` & `System` module of [SFML](https://github.com/SFML/SFML) 2.5.1 library for SDL2 (see `example/`) that can be used for emscripten / android / windows\
\
**dependencies**\
[SDL2](https://github.com/libsdl-org/SDL) - loading GL extensions via `SDL_GL_GetProcAddress`\
[freetype](https://github.com/freetype/freetype) - using `sf::Font` & `sf::Text` objects\
\
**notes**\
all `loadFromFile` functions are erased and should be replaced with `loadFromMemory` or `loadFromStream`