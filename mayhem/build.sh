#!/usr/bin/env bash
# mayhem/build.sh — build the sanitized OpenVDB core library + fuzz harnesses, and the
# NanoVDB unit test suite (normal flags) that mayhem/test.sh runs.
set -euo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${SRC:=/mayhem}"
export CC CXX
cd "$SRC"

BF="$SRC/mayhem/build-fuzz"
BT="$SRC/mayhem/build-test"
mkdir -p "$BT"

# 1) OpenVDB core, static, instrumented. -fsanitize=fuzzer-no-link adds SanCov edges.
#    No -DCMAKE_*_COMPILER (CMake takes $CC/$CXX).
FLAGS="$SANITIZER_FLAGS $DEBUG_FLAGS -fsanitize=fuzzer-no-link"
cmake -S "$SRC" -B "$BF" -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_FLAGS="$FLAGS -O1" -DCMAKE_CXX_FLAGS="$FLAGS -O1" \
  -DCMAKE_C_FLAGS_DEBUG="" -DCMAKE_CXX_FLAGS_DEBUG="" \
  -DOPENVDB_BUILD_CORE=ON -DOPENVDB_CORE_SHARED=OFF -DOPENVDB_CORE_STATIC=ON \
  -DOPENVDB_BUILD_BINARIES=OFF -DOPENVDB_BUILD_NANOVDB=OFF -DOPENVDB_BUILD_UNITTESTS=OFF \
  -DUSE_CCACHE=OFF -DCONCURRENT_MALLOC=None -DUSE_ZLIB=ON -DUSE_ZSTD=ON \
  -DOPENVDB_ENABLE_ASSERTS=ON -DUSE_EXPLICIT_INSTANTIATION=OFF -DOPENVDB_FUTURE_DEPRECATION=OFF
cmake --build "$BF" -j"$MAYHEM_JOBS" --target openvdb_static

LIBVDB="$(find "$BF/openvdb" -name 'libopenvdb.a' | head -1)"
# version.h is generated into $BF/openvdb/openvdb/openvdb and included both as
# <openvdb/version.h> and as "version.h".
VDB_INC="-I$SRC/openvdb -I$BF/openvdb/openvdb -I$BF/openvdb/openvdb/openvdb -I$SRC/ext/imath -I$SRC/nanovdb -DOPENVDB_STATICLIB -DTBB_USE_DEBUG"
LIBS="$LIBVDB -ltbb -lz -lzstd -lpthread -lm"

$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$BF/standalone_main.o"
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c "$SRC/mayhem/lsan_off.cc" -o "$BF/lsan_off.o"

HFLAGS="-std=c++17 -O1 $SANITIZER_FLAGS $DEBUG_FLAGS -DNANOVDB_USE_ZIP -DNANOVDB_USE_OPENVDB $VDB_INC -I$SRC/openvdb_cmd/vdb_tool/include"
build_harness() {
  local h="$1"
  $CXX $HFLAGS -fsanitize=fuzzer-no-link -c "$SRC/mayhem/harnesses/$h.cc" -o "$BF/$h.o"
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE "$BF/$h.o" "$BF/lsan_off.o" $LIBS -o "$SRC/$h"
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS "$BF/$h.o" "$BF/standalone_main.o" "$BF/lsan_off.o" $LIBS -o "$SRC/$h-standalone"
}
pids=()
for h in fuzz_vdb_stream fuzz_vdb_file fuzz_nanovdb_read fuzz_nanovdb_metadata fuzz_nanovdb_validate fuzz_vdbtool_geometry; do
  build_harness "$h" & pids+=("$!")
done

# 2) NanoVDB's own gtest suite (header-only), normal flags, for mayhem/test.sh.
$CXX -std=c++17 -O1 $COVERAGE_FLAGS -Wno-invalid-offsetof -DNANOVDB_USE_ZIP \
  -I"$SRC/nanovdb" -I"$SRC/nanovdb/nanovdb/unittest" \
  "$SRC/nanovdb/nanovdb/unittest/TestNanoVDB.cc" -lgtest -lgtest_main -lz -lpthread \
  -o "$BT/nanovdb_test_nanovdb"
for p in "${pids[@]}"; do wait "$p"; done
