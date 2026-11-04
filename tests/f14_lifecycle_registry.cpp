// Native tests for the F14 attack/corpse lifecycle registry (pure header).
// Built with -Werror under GCC/Clang, C++17/C++20, with and without NDEBUG.
// These prove bookkeeping semantics only, not in-game behaviour.
#include "IndividualProgressionF14LifecycleRegistry.h"

#include <cstdint>
#include <cstdio>
#include <vector>

namespace R = IndividualProgressionF14LifecycleRegistry;
namespace P = IndividualProgressionF14MultiAttackerPolicy;
namespace V = IndividualProgressionF14CombatVisibilityPolicy;

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

namespace
{
    constexpr std::uint64_t BotA = 101; // verified login epochs
    constexpr std::uint64_t BotB = 102;
    constexpr std::uint64_t Human = 0;  // any unverified participant

    R::CreatureKey Key(std::uint32_t map, std::uint32_t instance, std::uint64_t guid)
    {
        R::CreatureKey k;
        k.mapId = map;
        k.instanceId = instance;
        k.guidRaw = guid;
        return k;
    }

    std::uint64_t Begin(R::Registry& r, R::CreatureKey const& k, std::uint64_t stamp,
                        std::uint64_t actor, std::vector<std::uint64_t> others = {})
    {
        return r.OnParticipantBegin(k, stamp, actor, others.data(), others.size()).stamp;
    }

    unsigned Active(R::Registry const& r, std::uint64_t stamp)
    {
        R::Entry e;
        return r.Lookup(stamp, e) ? P::ActiveAttackers(e.state) : 999u;
    }

    bool Uncertain(R::Registry const& r, std::uint64_t stamp)
    {
        R::Entry e;
        return r.Lookup(stamp, e) && e.state.uncertain;
    }

    bool Has(R::Registry const& r, std::uint64_t stamp, std::uint64_t actor)
    {
        R::Entry e;
        if (!r.Lookup(stamp, e))
            return false;
        for (auto a : e.state.verifiedAttackers)
            if (a == actor)
                return true;
        return false;
    }

    void TestUnverifiedNeverCreates()
    {
        R::Registry r;
        R::CreatureKey const k = Key(0, 0, 0xF130000001);
        CHECK(Begin(r, k, 0, Human) == 0);
        CHECK(r.Size() == 0);
        r.OnParticipantEnd(k, 0, BotA); // end never creates
        CHECK(r.Size() == 0);
        CHECK(!r.OnCreatureStoppedAttacking(k, 0));
        r.OnCreatureTargetChange(k, 0, false);
        CHECK(r.Size() == 0);
        CHECK(r.GetCounters().ignoredWithoutEntry >= 2);
    }

    void TestVerifiedBeginAndDuplicate()
    {
        R::Registry r;
        R::CreatureKey const k = Key(0, 0, 0xF130000002);
        std::uint64_t const s = Begin(r, k, 0, BotA);
        CHECK(s != 0);
        CHECK(r.Size() == 1);
        CHECK(Active(r, s) == 1);
        CHECK(Has(r, s, BotA));
        CHECK(!Uncertain(r, s));
        // Duplicate begin (same stamp) is not double counted.
        CHECK(Begin(r, k, s, BotA) == s);
        CHECK(Active(r, s) == 1);
        // Event-built rosters never become concealment candidates.
        CHECK(!r.HasParticipantProof(s));
    }

    void TestTwoAttackersLastExit()
    {
        R::Registry r;
        R::CreatureKey const k = Key(1, 0, 0xF130000003);
        std::uint64_t s = Begin(r, k, 0, BotA);
        s = Begin(r, k, s, BotB, {BotA});
        CHECK(Active(r, s) == 2);
        r.OnParticipantEnd(k, s, BotA);
        CHECK(Active(r, s) == 1);
        CHECK(Has(r, s, BotB));
        // Creature stopping while a verified attacker remains: keep.
        CHECK(!r.OnCreatureStoppedAttacking(k, s));
        CHECK(r.Size() == 1);
        r.OnParticipantEnd(k, s, BotB);
        CHECK(Active(r, s) == 0);
        CHECK(!Uncertain(r, s));
        // Last exit: creature stop retires the entry.
        CHECK(r.OnCreatureStoppedAttacking(k, s));
        CHECK(r.Size() == 0);
        CHECK(r.GetCounters().evadeRetired == 1);
    }

