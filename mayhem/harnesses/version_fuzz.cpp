// Fuzzes LeviLamina's semver parser (ll::data::Version) and its range parser
// (ll::data::VersionRequirement): the code that reads the "version" and dependency
// "version" fields of every mod manifest.json.
// Input: "<version>\n<requirement>"; with no newline, the whole input is tried as both.
#include "ll/api/data/Version.h"
#include "ll/api/data/VersionRequirement.h"

#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    std::string_view in{reinterpret_cast<const char*>(data), size};
    auto             nl      = in.find('\n');
    std::string_view verStr  = nl == std::string_view::npos ? in : in.substr(0, nl);
    std::string_view reqStr  = nl == std::string_view::npos ? in : in.substr(nl + 1);

    ll::data::Version            v;
    ll::data::VersionRequirement r;
    bool                         vOk = static_cast<bool>(v.from_string_noexcept(verStr));
    bool                         rOk = static_cast<bool>(r.from_string_noexcept(reqStr));
    if (vOk) {
        std::string       s = v.to_string();
        ll::data::Version v2;
        if (v2.from_string_noexcept(s)) {
            (void)(v2 == v);
            (void)(v2 < v);
        }
    }
    if (rOk) {
        std::string                  s = r.to_string();
        ll::data::VersionRequirement r2;
        (void)r2.from_string_noexcept(s);
        if (vOk) (void)r.matches(v);
    }
    return 0;
}
