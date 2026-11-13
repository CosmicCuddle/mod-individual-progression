// Native tests for the pure F14 pair-rights evidence mapping.
// GCC/Clang, C++17/C++20, with and without NDEBUG. Raw inputs here are
// synthetic; nothing touches a Core object, packet or client.
#include "IndividualProgressionF14PairRightsEvidence.h"

#include <cstdint>
#include <cstdio>

namespace E = IndividualProgressionF14PairRightsEvidence;
namespace R = IndividualProgressionF14CurrentPairRightsRisk;

static unsigned g_pass = 0;
static unsigned g_fail = 0;

#define CHECK(cond)                                                              \
    do                                                                           \
    {                                                                            \
        if (cond)                                                                \
            ++g_pass;                                                            \
        else                                                                     \
        {                                                                        \
            ++g_fail;                                                            \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);          \
        }                                                                        \
    } while (0)

static_assert(!E::CanReachCleanTerminalOnCore25(), "Core25 runtime never reaches a clean terminal");
static_assert(!E::IsConcealmentAuthorized(E::Raw{}), "never concealment authority");
static_assert(!R::IsConcealmentAuthorized(R::Facts{}), "model never authorizes");
static_assert(E::ResultCount == 16, "result table matches the model");

namespace
{
    // The best snapshot the runtime collector can produce: every scan done,
    // nothing present, no veto anywhere.
    E::Raw BestRuntime()
    {
        E::Raw r;
        r.observerGatesPassed = true;
        r.viewerPresent = r.creaturePresent = true;
        r.viewerInWorld = r.creatureInWorld = true;
        r.sameMapAndInstance = true;
        r.creatureAlive = true;
        r.eligibilityScanned = true;
        r.viewerIsGameMaster = false;
        r.viewerCharmed = false;
        r.viewerSeerIsSelf = true;
        r.directScanned = true;
        r.ownedScanned = true;
        r.anyControlledUnit = false;
        r.anySummonSlot = false;
        r.anyCharmMinionPetOrCritterGuid = false;
        r.anyViewerVehicleRelation = false;
        r.roleScanned = true;
        r.groupLootScanned = true;
        r.creatureAuras.scanned = true;
        r.viewerAuras.scanned = true;
        return r;
    }

    bool CompleteCount7(R::Facts const& f)
    {
        return f.viewerEligibilityEvidenceComplete && f.creatureEligibilityEvidenceComplete &&
            f.directInteractionEvidenceComplete && f.ownedUnitEvidenceComplete &&
            f.creatureRoleEvidenceComplete && f.groupLootEvidenceComplete && f.auraEvidenceComplete;
    }

    bool HardIncomplete4(R::Facts const& f)
    {
        return !f.pendingSpellEvidenceComplete && !f.serviceInteractionEvidenceComplete &&
            !f.scriptedRewardEvidenceComplete && !f.questEvidenceComplete;
    }

    void TestDefaultsAndBest()
    {
        E::Raw const none{};
        CHECK(E::Classify(none) == R::Result::InvalidOrUnavailablePair);
        R::Facts const nf = E::ToFacts(none);
        CHECK(!nf.viewerEligibilityEvidenceComplete && !nf.directInteractionEvidenceComplete &&
              !nf.ownedUnitEvidenceComplete && !nf.auraEvidenceComplete);

        E::Raw const best = BestRuntime();
        R::Facts const f = E::ToFacts(best);
        CHECK(CompleteCount7(f));
        CHECK(HardIncomplete4(f));
        CHECK(E::Classify(best) == R::Result::EvidenceIncomplete);
        CHECK(R::MustRemainVisible(f));
    }

