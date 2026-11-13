/*
 * CosmicCuddle F14 pair-rights Core25 COLLECTOR. EXPERIMENTAL, DEFAULT OFF.
 *
 * Included only by IndividualProgressionF14CurrentCoreLifecycle.cpp under
 * COSMIC_IP_F14_PAIR_RIGHTS_ADAPTER_EXPERIMENT. It is called synchronously
 * from the token-bound initial-OUT observer, after its existing gates, inside
 * the Core VisibleNotifier frame that is itself reading these two objects.
 *
 * Read rules:
 *   - only the viewer's and the creature's OWN state, through const
 *     accessors, plus static quest/loot stores;
 *   - controlled units, summon slots, vehicle base/passengers and aura
 *     casters are NEVER dereferenced or looked up (presence/GUID only);
 *   - no Auth/character DB, no Playerbots, no group member iteration;
 *   - every loop has a compile-time or explicit bound;
 *   - no pointer outlives the call; nothing is written to Core state.
 *
 * Output is diagnostic only: the observer still returns FALSE.
 */
#ifndef INDIVIDUAL_PROGRESSION_F14_PAIR_RIGHTS_CORE_ADAPTER_H
#define INDIVIDUAL_PROGRESSION_F14_PAIR_RIGHTS_CORE_ADAPTER_H

#include "IndividualProgressionF14PairRightsEvidence.h"
#include "Creature.h"
#include "CreatureData.h"
#include "LootMgr.h"
#include "Map.h"
#include "ObjectGuid.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "QuestDef.h"
#include "SpellAuras.h"
#include "TemporarySummon.h"
#include "WorldSession.h"

namespace IndividualProgressionF14PairRightsCoreAdapter
{
    namespace Evidence = IndividualProgressionF14PairRightsEvidence;

    // Bounded scan of one unit's applied auras. Casters are classified by
    // GUID only; the caster object is never resolved.
    inline Evidence::AuraScan ScanAppliedAuras(Unit const* owner, ObjectGuid const& targetCreature)
    {
        Evidence::AuraScan out{};
        if (!owner)
            return out;
        ObjectGuid const self = owner->GetGUID();
        for (auto const& entry : owner->GetAppliedAuras())
        {
            if (out.total == Evidence::MaxAurasPerUnit)
            {
                out.overflow = true;
                break;
            }
            ++out.total;
            AuraApplication const* app = entry.second;
            Aura const* base = app ? app->GetBase() : nullptr;
            ObjectGuid const caster = base ? base->GetCasterGUID() : ObjectGuid::Empty;
            if (!caster.IsEmpty() && caster == self)
                ++out.castBySelf;
            else if (!caster.IsEmpty() && caster == targetCreature)
                ++out.castByTargetCreature;
            else if (caster.IsPlayer() || caster.IsPet())
                ++out.castByPlayerOrPet;
            else
                ++out.castByUnresolved;
        }
        out.scanned = true;
        return out;
    }

    inline bool LootStoreHasQuestLoot(LootStore const& store, uint32 lootId, Player const* viewer)
    {
        return lootId && store.HaveQuestLootForPlayer(lootId, viewer);
    }

