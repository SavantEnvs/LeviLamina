// Fuzzes ll::compress_utils (CompressUtils.cpp): the streaming Decompressor and the one-shot
// decompress() over gzip and zlib streams. Byte 0 picks the format and the chunk size the
// stream is fed in; the rest is the compressed stream.
#include "ll/api/utils/CompressUtils.h"

#include <algorithm>
#include <cstdint>
#include <string>

using namespace ll::compress_utils;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1) return -1;
    auto             format = (data[0] & 1) ? CompressFormat::Zlib : CompressFormat::Gzip;
    size_t           chunk  = 1 + ((data[0] >> 1) * 37);
    std::string_view in{reinterpret_cast<const char*>(data + 1), size - 1};

    std::string  out;
    Decompressor d(
        [&](std::string_view s) -> ll::Expected<> {
            if (out.size() + s.size() > (1u << 22)) return ll::makeStringError("too large");
            out.append(s);
            return {};
        },
        format
    );
    bool ok = static_cast<bool>(d.status());
    for (size_t i = 0; ok && i < in.size(); i += chunk) {
        ok = static_cast<bool>(d.write(in.substr(i, std::min(chunk, in.size() - i))));
    }
    if (ok) (void)d.finish();

    auto one = decompress(in, format, 1u << 22);
    if (one && ok) (void)(one->size() == out.size());
    return 0;
}