    void TestUnknownHumanAndMinion()
    {
        R::Registry r;
        R::CreatureKey const k = Key(0, 0, 0xF130000004);
        std::uint64_t const s = Begin(r, k, 0, BotA);
        // A human (or pet/minion/Altbot: all token 0) joining.
        CHECK(Begin(r, k, s, Human) == s);
        CHECK(Uncertain(r, s));
        CHECK(Has(r, s, BotA));
        // Leaving does not restore certainty.
        r.OnParticipantEnd(k, s, Human);
        CHECK(Uncertain(r, s));
        CHECK(!r.HasParticipantProof(s));
    }

    void TestCreationSnapshot()
    {
        R::Registry r;
        // A human was already attacking before the first verified bot.
        R::CreatureKey const k1 = Key(0, 0, 0xF130000005);
        std::uint64_t const s1 = Begin(r, k1, 0, BotA, {Human});
        CHECK(s1 != 0);
        CHECK(Uncertain(r, s1));
        // Another verified bot already attacking is seeded into the roster.
        R::CreatureKey const k2 = Key(0, 0, 0xF130000006);
        std::uint64_t const s2 = Begin(r, k2, 0, BotA, {BotB});
        CHECK(Active(r, s2) == 2);
        CHECK(!Uncertain(r, s2));
        // Snapshot listing the actor itself is not double counted.
        R::CreatureKey const k3 = Key(0, 0, 0xF130000007);
        std::uint64_t const s3 = Begin(r, k3, 0, BotA, {BotA});
        CHECK(Active(r, s3) == 1);
        // Oversized snapshot (cannot list all participants) is uncertain.
        R::CreatureKey const k4 = Key(0, 0, 0xF130000008);
        std::vector<std::uint64_t> many;
        for (std::uint64_t i = 0; i < P::MaxTrackedAttackers + 1; ++i)
            many.push_back(1000 + i);
        std::uint64_t const s4 = Begin(r, k4, 0, BotA, many);
        CHECK(Uncertain(r, s4));
        // Null snapshot with a nonzero count is uncertain, never trusted.
        R::CreatureKey const k5 = Key(0, 0, 0xF130000009);
        std::uint64_t const s5 = r.OnParticipantBegin(k5, 0, BotA, nullptr, 3).stamp;
        CHECK(Uncertain(r, s5));
    }

    void TestTargetSwitch()
    {
        R::Registry r;
        R::CreatureKey const c1 = Key(0, 0, 0xF13000000A);
        R::CreatureKey const c2 = Key(0, 0, 0xF13000000B);
        std::uint64_t const s1 = Begin(r, c1, 0, BotA);
        // Switch: end on old victim then begin on the new one.
        r.OnParticipantEnd(c1, s1, BotA);
        std::uint64_t const s2 = Begin(r, c2, 0, BotA);
        CHECK(s1 != s2);
        CHECK(Active(r, s1) == 0);
        CHECK(Has(r, s2, BotA));
        CHECK(!Uncertain(r, s1));
        // Creature's own target change: verified victim keeps state,
        // a human/pet/creature victim makes it uncertain.
        r.OnCreatureTargetChange(c2, s2, true);
        CHECK(!Uncertain(r, s2));
        r.OnCreatureTargetChange(c2, s2, false);
        CHECK(Uncertain(r, s2));
    }

