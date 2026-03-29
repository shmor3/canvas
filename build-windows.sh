#!/bin/bash
# Canvas package — cross-compile MAGI program with SDL2 for Windows
#
# Usage: ./build-windows.sh <file.magi> [output_name.exe]
# Requires: magi, x86_64-w64-mingw32-gcc

set -e
DIR="$(cd "$(dirname "$0")" && pwd)"
INPUT="$1"
OUT="${2:-doom.exe}"

if [ ! -f "$INPUT" ]; then
    echo "Usage: $0 <file.magi|file.o> [output_name.exe]"
    exit 1
fi

if [[ "$INPUT" == *.magi ]]; then
    echo "Compiling MAGI source..."
    TMP="/tmp/_canvas_win_$$"
    magi compile "$INPUT" --target x86_64-pc-windows-gnu -o "$TMP" 2>/dev/null || true
    OBJ="$TMP.o"
    RT="$TMP.magi_rt.c"
else
    OBJ="$INPUT"
    RT="${OBJ%.o}.magi_rt.c"
fi

if [ ! -f "$OBJ" ]; then
    echo "ERROR: Object file not found. Compile with: magi compile file.magi --target x86_64-pc-windows-gnu -o out"
    exit 1
fi

PATCHED_RT="/tmp/_canvas_rt_win_$$.c"
sed 's|// Unknown: return null|{ extern int64_t canvas_sdl_dispatch(const char*, int32_t, int64_t*); if (strncmp(name, "sdl_", 4) == 0) return canvas_sdl_dispatch(name, argc, args); } // Unknown: return null|' "$RT" > "$PATCHED_RT"

# Fix POSIX-only functions for Windows
sed -i 's|#include <unistd.h>|#ifdef _WIN32\n#include <windows.h>\n#include <direct.h>\n#define getcwd _getcwd\n#define getpid GetCurrentProcessId\n#else\n#include <unistd.h>\n#endif|' "$PATCHED_RT"
sed -i 's|clock_gettime(CLOCK_REALTIME, \&ts);|#ifdef _WIN32\nDWORD ms = GetTickCount(); ts.tv_sec = ms/1000; ts.tv_nsec = (ms%1000)*1000000;\n#else\nclock_gettime(CLOCK_REALTIME, \&ts);\n#endif|' "$PATCHED_RT"

echo "Cross-compiling for Windows..."
x86_64-w64-mingw32-gcc "$OBJ" "$PATCHED_RT" "$DIR/lib/magi_sdl2.c" \
    -I"$DIR/lib/include" \
    "$DIR/lib/windows-x86_64/libSDL2main.a" \
    "$DIR/lib/windows-x86_64/libSDL2.a" \
    -lmingw32 -lm -ldinput8 -ldxguid -ldxerr8 -luser32 -lgdi32 \
    -lwinmm -limm32 -lole32 -loleaut32 -lshell32 -lsetupapi -lversion -luuid \
    -mwindows -O2 \
    -o "$OUT"

rm -f "$PATCHED_RT"

echo "Built: $OUT (Windows x86_64)"
echo "Ship with: SDL2.dll (copy from $DIR/lib/windows-x86_64/SDL2.dll)"
