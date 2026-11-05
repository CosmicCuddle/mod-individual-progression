// Synthetic behaviour harness for EXACT transformed Core C++ helper.
// This is NOT a live Worldserver or a complete quest/group rights guarantee.
// Generated header /tmp/f14_core_group_aura_veto.h is extracted from the
// #99 + current CI patchers, not reimplemented here.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <utility>
#include <vector>

struct ObjectGuid
{
    std::uint64_t raw = 0;
    bool player = false;
    bool operator==(ObjectGuid const& other) const
    {
        return raw == other.raw && player == other.player;
    }
    explicit operator bool() const { return raw != 0; }
    bool IsPlayer() const { return player && raw != 0; }
};
struct WorldMap {};
struct Group {};

struct Unit
{
    ObjectGuid selection{};
    Unit const* victim = nullptr;
    ObjectGuid GetTarget() const { return selection; }
    Unit const* GetVictim() const { return victim; }
};
struct Pet : Unit {};

struct Aura
{
    ObjectGuid caster{};
    ObjectGuid GetCasterGUID() const { return caster; }
};
struct AuraApplication
{
    Aura const* base = nullptr;
    Aura const* GetBase() const { return base; }
};
struct Creature : Unit
{
    WorldMap* map = nullptr;
    ObjectGuid guid{91, false};
    ObjectGuid lootRecipient{};
    std::uint32_t lootGroup = 0;
    bool inWorld = true;
    bool alive = true;
    std::vector<std::pair<int, AuraApplication*>> auras;

    bool IsInWorld() const { return inWorld; }
    bool IsAlive() const { return alive; }
    WorldMap* GetMap() const { return map; }
    ObjectGuid GetGUID() const { return guid; }
    std::uint32_t GetLootRecipientGroupGUID() const { return lootGroup; }
    ObjectGuid GetLootRecipientGUID() const { return lootRecipient; }
    auto const& GetAppliedAuras() const { return auras; }
};
struct Player : Unit
{
    WorldMap* map = nullptr;
    ObjectGuid guid{6, true};
    Pet* pet = nullptr;
    Group* group = nullptr;
    bool inWorld = true;
    bool gm = false;

    bool IsInWorld() const { return inWorld; }
    bool IsGameMaster() const { return gm; }
    WorldMap* GetMap() const { return map; }
    ObjectGuid GetGUID() const { return guid; }
    Pet* GetPet() const { return pet; }
    Group* GetGroup() const { return group; }
};

#include "f14_core_group_aura_veto.h"

static int passed = 0;
static int failed = 0;
#define CHECK(expr) do { if (expr) ++passed; else { ++failed; \
    std::printf("FAIL at %d: %s\n", __LINE__, #expr); } } while (0)

int main()
{
    WorldMap map;
    WorldMap otherMap;
    Player viewer;
    Creature creature;
    viewer.map = &map;
    creature.map = &map;
    Pet pet;
    Group group;
    Aura npcAura{{4, false}};
    Aura playerAura{{5, true}};
    Aura botAura{{6, true}}; // same client type, conservative veto
    Aura unknownAura{{0, false}};
    AuraApplication appliedNpc{&npcAura};
    AuraApplication appliedPlayer{&playerAura};
    AuraApplication appliedBot{&botAura};
    AuraApplication appliedUnknown{&unknownAura};
    AuraApplication appliedNull{};

    CHECK(!F14ViewerHasDirectInteractionVeto(&viewer, &creature));
    CHECK(F14ViewerHasDirectInteractionVeto(nullptr, &creature));
    CHECK(F14ViewerHasDirectInteractionVeto(&viewer, nullptr));

    viewer.inWorld = false;
    CHECK(F14ViewerHasDirectInteractionVeto(&viewer, &creature));
    viewer.inWorld = true;
    creature.inWorld = false;
    CHECK(F14ViewerHasDirectInteractionVeto(&viewer, &creature));
    creature.inWorld = true;
    viewer.map = &otherMap;
    CHECK(F14ViewerHasDirectInteractionVeto(&viewer, &creature));
    viewer.map = &map;

    viewer.gm = true;
    CHECK(F14ViewerHasDirectInteractionVeto(&viewer, &creature));
    viewer.gm = false;
    creature.alive = false;
    CHECK(F14ViewerHasDirectInteractionVeto(&viewer, &creature));
    creature.alive = true;

    viewer.selection = creature.guid;
    CHECK(F14ViewerHasDirectInteractionVeto(&viewer, &creature));
    viewer.selection = {};
    viewer.victim = &creature;
    CHECK(F14ViewerHasDirectInteractionVeto(&viewer, &creature));
    viewer.victim = nullptr;
    creature.victim = &viewer;
    CHECK(F14ViewerHasDirectInteractionVeto(&viewer, &creature));
    creature.victim = nullptr;

    creature.lootRecipient = viewer.guid;
    CHECK(F14ViewerHasDirectInteractionVeto(&viewer, &creature));
    creature.lootRecipient = {};
    creature.lootGroup = 17;
    CHECK(F14ViewerHasDirectInteractionVeto(&viewer, &creature));
    creature.lootGroup = 0;

    viewer.pet = &pet;
    pet.selection = creature.guid;
    CHECK(F14ViewerHasDirectInteractionVeto(&viewer, &creature));
    pet.selection = {};
    pet.victim = &creature;
    CHECK(F14ViewerHasDirectInteractionVeto(&viewer, &creature));
    pet.victim = nullptr;
    creature.victim = &pet;
    CHECK(F14ViewerHasDirectInteractionVeto(&viewer, &creature));
    creature.victim = nullptr;
    viewer.pet = nullptr;

    viewer.group = &group;
    CHECK(F14ViewerHasDirectInteractionVeto(&viewer, &creature));
    viewer.group = nullptr;
    CHECK(!F14ViewerHasDirectInteractionVeto(&viewer, &creature));

    creature.auras.push_back({1, &appliedPlayer});
    CHECK(F14ViewerHasDirectInteractionVeto(&viewer, &creature));
    creature.auras.clear();
    creature.auras.push_back({1, &appliedBot});
    CHECK(F14ViewerHasDirectInteractionVeto(&viewer, &creature));
    creature.auras.clear();
    creature.auras.push_back({1, &appliedUnknown});
    CHECK(F14ViewerHasDirectInteractionVeto(&viewer, &creature));
    creature.auras.clear();
    creature.auras.push_back({1, nullptr});
    CHECK(F14ViewerHasDirectInteractionVeto(&viewer, &creature));
    creature.auras.clear();
    creature.auras.push_back({1, &appliedNull});
    CHECK(F14ViewerHasDirectInteractionVeto(&viewer, &creature));
    creature.auras.clear();
    creature.auras.push_back({1, &appliedNpc});
    CHECK(!F14ViewerHasDirectInteractionVeto(&viewer, &creature));

    creature.auras.resize(64, {1, &appliedNpc});
    CHECK(!F14ViewerHasDirectInteractionVeto(&viewer, &creature));
    creature.auras.push_back({65, &appliedNpc});
    CHECK(F14ViewerHasDirectInteractionVeto(&viewer, &creature));
    creature.auras.clear();
    CHECK(!F14ViewerHasDirectInteractionVeto(&viewer, &creature));

    std::printf("F14 exact Core negative veto stub model: %d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