    void TestDeathCorpseRespawn()
    {
        R::Registry r;
        R::CreatureKey const k = Key(0, 0, 0xF13000000C);
        std::uint64_t const s = Begin(r, k, 0, BotA);
        // Corpse while the bot is still linked: always retires.
        r.OnCreatureCorpseOrRespawn(k, s, false);
        CHECK(r.Size() == 0);
        CHECK(r.GetCounters().corpseRetired == 1);
        R::Entry e;
        CHECK(!r.Lookup(s, e));
        // A late AttackStop on the dead incarnation is ignored, not revived.
        r.OnParticipantEnd(k, s, BotA);
        CHECK(r.Size() == 0);
        // After respawn (caller cleared stamp) a NEW incarnation is issued.
        std::uint64_t const s2 = Begin(r, k, 0, BotA);
        CHECK(s2 != 0 && s2 != s);
        r.OnCreatureCorpseOrRespawn(k, s2, true);
        CHECK(r.Size() == 0);
        CHECK(r.GetCounters().respawnRetired == 1);
        // Even a caller that forgot to clear the stamp cannot revive it.
        std::uint64_t const s3 = Begin(r, k, s2, BotA);
        CHECK(s3 != s2);
        CHECK(Active(r, s3) == 1);
    }

    void TestLoginExpiry()
    {
        R::Registry r;
        R::CreatureKey const c1 = Key(0, 0, 0xF13000000D);
        R::CreatureKey const c2 = Key(0, 0, 0xF13000000E);
        std::uint64_t const s1 = Begin(r, c1, 0, BotA);
        std::uint64_t s2 = Begin(r, c2, 0, BotA);
        s2 = Begin(r, c2, s2, BotB, {BotA});
        // Disconnect: no AttackStop is ever delivered.
        CHECK(r.ExpireActor(BotA) == 2);
        CHECK(!Has(r, s1, BotA));
        CHECK(!Has(r, s2, BotA));
        CHECK(Has(r, s2, BotB));
        CHECK(Uncertain(r, s1));
        CHECK(Uncertain(r, s2));
        CHECK(r.ExpireActor(BotA) == 0);
        CHECK(r.ExpireActor(0) == 0);
        CHECK(r.GetCounters().expiryFanout == 2);
    }

    void TestInstanceReuseAndMapSafety()
    {
        R::Registry r;
        R::CreatureKey const k = Key(533, 7, 0xF13000000F);
        std::uint64_t const old = Begin(r, k, 0, BotA);
        // Instance 7 unloads without a removal hook; a NEW Creature object
        // reuses map/instance/GUID with no stamp: stale roster is dropped.
        std::uint64_t const fresh = Begin(r, k, 0, BotB);
        CHECK(fresh != old);
        CHECK(r.Size() == 1);
        CHECK(r.GetCounters().staleKeyReplaced == 1);
        CHECK(!Has(r, fresh, BotA));
        R::Entry e;
        CHECK(!r.Lookup(old, e));
        // Same GUID in another instance or map is a different creature.
        std::uint64_t const other = Begin(r, Key(533, 8, 0xF13000000F), 0, BotA);
        std::uint64_t const otherMap = Begin(r, Key(534, 7, 0xF13000000F), 0, BotA);
        CHECK(other != fresh && otherMap != fresh && other != otherMap);
        CHECK(r.Size() == 3);
    }

    void TestStampMismatchPoisons()
    {
        R::Registry r;
        std::uint64_t const s = Begin(r, Key(0, 0, 0xF130000010), 0, BotA);
        // A stamp presented with a different creature key is impossible.
        Begin(r, Key(0, 0, 0xF130000011), s, BotA);
        CHECK(r.IsPoisoned());
        CHECK(r.GetPoisonReason() == R::PoisonReason::StampKeyMismatch);
        CHECK(r.Size() == 0);
        CHECK(!r.HasParticipantProof(s));
        CHECK(Begin(r, Key(0, 0, 0xF130000012), 0, BotA) == 0);
        CHECK(r.ExpireActor(BotA) == 0);
        CHECK(r.GetCounters().poisonedCalls >= 2);
        // Reset lifts poison; old stamps never match new tokens.
        r.Reset();
        CHECK(!r.IsPoisoned());
        std::uint64_t const s2 = Begin(r, Key(0, 0, 0xF130000010), s, BotA);
        CHECK(s2 != 0 && s2 > s);
    }

