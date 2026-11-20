#!/usr/bin/env bash
#
# mayhem/build.sh — builds LeviLamina's BDS-independent parsers into libFuzzer harnesses.
#
# Upstream builds one Windows DLL with xmake + clang-cl against the closed Bedrock Dedicated
# Server binary, so its build cannot run here. We compile only the LeviLamina sources the
# harnesses need (src/ll/...), with upstream's own compile model reproduced by hand:
#   * C++23 on libc++ (upstream uses clang + libc++ semantics), MSVC-compat mode because the
#     headers use MSVC-isms, and src/ll/api/Global.h force-included (upstream's PCH);
#   * header-only deps pinned to upstream's xmake versions, cached under /opt/toolchains/deps
#     by mayhem/Dockerfile (air-gapped re-run: nothing is fetched here);
#   * mayhem/harnesses/ll_stubs.cpp supplies the few symbols whose only implementation is in
#     *_win.cpp files or in BDS (logger output, locale query, exception-ref helpers).
#
# Outputs (in /mayhem/mayhem/out, gitignored): <t>_fuzz, <t>_fuzz-standalone, ll_kat (test oracle).
set -euo pipefail

[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${SRC:=/mayhem}"
: "${STANDALONE_FUZZ_MAIN:=/opt/mayhem/StandaloneFuzzTargetMain.c}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS

cd "$SRC"
DEPS=/opt/toolchains/deps
OUT="$SRC/mayhem/out"
OBJ="$OUT/obj"
mkdir -p "$OUT" "$OBJ/fuzz" "$OBJ/sa" "$OBJ/test" "$OUT/cfg/ll/core"

# Generated header (xmake configures it from Version.h.in); regenerated every run.
sed -e 's/\${LL_VERSION_MAJOR}/1/' -e 's/\${LL_VERSION_MINOR}/0/' -e 's/\${LL_VERSION_PATCH}/0/' \
    -e 's/\${define LL_VERSION_PRERELEASE}//' -e 's/\${GIT_COMMIT}/0/' -e 's/\${LL_WORKSPACE_FOLDER}//' \
    src/ll/core/Version.h.in > "$OUT/cfg/ll/core/Version.h.tmp"
cmp -s "$OUT/cfg/ll/core/Version.h.tmp" "$OUT/cfg/ll/core/Version.h" 2>/dev/null \
    && rm "$OUT/cfg/ll/core/Version.h.tmp" || mv "$OUT/cfg/ll/core/Version.h.tmp" "$OUT/cfg/ll/core/Version.h"

INC=(
  -Isrc -Isrc-server -I"$OUT/cfg"
  -I"$DEPS/fmt-11.2.0/src/include"
  -I"$DEPS/nlohmann-json-3.12.0/src/include"
  -I"$DEPS/magic_enum-0.9.7/src/include"
  -I"$DEPS/expected-lite-f339d2f/src/include"
  -I"$DEPS/parallel-hashmap-2.0.0/src"
  -I"$DEPS/concurrentqueue-1.0.4/src"
  -I"$DEPS/gsl-4.2.0/src/include"
  -I"$DEPS/entt-4.0.0/src/src"
  -I"$DEPS/glm-1.0.1/src"
  -I"$DEPS/type_safe-0.2.4/src/include"
  -I"$DEPS/ctre-3.8.1/src/include"
  -I"$DEPS/pfr-294a497/src/include"
  -I"$DEPS/pcg-428802d/src/include"
  -I"$DEPS/leveldb-1.23/src/include"
  -I"$DEPS/rapidjson-297637b/src/include"
  -I"$DEPS/stb-6b5799b/src"
)
CXXSTD=(
  -std=c++23 -stdlib=libc++ -DLL_PLAT_S -DFMT_HEADER_ONLY -w
  -fms-extensions -fms-compatibility -fms-compatibility-version=19.40
  "-D__extern_always_inline=extern __always_inline __attribute__((__gnu_inline__))"
  "-D__extern_inline=extern __inline __attribute__((__gnu_inline__))"
  -include mayhem/harnesses/libcxx19_from_chars.h
  -include src/ll/api/Global.h
)
# Everything is libc++. clang's stock libFuzzer runtime (what -fsanitize=fuzzer links) is built
# against libstdc++, and libstdc++ + libc++abi in one process share the unversioned std::exception /
# std::exception_ptr symbols, which corrupts exception handling (false heap-use-after-free in
# ~exception). So the default engine is libFuzzer 19.1.7 (clang's own version) compiled against
# libc++ by mayhem/Dockerfile; an explicit non-default $LIB_FUZZING_ENGINE is used as given.
LIBFUZZER_LIBCXX=/opt/toolchains/deps/libfuzzer-19.1.7/lib/libFuzzer.a
if [ "$LIB_FUZZING_ENGINE" = "-fsanitize=fuzzer" ] && [ -f "$LIBFUZZER_LIBCXX" ]; then
  FUZZ_ENGINE=("$LIBFUZZER_LIBCXX")
else
  FUZZ_ENGINE=($LIB_FUZZING_ENGINE)
fi
LINK_LIBS=(-stdlib=libc++ -lz -lpthread)

LL_SRCS=(
  src/ll/core/mod/ModLoadPlanner.cpp
  src/ll/api/reflection/ReflectionError.cpp
  src/ll/api/Expected.cpp
  src/ll/api/i18n/I18n.cpp
  src/ll/api/utils/SystemUtils.cpp
  src/ll/api/utils/SystemUtils_linux.cpp
  src/ll/api/utils/CompressUtils.cpp
  src/ll/api/io/FileUtils.cpp
  mayhem/harnesses/ll_stubs.cpp
)

# Upstream's own unit tests for the harnessed code (src-test/common), run by mayhem/test.sh.
TEST_SRCS=(src-test/common/VersionTest.cpp src-test/common/ModLoadPlannerTest.cpp src-test/common/CompressTest.cpp)
GTEST="$DEPS/googletest-1.17.0"

# compile <objdir> <src> <flags...>: rebuild only when the source or a flag changed (idempotent).
compile() {
  local dir=$1 src=$2; shift 2
  local o="$dir/$(echo "$src" | tr '/.' '__').o"
  local sig; sig=$(printf '%s\n' "$CXX" "$@" | sha1sum | cut -c1-40)
  if [ -f "$o" ] && [ -f "$o.sig" ] && [ "$(cat "$o.sig")" = "$sig" ] && [ "$o" -nt "$src" ]; then
    echo "$o"; return
  fi
  "$CXX" "${CXXSTD[@]}" "${INC[@]}" "$@" -c "$src" -o "$o" >&2
  echo "$sig" > "$o.sig"
  echo "$o"
}

FUZZ_FLAGS=($SANITIZER_FLAGS $DEBUG_FLAGS -O1 -fsanitize=fuzzer-no-link)
# standalone reproducers: same sanitizers, no SanitizerCoverage (links without libFuzzer).
SA_FLAGS=($SANITIZER_FLAGS $DEBUG_FLAGS -O1)
TEST_FLAGS=(-O1 $COVERAGE_FLAGS -I"$GTEST/include")

# 1) LeviLamina sources, sanitized + coverage-instrumented (fuzz) and with normal flags (test).
FUZZ_OBJS=(); SA_OBJS=(); TEST_OBJS=()
HARN=(mayhem/harnesses/{version,manifest,i18n,decompress,config}_fuzz.cpp)
pids=()
for s in "${HARN[@]}"; do
  ( compile "$OBJ/fuzz" "$s" "${FUZZ_FLAGS[@]}" >/dev/null ) & pids+=($!)
  ( compile "$OBJ/sa" "$s" "${SA_FLAGS[@]}" >/dev/null ) & pids+=($!)
done
for s in "${TEST_SRCS[@]}"; do ( compile "$OBJ/test" "$s" "${TEST_FLAGS[@]}" >/dev/null ) & pids+=($!); done
for s in "${LL_SRCS[@]}"; do
  ( compile "$OBJ/fuzz" "$s" "${FUZZ_FLAGS[@]}" >/dev/null ) & pids+=($!)
  ( compile "$OBJ/sa" "$s" "${SA_FLAGS[@]}" >/dev/null ) & pids+=($!)
  ( compile "$OBJ/test" "$s" "${TEST_FLAGS[@]}" >/dev/null ) & pids+=($!)
done
for p in "${pids[@]}"; do wait "$p"; done
for s in "${LL_SRCS[@]}"; do
  FUZZ_OBJS+=("$(compile "$OBJ/fuzz" "$s" "${FUZZ_FLAGS[@]}")")
  SA_OBJS+=("$(compile "$OBJ/sa" "$s" "${SA_FLAGS[@]}")")
  TEST_OBJS+=("$(compile "$OBJ/test" "$s" "${TEST_FLAGS[@]}")")
done

$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c mayhem/lsan_off.cc -o "$OBJ/lsan_off.o"
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$OBJ/standalone_main.o"

# 2) Harnesses: libFuzzer binary + standalone reproducer.
for t in version manifest i18n decompress config; do
  h="$(compile "$OBJ/fuzz" "mayhem/harnesses/${t}_fuzz.cpp" "${FUZZ_FLAGS[@]}")"
  hs="$(compile "$OBJ/sa" "mayhem/harnesses/${t}_fuzz.cpp" "${SA_FLAGS[@]}")"
  "$CXX" $SANITIZER_FLAGS $DEBUG_FLAGS "$h" "${FUZZ_OBJS[@]}" "$OBJ/lsan_off.o" \
      "${FUZZ_ENGINE[@]}" "${LINK_LIBS[@]}" -o "$OUT/${t}_fuzz"
  "$CXX" $SANITIZER_FLAGS $DEBUG_FLAGS "$OBJ/standalone_main.o" "$hs" "${SA_OBJS[@]}" "$OBJ/lsan_off.o" \
      "${LINK_LIBS[@]}" -o "$OUT/${t}_fuzz-standalone"
done

# 3) Upstream unit tests (normal flags, no sanitizers) against googletest built with libc++.
K=(); for s in "${TEST_SRCS[@]}"; do K+=("$(compile "$OBJ/test" "$s" "${TEST_FLAGS[@]}")"); done
"$CXX" $COVERAGE_FLAGS "${K[@]}" "${TEST_OBJS[@]}" "$GTEST/lib/libgtest_main.a" "$GTEST/lib/libgtest.a" \
    "${LINK_LIBS[@]}" -o "$OUT/ll_unittests"

ls -l "$OUT"/*_fuzz "$OUT"/*_fuzz-standalone "$OUT/ll_unittests"