    void TestPairAndEligibility()
    {
        E::Raw r = BestRuntime();
        r.sameMapAndInstance = false;
        CHECK(E::Classify(r) == R::Result::InvalidOrUnavailablePair);
        r = BestRuntime();
        r.viewerInWorld = false;
        CHECK(E::Classify(r) == R::Result::InvalidOrUnavailablePair);
        r = BestRuntime();
        r.viewerIsGameMaster = true;
        CHECK(E::Classify(r) == R::Result::ViewerNotOrdinaryEligibleHuman);
        r = BestRuntime();
        r.creatureAlive = false;
        CHECK(E::Classify(r) == R::Result::CreatureNotLiveEligibleTarget);
        CHECK(!E::ToFacts(r).creatureEligibilityEvidenceComplete);

        // Eligibility completeness needs the gates, no charm and own seer.
        r = BestRuntime();
        r.observerGatesPassed = false;
        CHECK(!E::ToFacts(r).viewerEligibilityEvidenceComplete);
        CHECK(!E::ToFacts(r).creatureEligibilityEvidenceComplete);
        r = BestRuntime();
        r.viewerCharmed = true;
        CHECK(!E::ToFacts(r).viewerEligibilityEvidenceComplete);
        r = BestRuntime();
        r.viewerSeerIsSelf = false; // far sight / possession
        CHECK(!E::ToFacts(r).viewerEligibilityEvidenceComplete);
        r = BestRuntime();
        r.eligibilityScanned = false;
        CHECK(!E::ToFacts(r).viewerEligibilityEvidenceComplete);
    }

    void TestDirectInteraction()
    {
        bool E::Raw::*fields[] = {&E::Raw::viewerTargetIsCreature, &E::Raw::viewerVictimIsCreature,
                                  &E::Raw::creatureVictimIsViewer, &E::Raw::creatureThreatenedByViewer,
                                  &E::Raw::creatureInCombatWithViewer, &E::Raw::viewerInCombatWithCreature};
        for (auto field : fields)
        {
            E::Raw r = BestRuntime();
            r.*field = true;
            CHECK(E::Classify(r) == R::Result::DirectViewerInteraction);
        }
        E::Raw r = BestRuntime();
        r.directScanned = false;
        CHECK(!E::ToFacts(r).directInteractionEvidenceComplete);
        CHECK(E::Classify(r) == R::Result::EvidenceIncomplete);
    }

    void TestOwnedUnitsPresenceOnly()
    {
        bool E::Raw::*fields[] = {&E::Raw::anyControlledUnit, &E::Raw::anySummonSlot,
                                  &E::Raw::anyCharmMinionPetOrCritterGuid, &E::Raw::anyViewerVehicleRelation};
        for (auto field : fields)
        {
            E::Raw r = BestRuntime();
            r.*field = true;
            R::Facts const f = E::ToFacts(r);
            CHECK(!f.ownedUnitEvidenceComplete);
            // Never dereferenced, so no interaction veto is invented either.
            CHECK(!f.ownedUnitTargetsOrAttacksCreature && !f.creatureAttacksOwnedUnit &&
                  !f.ownedUnitOnCreatureThreatOrCombatReferences);
            CHECK(E::Classify(r) == R::Result::EvidenceIncomplete);
        }
        E::Raw r = BestRuntime();
        r.ownedScanned = false;
        CHECK(!E::ToFacts(r).ownedUnitEvidenceComplete);
        // Defaults assume presence.
        E::Raw d{};
        d.ownedScanned = true;
        CHECK(!E::ToFacts(d).ownedUnitEvidenceComplete);
    }

    void TestCreatureRoleAndGroupLoot()
    {
        bool E::Raw::*role[] = {&E::Raw::creatureHasOwnerCharmerOrCreator, &E::Raw::creatureControlledByPlayer,
                                &E::Raw::creatureIsSummonPetTotemOrGuardian, &E::Raw::creatureHasVehicleRelation};
        for (auto field : role)
        {
            E::Raw r = BestRuntime();
            r.*field = true;
            CHECK(E::Classify(r) == R::Result::CreatureOwnershipOrVehicleInterest);
        }
        bool E::Raw::*group[] = {&E::Raw::viewerInGroup, &E::Raw::lootRecipientIsViewer,
                                 &E::Raw::lootRecipientGroupSet};
        for (auto field : group)
        {
            E::Raw r = BestRuntime();
            r.*field = true;
            CHECK(E::Classify(r) == R::Result::GroupOrLootInterest);
        }
        E::Raw r = BestRuntime();
        r.roleScanned = false;
        CHECK(!E::ToFacts(r).creatureRoleEvidenceComplete);
        r = BestRuntime();
        r.groupLootScanned = false;
        CHECK(!E::ToFacts(r).groupLootEvidenceComplete);
    }