    void TestCapacityOverflowPoisons()
    {
        R::Registry r(2);
        CHECK(Begin(r, Key(0, 0, 1), 0, BotA) != 0);
        CHECK(Begin(r, Key(0, 0, 2), 0, BotA) != 0);
        CHECK(Begin(r, Key(0, 0, 3), 0, BotA) == 0);
        CHECK(r.IsPoisoned());
        CHECK(r.GetPoisonReason() == R::PoisonReason::CreatureCapacity);
        CHECK(r.Size() == 0);
    }

    void TestAttackerOverflowUncertain()
    {
        R::Registry r;
        R::CreatureKey const k = Key(0, 0, 0xF130000013);
        std::uint64_t s = 0;
        for (std::uint64_t i = 0; i < P::MaxTrackedAttackers; ++i)
            s = Begin(r, k, s, 5000 + i);
        CHECK(Active(r, s) == P::MaxTrackedAttackers);
        CHECK(!Uncertain(r, s));
        s = Begin(r, k, s, 9999);
        CHECK(Uncertain(r, s));
        CHECK(!r.IsPoisoned());
    }

    void TestInvalidKeyPoisons()
    {
        R::Registry r;
        CHECK(Begin(r, Key(0, 0, 0), 0, BotA) == 0);
        CHECK(r.IsPoisoned());
        CHECK(r.GetPoisonReason() == R::PoisonReason::InvalidKey);
    }

    void TestVerifiedEndOfUnknownActor()
    {
        R::Registry r;
        R::CreatureKey const k = Key(0, 0, 0xF130000014);
        std::uint64_t const s = Begin(r, k, 0, BotA);
        // An end for a verified actor that never began: incomplete stream.
        r.OnParticipantEnd(k, s, BotB);
        CHECK(Uncertain(r, s));
        CHECK(Has(r, s, BotA));
    }

    void TestRemovedFromWorld()
    {
        R::Registry r;
        R::CreatureKey const k = Key(229, 3, 0xF130000020);
        std::uint64_t s = Begin(r, k, 0, BotA);
        s = Begin(r, k, s, BotB, {BotA});
        // Despawn/grid unload/instance unload while bots are still linked.
        r.OnCreatureRemovedFromWorld(k, s);
        CHECK(r.Size() == 0);
        CHECK(r.GetCounters().removedRetired == 1);
        R::Entry e;
        CHECK(!r.Lookup(s, e));
        // Core's later AttackStop/corpse/respawn on the same stamp: no-ops.
        r.OnParticipantEnd(k, s, BotA);
        r.OnCreatureCorpseOrRespawn(k, s, false);
        r.OnCreatureCorpseOrRespawn(k, s, true);
        r.OnCreatureRemovedFromWorld(k, s);
        CHECK(r.Size() == 0);
        CHECK(r.GetCounters().removedRetired == 1);
        CHECK(r.GetCounters().corpseRetired == 0);
        CHECK(r.GetCounters().respawnRetired == 0);
        // Same map/instance/GUID reused by a new object: clean incarnation,
        // nothing stale to replace because removal already retired it.
        std::uint64_t const fresh = Begin(r, k, 0, BotA);
        CHECK(fresh != 0 && fresh != s);
        CHECK(Active(r, fresh) == 1);
        CHECK(!Has(r, fresh, BotB));
        CHECK(r.GetCounters().staleKeyReplaced == 0);
        // Removal after corpse (corpse despawn) is idempotent.
        r.OnCreatureCorpseOrRespawn(k, fresh, false);
        r.OnCreatureRemovedFromWorld(k, fresh);
        CHECK(r.Size() == 0);
        CHECK(r.GetCounters().corpseRetired == 1);
        CHECK(r.GetCounters().removedRetired == 1);
        // A stamp/key mismatch on removal is still an impossible state.
        std::uint64_t const t = Begin(r, k, 0, BotA);
        r.OnCreatureRemovedFromWorld(Key(229, 4, 0xF130000020), t);
        CHECK(r.IsPoisoned());
    }

    // ---- Stage 2: participant/rights snapshot (creature half only) ----

