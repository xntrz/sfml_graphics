@ECHO OFF
PUSHD "%~dp0"
rmdir /q /s "obj/Release"
make config=release
pause
POPD