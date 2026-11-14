// Synthetic behaviour harness for EXACT transformed Core C++ helper.
// This is NOT a live Worldserver or a complete quest/group rights guarantee.
// Generated header /tmp/f14_core_quest_loot_veto.h is extracted from
// #99 + #100 + current #101 Core patchers, not reimplemented here.
#include <array>
#include <cstddef>
#include <map>
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
using uint8 = std::uint8_t;
using uint16 = std::uint16_t;
using uint32 = std::uint32_t;
using int32 = std::int32_t;
constexpr uint16 MAX_QUEST_LOG_SIZE = 25;
constexpr uint8 QUEST_OBJECTIVES_COUNT = 4;
constexpr uint8 MAX_KILL_CREDIT = 2;
enum QuestStatus : uint8 { QUEST_STATUS_NONE = 0, QUEST_STATUS_COMPLETE = 1, QUEST_STATUS_INCOMPLETE = 3 };
struct Quest
{
    int32 RequiredNpcOrGo[QUEST_OBJECTIVES_COUNT]{};
    uint32 RequiredNpcOrGoCount[QUEST_OBJECTIVES_COUNT]{};
};
struct ObjectMgr
{
    std::map<uint32, Quest> templates;
    Quest const* GetQuestTemplate(uint32 id) const
    {
        auto const it = templates.find(id);
        return it == templates.end() ? nullptr : &it->second;
    }
};
static ObjectMgr* sObjectMgr = nullptr;
struct Player;
struct LootStore
{
    bool viewerHasQuestLoot = false;
    bool HaveQuestLootForPlayer(uint32 lootId, Player const* viewer) const
    {
        return lootId == 8181 && viewer && viewerHasQuestLoot;
    }
};
static LootStore LootTemplates_Creature;
struct CreatureTemplate
{
    uint32 KillCredit[MAX_KILL_CREDIT]{};
    uint32 lootid = 0;
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
struct Guardian : Unit {};

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
    uint32 entry = 177;
    CreatureTemplate creatureTemplate{};
    std::vector<std::pair<int, AuraApplication*>> auras;

    bool IsInWorld() const { return inWorld; }
    bool IsAlive() const { return alive; }
    WorldMap* GetMap() const { return map; }
    ObjectGuid GetGUID() const { return guid; }
    uint32 GetEntry() const { return entry; }
    CreatureTemplate const* GetCreatureTemplate() const { return &creatureTemplate; }
    std::uint32_t GetLootRecipientGroupGUID() const { return lootGroup; }
    ObjectGuid GetLootRecipientGUID() const { return lootRecipient; }
    auto const& GetAppliedAuras() const { return auras; }
};
struct Player : Unit
{
    WorldMap* map = nullptr;
    ObjectGuid guid{6, true};
    Pet* pet = nullptr;
    Guardian* guardian = nullptr;
    Unit* charm = nullptr;
    Group* group = nullptr;
    bool inWorld = true;
    bool gm = false;
    std::array<uint32, MAX_QUEST_LOG_SIZE> questSlotIds{};
    std::map<uint32, QuestStatus> questStatuses{};

    uint32 GetQuestSlotQuestId(uint16 slot) const { return questSlotIds.at(slot); }
    QuestStatus GetQuestStatus(uint32 id) const
    {
        auto const it = questStatuses.find(id);
        return it == questStatuses.end() ? QUEST_STATUS_NONE : it->second;
    }

    bool IsInWorld() const { return inWorld; }
    bool IsGameMaster() const { return gm; }
    WorldMap* GetMap() const { return map; }
    ObjectGuid GetGUID() const { return guid; }
    Pet* GetPet() const { return pet; }
    Guardian* GetGuardianPet() const { return guardian; }
    Unit* GetCharm() const { return charm; }
    Group* GetGroup() const { return group; }
};

#include "f14_core_quest_loot_veto.h"

