#!/bin/bash
# Local build (Linux / WSL / macOS with mingw-w64):  ./build.sh [x86_64|i686]
# Produces build/MontyShell-<arch>.exe and build/MontyShell-Setup-<arch>.exe
set -e
ARCH="${1:-x86_64}"
CC="${ARCH}-w64-mingw32-gcc"
WINDRES="${ARCH}-w64-mingw32-windres"
CFLAGS="-std=gnu11 -O2 -Wall -Wno-unused-function -Wno-unused-variable -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0601 -DWINVER=0x0601"
LDFLAGS="-mwindows -s -static"
LIBS="-lgdi32 -luser32 -lmsimg32 -lcomdlg32 -lshell32 -lole32 -luuid -liphlpapi -lws2_32 -ladvapi32"

mkdir -p build
echo "== compiling MontyShell-${ARCH}.exe"
$CC $CFLAGS src/*.c -o "build/MontyShell-${ARCH}.exe" $LDFLAGS $LIBS

echo "== building installer for ${ARCH}"
echo "1 RCDATA \"build/MontyShell-${ARCH}.exe\"" > build/res.rc
$WINDRES build/res.rc -O coff -o build/res.o
$CC $CFLAGS installer/setup.c src/draw.c src/themes.c src/settings.c build/res.o \
    -o "build/MontyShell-Setup-${ARCH}.exe" $LDFLAGS $LIBS
ls -l build/*.exe