    R::ParticipantSnapshot Snap(std::vector<std::uint64_t> attackers,
                                std::vector<std::uint64_t> others = {},
                                bool tapOk = true, bool plain = true, bool alive = true)
    {
        R::ParticipantSnapshot s;
        s.complete = true;
        s.plainUnscriptedNonEliteHostileNpc = plain;
        s.alive = alive;
        s.inCombat = true;
        s.tapHolderNoneOrVerifiedBot = tapOk;
        for (auto a : attackers)
        {
            if (s.attackerCount < s.attackers.size())
                s.attackers[s.attackerCount++] = a;
            if (s.participantCount < s.participants.size())
                s.participants[s.participantCount++] = a;
        }
        for (auto o : others)
            if (s.participantCount < s.participants.size())
                s.participants[s.participantCount++] = o;
        return s;
    }

    V::ObserverSnapshot EveryObserverFactTrueExceptRefresh(R::Registry const& r, std::uint64_t stamp)
    {
        V::CreatureSnapshot c;
        r.BuildCreatureSnapshot(stamp, c);
        V::ObserverSnapshot o;
        o.explicitlyEnabled = true;
        o.positivelyVerifiedNormalPlayer = true;
        o.hasVanillaEffectiveProgression = true;
        o.allParticipantsAlreadyHiddenFromThisObserver = true;
        o.noOwnTargetAuraPetTotemGuardianOrSpellInFlight = true;
        o.noObserverOrOwnedUnitThreatCombatOrPendingAssist = true;
        o.provenOutsideCreatureAggroAssistCleaveAndAoeReach = true;
        o.noObserverOrGroupQuestLootTapSkinOrKillCredit = true;
        o.observerStageAndIdentityStable = true;
        o.coreVisibilityAndThreatRefreshVerified = false; // absent bridge
        o.targetIsAliveAtThisEvaluation = true;
        o.eligibleHiddenAttackerStillAttackingThisTargetAtThisEvaluation = true;
        o.checkedAgainstCreatureIdentityToken = c.creatureIdentityToken;
        o.checkedAgainstCreatureGeneration = c.generation;
        return o;
    }

    bool AllCreatureFacts(V::CreatureSnapshot const& c)
    {
        return c.complete && c.current && c.plainUnscriptedNonEliteHostileNpc && c.alive &&
               c.inCombat && c.eligibleHiddenRandomBotCurrentlyAttackingThisCreature &&
               c.everyParticipantVerifiedTbcRaceRandomBotOrItsMinion &&
               c.noRealPlayerCombatThreatTapOrRights && c.noPendingThreatOrCombatantChanges;
    }

    void TestSnapshotAllVerifiedIsCreatureHalfOnly()
    {
        R::Registry r;
        R::CreatureKey const k = Key(0, 0, 0xF130000100);
        std::uint64_t s = Begin(r, k, 0, BotA);
        CHECK(r.NeedsSnapshot(k, s));
        // Bot B heals bot A (participant, not attacker).
        r.ApplyParticipantSnapshot(k, s, Snap({BotA}, {BotB}));
        CHECK(!r.NeedsSnapshot(k, s));
        CHECK(r.HasParticipantProof(s));
        CHECK(Active(r, s) == 1);   // roster = actual attackers only
        CHECK(!Has(r, s, BotB));
        V::CreatureSnapshot c;
        CHECK(r.BuildCreatureSnapshot(s, c));
        CHECK(AllCreatureFacts(c));
        CHECK(c.creatureIdentityToken == s && c.generation != 0);
        // Still impossible to conceal: observer half default, or every
        // observer fact true except the missing Core refresh bridge.
        CHECK(!V::ShouldConcealCreatureFromObserver(c, V::ObserverSnapshot{}));
        CHECK(!V::ShouldConcealCreatureFromObserver(c, EveryObserverFactTrueExceptRefresh(r, s)));
        CHECK(r.GetCounters().snapshotsVerified == 1);
    }