static int passed = 0;
static int failed = 0;
#define CHECK(expr) do { if (expr) ++passed; else { ++failed; \
    std::printf("FAIL at %d: %s\n", __LINE__, #expr); } } while (0)

int main()
{
    ObjectMgr objectMgr;
    sObjectMgr = &objectMgr;
    WorldMap map;
    WorldMap otherMap;
    Player viewer;
    Creature creature;
    viewer.map = &map;
    creature.map = &map;
    Pet pet;
    Guardian guardian;
    Unit charmed;
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

    viewer.charm = &charmed;
    charmed.selection = creature.guid;
    CHECK(F14ViewerHasDirectInteractionVeto(&viewer, &creature));
    charmed.selection = {};
    charmed.victim = &creature;
    CHECK(F14ViewerHasDirectInteractionVeto(&viewer, &creature));
    charmed.victim = nullptr;
    creature.victim = &charmed;
    CHECK(F14ViewerHasDirectInteractionVeto(&viewer, &creature));
    creature.victim = nullptr;
    viewer.charm = nullptr;
    CHECK(!F14ViewerHasDirectInteractionVeto(&viewer, &creature));

    viewer.guardian = &guardian;
    guardian.selection = creature.guid;
    CHECK(F14ViewerHasDirectInteractionVeto(&viewer, &creature));
    guardian.selection = {};
    guardian.victim = &creature;
    CHECK(F14ViewerHasDirectInteractionVeto(&viewer, &creature));
    guardian.victim = nullptr;
    creature.victim = &guardian;
    CHECK(F14ViewerHasDirectInteractionVeto(&viewer, &creature));
    creature.victim = nullptr;
    viewer.guardian = nullptr;
    CHECK(!F14ViewerHasDirectInteractionVeto(&viewer, &creature));

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

    // Real quest-log-shaped synthetic facts: only active unfinished
    // creature/kills, including alternate KillCredit, can veto.
    constexpr uint32 questId = 910;
    viewer.questSlotIds[0] = questId;
    viewer.questStatuses[questId] = QUEST_STATUS_INCOMPLETE;
    CHECK(F14ViewerHasDirectInteractionVeto(&viewer, &creature)); // missing quest template
    Quest quest{};
    objectMgr.templates[questId] = quest;
    CHECK(!F14ViewerHasDirectInteractionVeto(&viewer, &creature)); // no NPC requirement
    objectMgr.templates[questId].RequiredNpcOrGo[0] = 177;
    objectMgr.templates[questId].RequiredNpcOrGoCount[0] = 1;
    CHECK(F14ViewerHasDirectInteractionVeto(&viewer, &creature)); // direct NPC entry
    viewer.questStatuses[questId] = QUEST_STATUS_COMPLETE;
    CHECK(!F14ViewerHasDirectInteractionVeto(&viewer, &creature)); // no longer incomplete
    viewer.questStatuses[questId] = QUEST_STATUS_INCOMPLETE;
    objectMgr.templates[questId].RequiredNpcOrGoCount[0] = 0;
    CHECK(!F14ViewerHasDirectInteractionVeto(&viewer, &creature)); // absent objective count
    objectMgr.templates[questId].RequiredNpcOrGoCount[0] = 1;
    objectMgr.templates[questId].RequiredNpcOrGo[0] = -177;
    CHECK(!F14ViewerHasDirectInteractionVeto(&viewer, &creature)); // GO objective, not NPC
    objectMgr.templates[questId].RequiredNpcOrGo[0] = 178;
    CHECK(!F14ViewerHasDirectInteractionVeto(&viewer, &creature)); // unrelated entry
    creature.creatureTemplate.KillCredit[0] = 178;
    CHECK(F14ViewerHasDirectInteractionVeto(&viewer, &creature)); // credit alias #1
    creature.creatureTemplate.KillCredit[0] = 0;
    creature.creatureTemplate.KillCredit[1] = 178;
    CHECK(F14ViewerHasDirectInteractionVeto(&viewer, &creature)); // credit alias #2
    creature.creatureTemplate.KillCredit[1] = 0;
    CHECK(!F14ViewerHasDirectInteractionVeto(&viewer, &creature));
    // Upper bound of exactly 25 log slots, no unbounded status-map scan.
    viewer.questSlotIds[0] = 0;
    viewer.questSlotIds[24] = questId;
    objectMgr.templates[questId].RequiredNpcOrGo[0] = 177;
    CHECK(F14ViewerHasDirectInteractionVeto(&viewer, &creature));
    viewer.questStatuses[questId] = QUEST_STATUS_COMPLETE;
    CHECK(!F14ViewerHasDirectInteractionVeto(&viewer, &creature));
    viewer.questSlotIds[24] = 0;
    viewer.questStatuses.clear();
    objectMgr.templates.clear();
    CHECK(!F14ViewerHasDirectInteractionVeto(&viewer, &creature));

    // Exact Core LootStore::HaveQuestLootForPlayer call, under a synthetic
    // loot store interface. This validates veto flow, not DB loot content.
    creature.creatureTemplate.lootid = 8181;
    LootTemplates_Creature.viewerHasQuestLoot = true;
    CHECK(F14ViewerHasDirectInteractionVeto(&viewer, &creature));
    LootTemplates_Creature.viewerHasQuestLoot = false;
    CHECK(!F14ViewerHasDirectInteractionVeto(&viewer, &creature));
    creature.creatureTemplate.lootid = 0;
    LootTemplates_Creature.viewerHasQuestLoot = true;
    CHECK(!F14ViewerHasDirectInteractionVeto(&viewer, &creature));
    creature.creatureTemplate.lootid = 8182;
    CHECK(!F14ViewerHasDirectInteractionVeto(&viewer, &creature));
    LootTemplates_Creature.viewerHasQuestLoot = false;
    creature.creatureTemplate.lootid = 0;
    CHECK(!F14ViewerHasDirectInteractionVeto(&viewer, &creature));

    std::printf("F14 exact Core quest item loot veto stub model: %d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
