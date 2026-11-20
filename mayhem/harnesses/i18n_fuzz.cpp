// Fuzzes ll::i18n::I18n::load (a mod's language file: JSON of locale -> nested key/value
// objects, flattened by addNestedData) and the locale-fallback lookup in I18n::get.
// The input is written to a scratch file under $TMPDIR (load() takes a path), loaded into a
// fresh I18n, and every flattened key is looked up under several locale codes.
#include "ll/api/i18n/I18n.h"

#include "nlohmann/json.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>

static void collectKeys(nlohmann::json const& j, std::string const& prefix, std::vector<std::string>& out) {
    if (!j.is_object() || out.size() > 64) return;
    for (auto& [k, v] : j.items()) {
        if (v.is_object()) collectKeys(v, prefix + k + '.', out);
        else out.push_back(prefix + k);
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    const char* tmp = getenv("TMPDIR");
    std::string path = std::string(tmp && *tmp ? tmp : "/tmp") + "/ll_i18n_XXXXXX";
    int fd = mkstemp(path.data());
    if (fd < 0) return 0;
    size_t off = 0;
    while (off < size) {
        ssize_t w = write(fd, data + off, size - off);
        if (w <= 0) break;
        off += static_cast<size_t>(w);
    }
    close(fd);

    ll::i18n::I18n i18n;
    auto           res = i18n.load(path);
    unlink(path.c_str());
    if (!res) {
        (void)res.error().message();
        return 0;
    }
    std::vector<std::string> keys{"", "a", "a.b"};
    auto j = nlohmann::json::parse(data, data + size, nullptr, false);
    if (!j.is_discarded() && j.is_object()) {
        for (auto& [code, v] : j.items()) {
            collectKeys(v, "", keys);
            if (keys.size() > 64) break;
        }
    }
    static const char* const locales[] = {"", "en_US", "en-us", "zh", "zh_CN", "-", "_", "x-"};
    for (auto& k : keys)
        for (auto* l : locales) (void)i18n.get(k, l);
    return 0;
}
