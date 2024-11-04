**sfml_graphics**\
Standalone `Graphics` module of [SFML](https://github.com/SFML/SFML) 2.5.1 library for SDL2 (see `example/`) that can be used for emscripten / android / windows\
\
NOTE: all `loadFromFile` and `loadFromStream` are erased (returns false always) and should be replaced with `loadFromMemory`\
NOTE: Build files are omitted due requirements of addition libraries like freetype, that should be build separately by emscripten / android cmake toolchains