    void TestSnapshotUnverifiedParticipantsFailVisible()
    {
        // Human / Altbot / pet / minion / totem / unknown: all token 0.
        for (int variant = 0; variant < 4; ++variant)
        {
            R::Registry r;
            R::CreatureKey const k = Key(0, 0, 0xF130000110 + variant);
            std::uint64_t s = Begin(r, k, 0, BotA);
            R::ParticipantSnapshot snap = Snap({BotA}, {BotB});
            if (variant == 0)
                snap = Snap({BotA}, {Human});          // threat-only human (DoT/healer)
            else if (variant == 1)
                snap = Snap({BotA, Human});            // human attacker
            else if (variant == 2)
                snap.complete = false;                 // overflow / PvP ref / no threat list
            else
                snap.participantCount = R::MaxSnapshotParticipants + 1;
            r.ApplyParticipantSnapshot(k, s, snap);
            V::CreatureSnapshot c;
            CHECK(r.BuildCreatureSnapshot(s, c));
            CHECK(!c.everyParticipantVerifiedTbcRaceRandomBotOrItsMinion);
            CHECK(!c.noRealPlayerCombatThreatTapOrRights);
            CHECK(!c.current);
            CHECK(!r.HasParticipantProof(s));
            CHECK(Uncertain(r, s));
        }
    }

    void TestSnapshotTapAndScopeFacetsIndependent()
    {
        R::Registry r;
        R::CreatureKey const k1 = Key(0, 0, 0xF130000120);
        std::uint64_t s1 = Begin(r, k1, 0, BotA);
        r.ApplyParticipantSnapshot(k1, s1, Snap({BotA}, {}, /*tapOk*/ false));
        V::CreatureSnapshot c;
        r.BuildCreatureSnapshot(s1, c);
        CHECK(c.everyParticipantVerifiedTbcRaceRandomBotOrItsMinion);
        CHECK(!c.noRealPlayerCombatThreatTapOrRights); // human or group tap
        CHECK(!AllCreatureFacts(c));

        R::CreatureKey const k2 = Key(0, 0, 0xF130000121);
        std::uint64_t s2 = Begin(r, k2, 0, BotA);
        r.ApplyParticipantSnapshot(k2, s2, Snap({BotA}, {}, true, /*plain*/ false));
        r.BuildCreatureSnapshot(s2, c);
        CHECK(!c.plainUnscriptedNonEliteHostileNpc); // elite/boss/scripted/NPC flag
        CHECK(!AllCreatureFacts(c));

        R::CreatureKey const k3 = Key(0, 0, 0xF130000122);
        std::uint64_t s3 = Begin(r, k3, 0, BotA);
        r.ApplyParticipantSnapshot(k3, s3, Snap({BotA}, {}, true, true, /*alive*/ false));
        r.BuildCreatureSnapshot(s3, c);
        CHECK(!c.alive);
        CHECK(!c.everyParticipantVerifiedTbcRaceRandomBotOrItsMinion);
        CHECK(Uncertain(r, s3));
    }

    void TestNoAttackerMeansNotAttacking()
    {
        R::Registry r;
        R::CreatureKey const k = Key(0, 0, 0xF130000130);
        std::uint64_t s = Begin(r, k, 0, BotA);
        // Bot A stopped melee but still holds threat (e.g. DoT/caster).
        r.ApplyParticipantSnapshot(k, s, Snap({}, {BotA}));
        V::CreatureSnapshot c;
        r.BuildCreatureSnapshot(s, c);
        CHECK(c.everyParticipantVerifiedTbcRaceRandomBotOrItsMinion);
        CHECK(!c.eligibleHiddenRandomBotCurrentlyAttackingThisCreature);
        CHECK(!r.HasParticipantProof(s));
        CHECK(Active(r, s) == 0);
    }

