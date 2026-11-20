@echo off
rem Build sihub.exe with MSVC (run from "x64 Native Tools Command Prompt for VS")
rc /nologo /i res /fo sihub.res res\sihub.rc
if errorlevel 1 exit /b 1
cl /nologo /O2 /W4 /DUNICODE /D_UNICODE src\*.c sihub.res /Fe:sihub.exe ^
   /link /SUBSYSTEM:WINDOWS user32.lib shell32.lib
if errorlevel 1 exit /b 1
del /q *.obj sihub.res 2>nul
echo Built sihub.exe