    // observerGatesPassed: the caller's existing exact-identity gates held.
    inline Evidence::Raw Collect(Player const* viewer, Creature const* creature, bool observerGatesPassed)
    {
        Evidence::Raw r{};
        r.observerGatesPassed = observerGatesPassed;
        r.viewerPresent = viewer != nullptr;
        r.creaturePresent = creature != nullptr;
        if (!viewer || !creature)
            return r;

        r.viewerInWorld = viewer->IsInWorld();
        r.creatureInWorld = creature->IsInWorld();
        if (!r.viewerInWorld || !r.creatureInWorld)
            return r;
        r.sameMapAndInstance = viewer->GetMap() == creature->GetMap() &&
            viewer->GetMapId() == creature->GetMapId() &&
            viewer->GetInstanceId() == creature->GetInstanceId();
        r.creatureAlive = creature->IsAlive();
        if (!r.sameMapAndInstance)
            return r;

        ObjectGuid const creatureGuid = creature->GetGUID();
        CreatureTemplate const* tmpl = creature->GetCreatureTemplate();
        uint32 const entry = creature->GetEntry();

        // Viewer eligibility.
        r.viewerIsGameMaster = viewer->IsGameMaster();
        r.viewerCharmed = !viewer->GetCharmerGUID().IsEmpty();
        r.viewerSeerIsSelf = viewer->GetSeer() == static_cast<WorldObject const*>(viewer);
        r.eligibilityScanned = true;

        // Direct interaction.
        r.viewerTargetIsCreature = viewer->GetTarget() == creatureGuid;
        r.viewerVictimIsCreature = viewer->GetVictim() == creature;
        r.creatureVictimIsViewer = creature->GetVictim() == viewer;
        r.creatureThreatenedByViewer = creature->IsThreatenedBy(viewer);
        r.creatureInCombatWithViewer = creature->IsInCombatWith(viewer);
        r.viewerInCombatWithCreature = viewer->IsInCombatWith(creature);
        r.directScanned = true;

        // Player-owned units: presence only.
        r.anyControlledUnit = !viewer->m_Controlled.empty();
        bool anySlot = false;
        for (uint32 slot = 0; slot < MAX_SUMMON_SLOT; ++slot)
            anySlot = anySlot || !viewer->m_SummonSlot[slot].IsEmpty();
        r.anySummonSlot = anySlot;
        r.anyCharmMinionPetOrCritterGuid = !viewer->GetCharmGUID().IsEmpty() ||
            !viewer->GetMinionGUID().IsEmpty() || !viewer->GetPetGUID().IsEmpty() ||
            !viewer->GetCritterGUID().IsEmpty();
        r.anyViewerVehicleRelation = viewer->GetVehicle() != nullptr ||
            viewer->GetVehicleKit() != nullptr;
        r.ownedScanned = true;

        // Creature role: its own fields only (no passenger dereference).
        TempSummon const* summon = creature->ToTempSummon();
        r.creatureHasOwnerCharmerOrCreator = !creature->GetOwnerGUID().IsEmpty() ||
            !creature->GetCharmerGUID().IsEmpty() || !creature->GetCreatorGUID().IsEmpty();
        r.creatureControlledByPlayer = creature->IsControlledByPlayer();
        r.creatureIsSummonPetTotemOrGuardian = creature->IsPet() || creature->IsTotem() ||
            creature->IsGuardian() || (summon && !summon->GetSummonerGUID().IsEmpty());
        r.creatureHasVehicleRelation = creature->GetVehicleKit() != nullptr ||
            creature->GetVehicle() != nullptr;
        r.roleScanned = true;

        // Group / loot.
        r.viewerInGroup = viewer->GetGroup() != nullptr;
        r.lootRecipientIsViewer = creature->GetLootRecipientGUID() == viewer->GetGUID();
        r.lootRecipientGroupSet = creature->GetLootRecipientGroupGUID() != 0;
        r.groupLootScanned = true;

        // Auras on both units, bounded.
        r.creatureAuras = ScanAppliedAuras(creature, creatureGuid);
        r.viewerAuras = ScanAppliedAuras(viewer, creatureGuid);

        // Pending spells: any current spell slot, target ignored.
        bool anySpell = false;
        for (uint32 type = 0; type < CURRENT_MAX_SPELL; ++type)
            anySpell = anySpell || viewer->GetCurrentSpell(CurrentSpellTypes(type)) != nullptr;
        r.viewerHasAnyCurrentSpell = anySpell;

        // Service capability and active vendor entry.
        r.creatureHasNpcFlagsOrService = creature->GetNpcFlags() != UNIT_NPC_FLAG_NONE ||
            creature->IsServiceProvider();
        WorldSession const* session = viewer->GetSession();
        r.viewerCurrentVendorIsCreatureEntry = session && session->GetCurrentVendor() == entry;

        // Scripted / encounter vetoes.
        r.creatureHasScriptOrAI = creature->GetScriptId() != 0 || !creature->GetAIName().empty();
        Map const* map = creature->GetMap();
        r.creatureOnInstanceableMap = map && (map->Instanceable() || map->IsDungeon());
        r.creatureInFormation = creature->GetFormation() != nullptr;
        r.creatureIsBoss = creature->isWorldBoss() || creature->IsDungeonBoss();

        // Quests: bounded quest log, static templates.
        bool missing = false;
        bool targets = false;
        for (uint16 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
        {
            uint32 const questId = viewer->GetQuestSlotQuestId(slot);
            if (!questId)
                continue;
            Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
            if (!quest)
            {
                missing = true;
                continue;
            }
            if (viewer->GetQuestStatus(questId) != QUEST_STATUS_INCOMPLETE)
                continue;
            for (uint32 i = 0; i < QUEST_OBJECTIVES_COUNT; ++i)
            {
                int32 const required = quest->RequiredNpcOrGo[i];
                if (required <= 0)
                    continue;
                uint32 const requiredEntry = uint32(required);
                bool killCredit = false;
                if (tmpl)
                    for (uint32 k = 0; k < MAX_KILL_CREDIT; ++k)
                        killCredit = killCredit || (tmpl->KillCredit[k] && tmpl->KillCredit[k] == requiredEntry);
                targets = targets || requiredEntry == entry || killCredit;
            }
        }
        r.questSlotTemplateMissing = missing;
        r.questTargetsCreatureOrKillCredit = targets;
        r.questLootForViewer = tmpl && (
            LootStoreHasQuestLoot(LootTemplates_Creature, tmpl->lootid, viewer) ||
            LootStoreHasQuestLoot(LootTemplates_Pickpocketing, tmpl->pickpocketLootId, viewer) ||
            LootStoreHasQuestLoot(LootTemplates_Skinning, tmpl->SkinLootId, viewer));

        return r;
    }
}

#endif
