#!/bin/sh
set -e
CC=${CC:-x86_64-w64-mingw32-gcc}
WINDRES=${WINDRES:-x86_64-w64-mingw32-windres}
$WINDRES -I res res/sihub.rc -O coff -o sihub_res.o
$CC -O2 -s -Wall -Wextra -DUNICODE -D_UNICODE -municode -mwindows -static src/*.c sihub_res.o -o sihub.exe -lshell32
rm -f sihub_res.o