    void TestAuras()
    {
        auto withCreature = [](std::uint32_t self, std::uint32_t target, std::uint32_t player, std::uint32_t unresolved) {
            E::Raw r = BestRuntime();
            r.creatureAuras.castBySelf = self;
            r.creatureAuras.castByTargetCreature = target;
            r.creatureAuras.castByPlayerOrPet = player;
            r.creatureAuras.castByUnresolved = unresolved;
            r.creatureAuras.total = self + target + player + unresolved;
            return r;
        };
        auto withViewer = [](std::uint32_t self, std::uint32_t target, std::uint32_t player, std::uint32_t unresolved) {
            E::Raw r = BestRuntime();
            r.viewerAuras.castBySelf = self;
            r.viewerAuras.castByTargetCreature = target;
            r.viewerAuras.castByPlayerOrPet = player;
            r.viewerAuras.castByUnresolved = unresolved;
            r.viewerAuras.total = self + target + player + unresolved;
            return r;
        };

        // Creature self-buffs and the viewer's own / other players' auras are fine.
        CHECK(E::ToFacts(withCreature(5, 0, 0, 0)).auraEvidenceComplete);
        CHECK(E::ToFacts(withViewer(3, 0, 4, 0)).auraEvidenceComplete);
        CHECK(E::Classify(withCreature(5, 0, 0, 0)) == R::Result::EvidenceIncomplete);

        // Player/pet-cast aura on the creature.
        CHECK(E::Classify(withCreature(1, 0, 1, 0)) == R::Result::AuraEvidenceUncertainOrPlayerCast);
        // Unresolved caster on the creature (other NPC, GO, dynobj, empty).
        CHECK(E::Classify(withCreature(0, 0, 0, 1)) == R::Result::AuraEvidenceUncertainOrPlayerCast);
        // Creature aura classified as "target" is impossible: treated as unknown.
        CHECK(E::Classify(withCreature(0, 1, 0, 0)) == R::Result::AuraEvidenceUncertainOrPlayerCast);
        // Creature-cast aura on the viewer.
        CHECK(E::Classify(withViewer(0, 1, 0, 0)) == R::Result::AuraEvidenceUncertainOrPlayerCast);
        CHECK(E::ToFacts(withViewer(0, 1, 0, 0)).creatureCastAuraOnViewer);
        // Unresolved caster on the viewer (could be the creature's summon).
        CHECK(E::Classify(withViewer(0, 0, 0, 1)) == R::Result::AuraEvidenceUncertainOrPlayerCast);

        // Overflow and inconsistent counts.
        E::Raw r = BestRuntime();
        r.creatureAuras.overflow = true;
        r.creatureAuras.total = E::MaxAurasPerUnit;
        r.creatureAuras.castBySelf = E::MaxAurasPerUnit;
        CHECK(E::Classify(r) == R::Result::AuraEvidenceUncertainOrPlayerCast);
        r = BestRuntime();
        r.viewerAuras.total = 3; // categories sum to 0
        CHECK(E::Classify(r) == R::Result::AuraEvidenceUncertainOrPlayerCast);
        r = BestRuntime();
        r.creatureAuras.total = E::MaxAurasPerUnit + 1;
        r.creatureAuras.castBySelf = E::MaxAurasPerUnit + 1;
        CHECK(E::Classify(r) == R::Result::AuraEvidenceUncertainOrPlayerCast);
        // Not scanned: incomplete.
        r = BestRuntime();
        r.viewerAuras.scanned = false;
        CHECK(!E::ToFacts(r).auraEvidenceComplete);
        CHECK(E::Classify(r) == R::Result::EvidenceIncomplete);
    }

