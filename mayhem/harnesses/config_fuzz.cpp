// Fuzzes ll::config::loadConfig — how a mod reads its config.json: JSON parse (comments
// allowed), version check + defaultConfigUpdater merge_patch, then the reflection
// deserializer (Deserialization.h) over a config struct covering the field kinds it supports
// (nested aggregates, enums by name, optionals, vectors, maps, sets, tuples, pairs, arrays,
// Version), and finally serialize<ordered_json> back, like saveConfig.
// loadConfig reports bad input by throwing (ParseType / Expected::value()); those are caught.
#include "ll/api/Config.h"
#include "ll/api/data/Version.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <unistd.h>
#include <vector>

namespace ll_fuzz {
enum class Mode { Off, Lazy, Eager };
struct Inner {
    int         hello = 233;
    float       trs   = 0.2f;
    std::string name  = "x";
};
struct Entry {
    std::string typo          = "hellotype";
    int64_t     I_am_a_number = 12415276547;
};
struct FuzzConfig {
    int                                version = 2;
    ll::data::Version                  ver{1, 2, 3};
    Inner                              structure;
    bool                               someFlag = false;
    std::string                        str      = R"(\1hellow)";
    uint8_t                            small    = 7;
    int16_t                            shortv   = -3;
    double                             dbl      = 1.5;
    Mode                               mode     = Mode::Lazy;
    std::map<std::string, Entry>       amap     = {{"key1", {}}, {"key2", {"a new thing", 42}}};
    std::map<int, std::string>         imap     = {{1, "one"}};
    std::tuple<int, bool, float>       tuple{};
    std::pair<std::string, Entry>      pair{};
    std::array<int, 5>                 array{};
    std::vector<short>                 vec{};
    std::optional<std::vector<float>>  ovec = std::vector<float>{0, 1};
    std::optional<Inner>               oinner{};
    std::set<std::string>              sset{"a", "b"};
    std::vector<std::vector<Entry>>    nested{};
};
} // namespace ll_fuzz
using namespace ll_fuzz;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    const char* tmp  = getenv("TMPDIR");
    std::string path = std::string(tmp && *tmp ? tmp : "/tmp") + "/ll_config_XXXXXX";
    int         fd   = mkstemp(path.data());
    if (fd < 0) return 0;
    size_t off = 0;
    while (off < size) {
        ssize_t w = write(fd, data + off, size - off);
        if (w <= 0) break;
        off += static_cast<size_t>(w);
    }
    close(fd);

    FuzzConfig cfg;
    try {
        (void)ll::config::loadConfig(cfg, path);
        (void)ll::reflection::serialize<nlohmann::ordered_json>(cfg).value().dump(4);
    } catch (...) {
    }
    unlink(path.c_str());
    return 0;
}
