#!/bin/sh
set -e
CC=${CC:-x86_64-w64-mingw32-gcc}
WINDRES=${WINDRES:-x86_64-w64-mingw32-windres}
LIBSSH2_DIR=third_party/libssh2
LIBSSH2_SOURCES="agent bcrypt_pbkdf channel comp chacha cipher-chachapoly crypt crypto global hostkey keepalive kex knownhost mac misc packet pem poly1305 publickey scp session sftp transport userauth userauth_kbd_packet version"
LIBSSH2_FLAGS="-DLIBSSH2_WINCNG -DLIBSSH2_ECDSA_WINCNG -I$LIBSSH2_DIR/include"
BUILD_DIR=build
mkdir -p $BUILD_DIR
for name in $LIBSSH2_SOURCES; do
	$CC -O2 $LIBSSH2_FLAGS -c $LIBSSH2_DIR/src/$name.c -o $BUILD_DIR/libssh2_$name.o
done
$WINDRES -I res res/sihub.rc -O coff -o $BUILD_DIR/sihub_res.o
$CC -O2 -s -Wall -Wextra -DUNICODE -D_UNICODE -municode -mwindows -static -I$LIBSSH2_DIR/include src/*.c $BUILD_DIR/*.o -o sihub.exe -lshell32 -lws2_32 -lbcrypt -lcrypt32
rm -rf $BUILD_DIR