    void TestHardIncompleteFamilies()
    {
        E::Raw r = BestRuntime();
        r.viewerHasAnyCurrentSpell = true;
        CHECK(E::Classify(r) == R::Result::PendingSpellOrChannelInterest);
        r = BestRuntime();
        r.creatureHasNpcFlagsOrService = true;
        CHECK(E::Classify(r) == R::Result::ServiceOrEscortInteraction);
        r = BestRuntime();
        r.viewerCurrentVendorIsCreatureEntry = true;
        CHECK(E::Classify(r) == R::Result::ServiceOrEscortInteraction);
        bool E::Raw::*scripted[] = {&E::Raw::creatureHasScriptOrAI, &E::Raw::creatureOnInstanceableMap,
                                    &E::Raw::creatureInFormation, &E::Raw::creatureIsBoss};
        for (auto field : scripted)
        {
            r = BestRuntime();
            r.*field = true;
            CHECK(E::Classify(r) == R::Result::ScriptedOrRewardInterest);
        }
        r = BestRuntime();
        r.questSlotTemplateMissing = true;
        CHECK(E::Classify(r) == R::Result::ActiveQuestTemplateUncertain);
        r = BestRuntime();
        r.questTargetsCreatureOrKillCredit = true;
        CHECK(E::Classify(r) == R::Result::ActiveQuestKillCreditInterest);
        r = BestRuntime();
        r.questLootForViewer = true;
        CHECK(E::Classify(r) == R::Result::QuestLootInterest);
        CHECK(!E::ToFacts(r).skinningOrScriptedRewardInterest);
    }

    // Omission witness on the model: with no veto, a clean result occurs
    // only when ALL eleven completeness bits are true.
    void TestOmissionWitnessExhaustive()
    {
        unsigned clean = 0;
        for (unsigned mask = 0; mask < (1u << 11); ++mask)
        {
            R::Facts f{};
            f.viewerPresent = f.creaturePresent = f.viewerInWorld = f.creatureInWorld = true;
            f.sameMapAndInstance = f.creatureAlive = true;
            f.viewerEligibilityEvidenceComplete = mask & (1u << 0);
            f.creatureEligibilityEvidenceComplete = mask & (1u << 1);
            f.directInteractionEvidenceComplete = mask & (1u << 2);
            f.ownedUnitEvidenceComplete = mask & (1u << 3);
            f.creatureRoleEvidenceComplete = mask & (1u << 4);
            f.groupLootEvidenceComplete = mask & (1u << 5);
            f.auraEvidenceComplete = mask & (1u << 6);
            f.pendingSpellEvidenceComplete = mask & (1u << 7);
            f.serviceInteractionEvidenceComplete = mask & (1u << 8);
            f.scriptedRewardEvidenceComplete = mask & (1u << 9);
            f.questEvidenceComplete = mask & (1u << 10);
            bool const isClean = R::Evaluate(f) == R::Result::NoKnownVeto_NotAuthority;
            CHECK(isClean == (mask == (1u << 11) - 1));
            clean += isClean;
        }
        CHECK(clean == 1);
    }

    // Runtime-reachability witness: exhaustive over every input that can
    // influence the seven provable completeness bits, with no veto set.
    void TestRuntimeNeverCleanExhaustive()
    {
        unsigned seen = 0;
        for (unsigned mask = 0; mask < (1u << 16); ++mask)
        {
            E::Raw r = BestRuntime();
            r.observerGatesPassed = mask & (1u << 0);
            r.eligibilityScanned = mask & (1u << 1);
            r.viewerCharmed = mask & (1u << 2);
            r.viewerSeerIsSelf = mask & (1u << 3);
            r.directScanned = mask & (1u << 4);
            r.ownedScanned = mask & (1u << 5);
            r.anyControlledUnit = mask & (1u << 6);
            r.anySummonSlot = mask & (1u << 7);
            r.anyCharmMinionPetOrCritterGuid = mask & (1u << 8);
            r.anyViewerVehicleRelation = mask & (1u << 9);
            r.roleScanned = mask & (1u << 10);
            r.groupLootScanned = mask & (1u << 11);
            r.creatureAuras.scanned = mask & (1u << 12);
            r.viewerAuras.scanned = mask & (1u << 13);
            r.creatureAuras.overflow = mask & (1u << 14);
            r.creatureAlive = mask & (1u << 15);
            R::Facts const f = E::ToFacts(r);
            CHECK(HardIncomplete4(f));
            CHECK(E::Classify(r) != R::Result::NoKnownVeto_NotAuthority);
            CHECK(R::MustRemainVisible(f));
            seen += CompleteCount7(f);
        }
        CHECK(seen > 0); // the seven provable bits are reachable together
    }

