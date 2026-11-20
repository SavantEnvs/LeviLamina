# syntax=docker/dockerfile:1.7
#
# Commit image for LeviLamina's BDS-independent parsers (see mayhem/build.sh for why upstream's
# xmake/Windows build is not used). Build from the repo root:
#   DOCKER_BUILDKIT=1 docker build -f mayhem/Dockerfile --build-arg COMMIT=$(git rev-parse HEAD) -t ll .
FROM ghcr.io/savantenvs/base:20261023

ARG COMMIT
ARG REPO_URL
ARG SOURCE_DATE_EPOCH
ARG SANITIZER_FLAGS
ARG COVERAGE_FLAGS=
ARG MAYHEM_JOBS=

# libc++ (the headers need C++23 library features; upstream builds against libc++ semantics).
USER root
RUN [ -n "$SOURCE_DATE_EPOCH" ] && repro-sources-list.sh || true ; \
    apt-get update && apt-get install -yq --no-install-recommends libc++-19-dev libc++abi-19-dev xz-utils \
 && rm -rf /var/lib/apt/lists/*

# Header-only dependencies at the versions upstream's xmake.lua pins (LiteLDev/xmake-repo for the
# LiteLDev packages), plus googletest for upstream's unit tests. Pinned by commit/tag + sha256 and
# cached under /opt/toolchains/deps/<name>-<version>/src so build.sh runs air-gapped.
RUN <<'EOS'
set -eu
while read -r name ver url sha; do
  d=/opt/toolchains/deps/$name-$ver
  mkdir -p "$d/src"
  curl -fsSL --retry 3 -o /tmp/dep.tgz "$url"
  echo "$sha  /tmp/dep.tgz" | sha256sum -c -
  tar -xzf /tmp/dep.tgz --strip-components=1 -C "$d/src"
  rm -f /tmp/dep.tgz
done <<'LIST'
    fmt 11.2.0 https://github.com/fmtlib/fmt/archive/40626af88bd7df9a5fb80be7b25ac85b122d6c21.tar.gz 992216a0605e03f0373984ecad11158f42d8569782f1b2c84035272da10f0ff1
    nlohmann-json 3.12.0 https://github.com/nlohmann/json/archive/55f93686c01528224f448c19128836e7df245f72.tar.gz 67f4cdd9ca930c9c1e130af4a437c7fc98fab77a2846fc2d2a14b4943831f8ef
    magic_enum 0.9.7 https://github.com/Neargye/magic_enum/archive/e046b69a3736d314fad813e159b1c192eaef92cd.tar.gz a06b11989a7802de62a405c373b23c3fcc4caca27e0a0df5f9905bc322a14a40
    expected-lite f339d2f https://github.com/martinmoene/expected-lite/archive/f339d2f73730f8fee4412f5e4938717866ecef48.tar.gz 8fe5f192f68b2a5c216316a1abea19e94c49f10c13431fbcd16ba6a611b2231f
    parallel-hashmap 2.0.0 https://github.com/greg7mdp/parallel-hashmap/archive/2ec799017610ef831f4dc29c21fb3cce7e4a19b9.tar.gz 11f42dd45abe08ad0c93d5509bfb24bc3877efdd11ce678e07b71b7d7fd74921
    concurrentqueue 1.0.4 https://github.com/cameron314/concurrentqueue/archive/6dd38b8a1dbaa7863aa907045f32308a56a6ff5d.tar.gz 7d702475761ad37c2e4c288b93514754e5dba2dc8758a5fa29b13808006ea2bc
    gsl 4.2.0 https://github.com/microsoft/GSL/archive/2828399820ef4928cc89b65605dca5dc68efca6e.tar.gz a4e61b4b2383acb8fe398ff11a16fe1719d5d902ac341b078ec95a45b2925bd3
    entt 4.0.0 https://github.com/skypjack/entt/archive/85c6bba014049b5de8fad49d25424df2f1f6a8c1.tar.gz 1160aaa50e9f28dc0e250079d6bc507e1566a9ee6db5f4f968a29d357ef17a00
    glm 1.0.1 https://github.com/g-truc/glm/archive/0af55ccecd98d4e5a8d1fad7de25ba429d60e863.tar.gz e7f187d83523f505eb38dd25d297ea6c0d4ed856d733e808f18253f5a8fa88a0
    type_safe 0.2.4 https://github.com/foonathan/type_safe/archive/1dbea7936a1e389caa692d01bb3f5ba4b6da7d82.tar.gz 7ea2dc897428e434bdc7ed241b2b81c2a84e277e7bdacd797553921b9fd9b819
    ctre 3.8.1 https://github.com/hanickadot/compile-time-regular-expressions/archive/b3d7788b559e34d985c8530c3e0e7260b67505a6.tar.gz 91dcfb8b1f9600f0ceb9d87fcce5308515cb8695263b94b71cc2f7930f719ddf
    pfr 294a497 https://github.com/boostorg/pfr/archive/294a4976bd04829dd204aaf9e9fd30338a5d3199.tar.gz 26c0999c746ad9da2426b7fcbfc1d06582392c8adbfbf42c3151cc54bbcde7cd
    pcg 428802d https://github.com/imneme/pcg-cpp/archive/428802d1a5634f96bcd0705fab379ff0113bcf13.tar.gz 79f23706ed0cbc1bb57ea35d50d0abed66898b8f7b0bcf2aaecdfd234863b060
    leveldb 1.23 https://github.com/google/leveldb/archive/99b3c03b3284f5886f9ef9a4ef703d57373e61be.tar.gz bc87b9bbc5674c91246a89813355e78401759761342cc049e1c3d56350a8a9d1
    rapidjson 297637b https://github.com/Tencent/rapidjson/archive/297637bc0a0f00dc504dbc1a47ebdaa577f8ae19.tar.gz 104f57d1512da80740a3eca7d7b2acee7f749d79dbf15328ad1869c5c962011b
    stb 6b5799b https://github.com/nothings/stb/archive/6b5799bae8a1fe3792691ef4b72ae26434c3e799.tar.gz 87e9ecd2d2e0cb8fa35bf5a93ea6ca3999595ea4f0a4738ed1aac0a34cbd4da9
    googletest 1.17.0 https://github.com/google/googletest/archive/refs/tags/v1.17.0.tar.gz 65fab701d9829d38cb77c14acdc431d2108bfdbf8979e40eb8ae567edf10b27c
LIST
EOS

# googletest and libFuzzer, built once against libc++ (static archives, uninstrumented, DWARF 3).
# clang's stock libFuzzer runtime is libstdc++-built and cannot share a process with libc++abi
# exception handling (see build.sh), so libFuzzer comes from compiler-rt 19.1.7 = the base's clang.
ARG CRT_SHA256=c12b6e764202c615c1a3af9a13d477846878757ae0e29e5f8979215a6958fffc
RUN set -eu; \
    g=/opt/toolchains/deps/googletest-1.17.0; mkdir -p $g/lib $g/include /tmp/gb; cd /tmp/gb; \
    clang++ -std=c++17 -stdlib=libc++ -O1 -g -gdwarf-3 -I$g/src/googletest/include -I$g/src/googletest \
        -c $g/src/googletest/src/gtest-all.cc -o gtest-all.o; \
    clang++ -std=c++17 -stdlib=libc++ -O1 -g -gdwarf-3 -I$g/src/googletest/include \
        -c $g/src/googletest/src/gtest_main.cc -o gtest_main.o; \
    ar rcs $g/lib/libgtest.a gtest-all.o; ar rcs $g/lib/libgtest_main.a gtest_main.o; \
    cp -r $g/src/googletest/include/gtest $g/include/; \
    f=/opt/toolchains/deps/libfuzzer-19.1.7; mkdir -p $f/src $f/lib /tmp/fb; \
    curl -fsSL --retry 3 -o /tmp/crt.tar.xz \
        https://github.com/llvm/llvm-project/releases/download/llvmorg-19.1.7/compiler-rt-19.1.7.src.tar.xz; \
    echo "$CRT_SHA256  /tmp/crt.tar.xz" | sha256sum -c -; \
    tar -xJf /tmp/crt.tar.xz --strip-components=3 -C $f/src compiler-rt-19.1.7.src/lib/fuzzer; \
    cd /tmp/fb; for s in $f/src/Fuzzer*.cpp; do case $s in *Interceptors*) continue;; esac; \
      clang++ -stdlib=libc++ -std=c++17 -O2 -g -gdwarf-3 -fno-omit-frame-pointer -c $s -o $(basename $s .cpp).o; done; \
    ar rcs $f/lib/libFuzzer.a /tmp/fb/*.o; \
    rm -rf /tmp/gb /tmp/fb /tmp/crt.tar.xz; chmod -R a+rX /opt/toolchains/deps
USER mayhem

USER mayhem
WORKDIR /mayhem
COPY --chown=2000:2000 . /mayhem

RUN if [ -x /opt/mayhem/ccache/prefill ]; then /opt/mayhem/ccache/prefill bash mayhem/build.sh; \
    else bash mayhem/build.sh; fi

RUN if [ -f mayhem/test.sh ]; then bash mayhem/test.sh; else echo "no mayhem/test.sh"; fi

LABEL org.opencontainers.image.source="${REPO_URL}" \
      org.opencontainers.image.revision="${COMMIT}" \
      rlenv.tier="commit" \
      rlenv.source_date_epoch="${SOURCE_DATE_EPOCH}" \
      rlenv.user="mayhem"

USER mayhem
WORKDIR /mayhem
