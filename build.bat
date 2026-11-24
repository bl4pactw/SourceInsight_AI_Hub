@echo off
rem Build sihub.exe with MSVC (run from "x64 Native Tools Command Prompt for VS")
setlocal
set LIBSSH2_DIR=third_party\libssh2
set LIBSSH2_FLAGS=/DLIBSSH2_WINCNG /DLIBSSH2_ECDSA_WINCNG /I %LIBSSH2_DIR%\include
set LIBSSH2_SOURCES=agent bcrypt_pbkdf channel comp chacha cipher-chachapoly crypt crypto global hostkey keepalive kex knownhost mac misc packet pem poly1305 publickey scp session sftp transport userauth userauth_kbd_packet version
if not exist build mkdir build
for %%n in (%LIBSSH2_SOURCES%) do (
    cl /nologo /O2 /c %LIBSSH2_FLAGS% %LIBSSH2_DIR%\src\%%n.c /Fo:build\libssh2_%%n.obj
    if errorlevel 1 exit /b 1
)
rc /nologo /i res /fo build\sihub.res res\sihub.rc
if errorlevel 1 exit /b 1
cl /nologo /O2 /W4 /DUNICODE /D_UNICODE /I %LIBSSH2_DIR%\include src\*.c build\*.obj build\sihub.res /Fe:sihub.exe ^
   /link /SUBSYSTEM:WINDOWS user32.lib shell32.lib ws2_32.lib bcrypt.lib crypt32.lib
if errorlevel 1 exit /b 1
del /q *.obj 2>nul
rmdir /s /q build
echo Built sihub.exe
