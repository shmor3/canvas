#!/bin/bash
# Canvas package — build MAGI program with SDL2 for Linux
#
# Usage: ./build-linux.sh <file.magi> [output_name]
# Or:    ./build-linux.sh <file.o> [output_name]  (pre-compiled)

set -e
DIR="$(cd "$(dirname "$0")" && pwd)"
INPUT="$1"
OUT="${2:-a.out}"

if [ ! -f "$INPUT" ]; then
    echo "Usage: $0 <file.magi|file.o> [output_name]"
    exit 1
fi

# If .magi source, compile to .o first
if [[ "$INPUT" == *.magi ]]; then
    echo "Compiling MAGI source..."
    OBJ="/tmp/_canvas_build_$$.o"
    RT="/tmp/_canvas_build_$$.magi_rt.c"
    magi compile "$INPUT" -o "/tmp/_canvas_build_$$" 2>/dev/null || true
    # The compiler generates .o and .magi_rt.c then tries to link (may fail without SDL2)
    # We just need the .o and .magi_rt.c
    if [ ! -f "$OBJ" ]; then
        echo "ERROR: Compilation failed"
        exit 1
    fi
else
    OBJ="$INPUT"
    RT="${OBJ%.o}.magi_rt.c"
fi

if [ ! -f "$RT" ]; then
    echo "ERROR: Runtime file not found: $RT"
    exit 1
fi

# Patch runtime to chain SDL2 calls before "Unknown: return null"
PATCHED_RT="/tmp/_canvas_rt_patched_$$.c"
sed 's|// Unknown: return null|{ extern int64_t canvas_sdl_dispatch(const char*, int32_t, int64_t*); if (strncmp(name, "sdl_", 4) == 0) return canvas_sdl_dispatch(name, argc, args); } // Unknown: return null|' "$RT" > "$PATCHED_RT"

echo "Linking with SDL2..."
cc "$OBJ" "$PATCHED_RT" "$DIR/lib/magi_sdl2.c" \
   -I"$DIR/lib/include" \
   "$DIR/lib/linux-x86_64/libSDL2.a" \
   -lpthread -ldl -lm -O2 \
   -o "$OUT"

rm -f "$PATCHED_RT"
if [[ "$INPUT" == *.magi ]]; then
    rm -f "$OBJ" "$RT"
fi

SIZE=$(stat -c%s "$OUT" 2>/dev/null || stat -f%z "$OUT")
echo "Built: $OUT ($SIZE bytes, Linux x86_64)"