    // Randomised: no Raw at all can produce a clean terminal or any of the
    // four hard-incomplete bits.
    void TestRuntimeNeverCleanRandomised()
    {
        std::uint64_t s = 0x9e3779b97f4a7c15ULL;
        auto next = [&]() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return s; };
        for (int i = 0; i < 200000; ++i)
        {
            std::uint64_t a = next(), b = next();
            auto bit = [&](int k) { return bool(((k < 64 ? a : b) >> (k % 64)) & 1u); };
            E::Raw r;
            bool* bools[] = {&r.observerGatesPassed, &r.viewerPresent, &r.creaturePresent, &r.viewerInWorld,
                &r.creatureInWorld, &r.sameMapAndInstance, &r.creatureAlive, &r.eligibilityScanned,
                &r.viewerIsGameMaster, &r.viewerCharmed, &r.viewerSeerIsSelf, &r.directScanned,
                &r.viewerTargetIsCreature, &r.viewerVictimIsCreature, &r.creatureVictimIsViewer,
                &r.creatureThreatenedByViewer, &r.creatureInCombatWithViewer, &r.viewerInCombatWithCreature,
                &r.ownedScanned, &r.anyControlledUnit, &r.anySummonSlot, &r.anyCharmMinionPetOrCritterGuid,
                &r.anyViewerVehicleRelation, &r.roleScanned, &r.creatureHasOwnerCharmerOrCreator,
                &r.creatureControlledByPlayer, &r.creatureIsSummonPetTotemOrGuardian,
                &r.creatureHasVehicleRelation, &r.groupLootScanned, &r.viewerInGroup,
                &r.lootRecipientIsViewer, &r.lootRecipientGroupSet, &r.creatureAuras.scanned,
                &r.creatureAuras.overflow, &r.viewerAuras.scanned, &r.viewerAuras.overflow,
                &r.viewerHasAnyCurrentSpell, &r.creatureHasNpcFlagsOrService,
                &r.viewerCurrentVendorIsCreatureEntry, &r.creatureHasScriptOrAI,
                &r.creatureOnInstanceableMap, &r.creatureInFormation, &r.creatureIsBoss,
                &r.questSlotTemplateMissing, &r.questTargetsCreatureOrKillCredit, &r.questLootForViewer};
            int k = 0;
            for (bool* p : bools)
                *p = bit(k++);
            r.creatureAuras.castBySelf = static_cast<std::uint32_t>(next() % 4);
            r.creatureAuras.castByPlayerOrPet = static_cast<std::uint32_t>(next() % 2);
            r.creatureAuras.total = r.creatureAuras.castBySelf + r.creatureAuras.castByPlayerOrPet;
            r.viewerAuras.castByUnresolved = static_cast<std::uint32_t>(next() % 2);
            r.viewerAuras.total = r.viewerAuras.castByUnresolved;
            R::Facts const f = E::ToFacts(r);
            CHECK(HardIncomplete4(f));
            CHECK(E::Classify(r) != R::Result::NoKnownVeto_NotAuthority);
        }
    }

    void TestResultNames()
    {
        for (std::uint32_t i = 0; i < E::ResultCount; ++i)
        {
            char const* name = E::ResultName(static_cast<R::Result>(i));
            CHECK(name != nullptr && name[0] != 'U');
        }
    }
}

int main()
{
    TestDefaultsAndBest();
    TestPairAndEligibility();
    TestDirectInteraction();
    TestOwnedUnitsPresenceOnly();
    TestCreatureRoleAndGroupLoot();
    TestAuras();
    TestHardIncompleteFamilies();
    TestOmissionWitnessExhaustive();
    TestRuntimeNeverCleanExhaustive();
    TestRuntimeNeverCleanRandomised();
    TestResultNames();
    std::printf("F14 PAIR RIGHTS EVIDENCE %u/%u passed; DIAGNOSTIC ONLY, NO CONCEALMENT AUTHORITY\n",
                g_pass, g_pass + g_fail);
    return g_fail == 0 ? 0 : 1;
}
