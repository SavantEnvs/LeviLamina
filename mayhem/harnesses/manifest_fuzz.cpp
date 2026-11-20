// Fuzzes the mod-manifest path of ll::mod::ModRegistrar::loadManifest: a JSON document
// (comments allowed, as loadManifest parses them) deserialized by LeviLamina's reflection
// deserializer into ll::mod::Manifest, then fed to the dependency resolver
// ll::mod::ModLoadPlanner (dependency graph, version ranges, conflicts, cycles, loadBefore).
// Input: one manifest object, or an array of manifest objects (a set of installed mods).
#include "ll/api/mod/Manifest.h"
#include "ll/api/reflection/Deserialization.h"
#include "ll/api/reflection/Serialization.h"
#include "ll/core/mod/ModLoadPlanner.h"

#include "nlohmann/json.hpp"

#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    auto json = nlohmann::ordered_json::parse(data, data + size, nullptr, false, true);
    if (json.is_discarded()) return -1;
    if (!json.is_array()) json = nlohmann::ordered_json::array({std::move(json)});

    ll::DenseMap<std::string, ll::mod::Manifest> ms;
    ll::DenseSet<std::string>                    roots;
    for (auto& j : json) {
        auto m = ll::reflection::deserialize_to<ll::mod::Manifest>(j);
        if (!m) continue;
        (void)ll::reflection::serialize<nlohmann::ordered_json>(*m);
        if (roots.size() < 2) roots.insert(m->name);
        ms.try_emplace(m->name, std::move(*m));
    }
    for (auto& [name, m] : ms) {
        (void)ll::mod::ModLoadPlanner::isPlatformCompatible(m, false);
        if (m.dependencies) {
            for (auto& d : *m.dependencies) {
                for (auto& [n2, m2] : ms) (void)ll::mod::ModLoadPlanner::matchesDependency(m2, d);
            }
        }
    }
    (void)ll::mod::ModLoadPlanner::resolve(ms, false);
    (void)ll::mod::ModLoadPlanner::resolve(ms, true);
    (void)ll::mod::ModLoadPlanner::resolve(ms, roots, false);
    return 0;
}
