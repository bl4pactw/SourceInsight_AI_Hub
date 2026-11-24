@echo off
rem Build with CMake + Visual Studio (CMake ships with Visual Studio), output in dist\
cmake -S . -B build -A x64
if errorlevel 1 exit /b 1
cmake --build build --config Release
if errorlevel 1 exit /b 1
cmake --install build --config Release --prefix dist
if errorlevel 1 exit /b 1
echo.
echo Done: copy the contents of dist\ to your sihub folder (e.g. C:\Tools\SiHub).