    void TestEvidenceInvalidatesAndOnlyLaterSnapshotRecovers()
    {
        R::Registry r;
        R::CreatureKey const k = Key(0, 0, 0xF130000140);
        std::uint64_t s = Begin(r, k, 0, BotA);
        r.ApplyParticipantSnapshot(k, s, Snap({BotA}));
        CHECK(r.HasParticipantProof(s));
        // Rostered verified damage: no change, no snapshot needed.
        CHECK(!r.OnParticipantEvidence(k, s, BotA));
        CHECK(!r.NeedsSnapshot(k, s));
        CHECK(r.HasParticipantProof(s));
        // Unrostered verified caster (DoT/spell): uncertain until a later
        // snapshot that lists it.
        CHECK(r.OnParticipantEvidence(k, s, BotB));
        CHECK(!r.HasParticipantProof(s));
        CHECK(r.NeedsSnapshot(k, s));
        V::CreatureSnapshot c;
        r.BuildCreatureSnapshot(s, c);
        CHECK(!c.current && !c.noPendingThreatOrCombatantChanges);
        r.ApplyParticipantSnapshot(k, s, Snap({BotA}, {BotB}));
        CHECK(r.HasParticipantProof(s));
        // Human DoT / environment / creature source: uncertain; a later
        // snapshot still listing the human keeps it visible.
        CHECK(r.OnParticipantEvidence(k, s, Human));
        CHECK(!r.HasParticipantProof(s));
        r.ApplyParticipantSnapshot(k, s, Snap({BotA}, {BotB, Human}));
        CHECK(!r.HasParticipantProof(s));
        // Human gone from threat and combat (combat ended for them).
        r.ApplyParticipantSnapshot(k, s, Snap({BotA}, {BotB}));
        CHECK(r.HasParticipantProof(s));
        // Evidence on an untracked creature never creates an entry.
        CHECK(!r.OnParticipantEvidence(Key(0, 0, 0xF130000141), 0, Human));
        r.ApplyParticipantSnapshot(Key(0, 0, 0xF130000141), 0, Snap({BotA}));
        CHECK(r.Size() == 1);
    }

    void TestStaleGenerationAfterAnyEvent()
    {
        R::Registry r;
        R::CreatureKey const k = Key(0, 0, 0xF130000150);
        std::uint64_t s = Begin(r, k, 0, BotA);
        r.ApplyParticipantSnapshot(k, s, Snap({BotA}));
        V::CreatureSnapshot before;
        r.BuildCreatureSnapshot(s, before);
        CHECK(before.current);
        // A second verified attacker joins: roster changes, snapshot stale.
        Begin(r, k, s, BotB, {BotA});
        V::CreatureSnapshot after;
        r.BuildCreatureSnapshot(s, after);
        CHECK(!after.current);
        CHECK(after.generation != before.generation);
        CHECK(!AllCreatureFacts(after));
        // An observer pair checked against the old generation never matches.
        V::ObserverSnapshot o = EveryObserverFactTrueExceptRefresh(r, s);
        o.checkedAgainstCreatureGeneration = before.generation;
        o.coreVisibilityAndThreatRefreshVerified = true; // even hypothetically
        CHECK(!V::ShouldConcealCreatureFromObserver(after, o));
    }

    void TestAssistInvalidation()
    {
        R::Registry r;
        R::CreatureKey const c1 = Key(0, 0, 0xF130000160);
        R::CreatureKey const c2 = Key(0, 0, 0xF130000161);
        std::uint64_t s1 = Begin(r, c1, 0, BotA);
        std::uint64_t s2 = Begin(r, c2, 0, BotB);
        r.ApplyParticipantSnapshot(c1, s1, Snap({BotA}));
        r.ApplyParticipantSnapshot(c2, s2, Snap({BotB}));
        // Self heal and rostered healer: nothing changes.
        CHECK(r.OnAssistOfActor(BotA, BotA) == 0);
        CHECK(r.HasParticipantProof(s1));
        // Human heals bot A: only creatures bot A fights become uncertain.
        CHECK(r.OnAssistOfActor(BotA, Human) == 1);
        CHECK(!r.HasParticipantProof(s1));
        CHECK(r.HasParticipantProof(s2));
        // Verified bot B heals bot A on c1 (B not rostered there).
        r.ApplyParticipantSnapshot(c1, s1, Snap({BotA}));
        CHECK(r.OnAssistOfActor(BotA, BotB) == 1);
        CHECK(!r.HasParticipantProof(s1));
        CHECK(r.OnAssistOfActor(0, Human) == 0);
        CHECK(r.GetCounters().assistInvalidations == 2);
    }

