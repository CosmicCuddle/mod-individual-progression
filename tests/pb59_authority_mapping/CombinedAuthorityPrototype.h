#pragma once
// Test-only PB59 coherent read model; no Player access or runtime registration.
#include <cstdint>
#include <optional>

namespace PB59Prototype
{
struct ProgressionPublication
{
    std::uint64_t guid = 0;
    std::uint64_t session = 0;
    std::uint64_t revision = 0;
    std::uint32_t generation = 0;
    std::uint8_t rawStage = 0;
    std::uint8_t effectiveStage = 0;
    bool gatedHuman = false;
    bool known = false;
    bool operator==(ProgressionPublication const&) const = default;
};
struct EquipmentPublication
{
    std::uint64_t guid = 0;
    std::uint64_t session = 0;
    std::uint64_t revision = 0;
    std::uint32_t level = 0;
    std::uint32_t average = 0;
    bool known = false;
    bool operator==(EquipmentPublication const&) const = default;
};
struct CombinedAuthority
{
    ProgressionPublication progression;
    EquipmentPublication equipment;
    bool operator==(CombinedAuthority const&) const = default;
};

// Each reader must return a linearizable copy for the requested GUID. Both
// publishers share a non-reused session epoch. Revisions never wrap/reuse
// within that epoch, including invalidation and same-value republication.
// Equal progression reads bracket the equipment read: the pair existed at
// that middle read. No lease is held and later changes are diagnostic only.
template<class ReadProgression, class ReadEquipment>
std::optional<CombinedAuthority> ReadCoherent(std::uint64_t guid,
    ReadProgression readProgression, ReadEquipment readEquipment)
{
    if (!guid)
        return std::nullopt;
    auto first = readProgression(guid);
    auto equipment = readEquipment(guid);
    auto last = readProgression(guid);
    if (!first.known || !equipment.known || first != last ||
        first.guid != guid || equipment.guid != guid || !first.session ||
        !first.revision || !first.generation || first.rawStage > 18 ||
        first.effectiveStage > first.rawStage ||
        first.session != equipment.session || !equipment.level || !equipment.average)
        return std::nullopt;
    return CombinedAuthority{first, equipment};
}

template<class ReadProgression, class ReadEquipment>
bool MatchesFinalB(CombinedAuthority const& expected,
    ReadProgression readProgression, ReadEquipment readEquipment)
{
    auto actual = ReadCoherent(expected.progression.guid, readProgression, readEquipment);
    return actual && *actual == expected;
}
}
