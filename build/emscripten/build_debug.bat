@ECHO OFF
PUSHD "%~dp0"
rmdir /q /s "obj/Debug"
make config=debug
pause
POPD