    void TestLogoutDuringSnapshotAndCombatEnd()
    {
        R::Registry r;
        R::CreatureKey const k = Key(0, 0, 0xF130000170);
        std::uint64_t s = Begin(r, k, 0, BotA);
        r.ApplyParticipantSnapshot(k, s, Snap({BotA}));
        CHECK(r.HasParticipantProof(s));
        CHECK(r.ExpireActor(BotA) == 1);
        CHECK(!r.HasParticipantProof(s));
        V::CreatureSnapshot c;
        r.BuildCreatureSnapshot(s, c);
        CHECK(!c.current);
        // A snapshot still listing the expired epoch cannot restore it:
        // the caller re-reads identity under the lock, so it reports 0.
        r.ApplyParticipantSnapshot(k, s, Snap({Human}));
        CHECK(!r.HasParticipantProof(s));
        // Combat end / evade retires; later corpse/removal are no-ops.
        CHECK(r.OnCreatureLeftCombat(k, s));
        CHECK(!r.OnCreatureLeftCombat(k, s));
        r.OnCreatureCorpseOrRespawn(k, s, false);
        r.OnCreatureRemovedFromWorld(k, s);
        CHECK(r.Size() == 0);
        CHECK(r.GetCounters().leftCombatRetired == 1);
        CHECK(r.GetCounters().corpseRetired == 0);
        CHECK(!r.BuildCreatureSnapshot(s, c));
        CHECK(!c.current && c.creatureIdentityToken == 0);
    }

    void TestSnapshotRosterOverflowAndPoison()
    {
        R::Registry r;
        R::CreatureKey const k = Key(0, 0, 0xF130000180);
        std::uint64_t s = Begin(r, k, 0, BotA);
        R::ParticipantSnapshot snap = Snap({BotA});
        snap.attackerCount = P::MaxTrackedAttackers + 1;
        r.ApplyParticipantSnapshot(k, s, snap);
        CHECK(!r.HasParticipantProof(s));
        CHECK(Uncertain(r, s));
        // Mismatched key on a snapshot is an impossible state: poison.
        r.ApplyParticipantSnapshot(Key(0, 1, 0xF130000180), s, Snap({BotA}));
        CHECK(r.IsPoisoned());
        V::CreatureSnapshot c;
        CHECK(!r.BuildCreatureSnapshot(s, c));
        CHECK(!r.NeedsSnapshot(k, s));
        CHECK(!r.OnParticipantEvidence(k, s, Human));
        CHECK(r.OnAssistOfActor(BotA, Human) == 0);
    }

    void TestTokensMonotonicAcrossReset()
    {
        R::Registry r;
        std::uint64_t const a = Begin(r, Key(0, 0, 1), 0, BotA);
        r.Reset();
        CHECK(r.Size() == 0);
        std::uint64_t const b = Begin(r, Key(0, 0, 1), 0, BotA);
        CHECK(b > a);
        CHECK(r.GetCounters().verifiedBegins == 1);
    }
}

int main()
{
    TestUnverifiedNeverCreates();
    TestVerifiedBeginAndDuplicate();
    TestTwoAttackersLastExit();
    TestUnknownHumanAndMinion();
    TestCreationSnapshot();
    TestTargetSwitch();
    TestDeathCorpseRespawn();
    TestLoginExpiry();
    TestInstanceReuseAndMapSafety();
    TestStampMismatchPoisons();
    TestCapacityOverflowPoisons();
    TestAttackerOverflowUncertain();
    TestInvalidKeyPoisons();
    TestVerifiedEndOfUnknownActor();
    TestTokensMonotonicAcrossReset();
    TestRemovedFromWorld();
    TestSnapshotAllVerifiedIsCreatureHalfOnly();
    TestSnapshotUnverifiedParticipantsFailVisible();
    TestSnapshotTapAndScopeFacetsIndependent();
    TestNoAttackerMeansNotAttacking();
    TestEvidenceInvalidatesAndOnlyLaterSnapshotRecovers();
    TestStaleGenerationAfterAnyEvent();
    TestAssistInvalidation();
    TestLogoutDuringSnapshotAndCombatEnd();
    TestSnapshotRosterOverflowAndPoison();

    unsigned const total = g_pass + g_fail;
    std::printf("F14 LIFECYCLE REGISTRY %u/%u passed\n", g_pass, total);
    return g_fail == 0 ? 0 : 1;
}
