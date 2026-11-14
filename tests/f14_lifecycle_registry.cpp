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
        CHECK(!r.IsConcealmentCandidate(s));
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
        CHECK(!r.IsConcealmentCandidate(s));
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
        CHECK(!r.IsConcealmentCandidate(s));
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

    // ---- Stage 2: NON-AUTHORITATIVE participant evidence ----

    R::ParticipantSnapshot Snap(std::vector<std::uint64_t> attackers,
                                std::vector<std::uint64_t> others = {},
                                bool tapOk = true, bool plain = true, bool alive = true)
    {
        R::ParticipantSnapshot s;
        s.complete = true;
        s.plainUnscriptedNonEliteNpcScope = plain;
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

    R::ConservativeEvidence Ev(R::Registry const& r, std::uint64_t stamp)
    {
        R::ConservativeEvidence e;
        r.GetConservativeEvidence(stamp, e);
        return e;
    }

    // Every observer-side fact true (even the absent Core refresh bridge,
    // hypothetically): the creature half from this stage must still never
    // let the pure model conceal.
    V::ObserverSnapshot EveryObserverFactTrue(V::CreatureSnapshot const& c)
    {
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
        o.coreVisibilityAndThreatRefreshVerified = true;
        o.targetIsAliveAtThisEvaluation = true;
        o.eligibleHiddenAttackerStillAttackingThisTargetAtThisEvaluation = true;
        o.checkedAgainstCreatureIdentityToken = c.creatureIdentityToken;
        o.checkedAgainstCreatureGeneration = c.generation;
        return o;
    }

    bool NoAuthoritativeProof(V::CreatureSnapshot const& c)
    {
        return !c.complete && !c.eligibleHiddenRandomBotCurrentlyAttackingThisCreature &&
               !c.everyParticipantVerifiedTbcRaceRandomBotOrItsMinion &&
               !c.noRealPlayerCombatThreatTapOrRights && !c.noPendingThreatOrCombatantChanges;
    }

    bool PolicyCandidate(R::Registry const& r, std::uint64_t stamp)
    {
        R::Entry e;
        return r.Lookup(stamp, e) && P::HasVerifiedAttackerCandidates(e.state);
    }

    void TestOnlyBotEvidenceIsNeverProof()
    {
        R::Registry r;
        R::CreatureKey const k = Key(0, 0, 0xF130000100);
        std::uint64_t s = Begin(r, k, 0, BotA);
        CHECK(r.NeedsSnapshot(k, s));
        r.ApplyParticipantSnapshot(k, s, Snap({BotA}, {BotB}));
        CHECK(!r.NeedsSnapshot(k, s));
        R::ConservativeEvidence e = Ev(r, s);
        CHECK(e.current && e.enumeratedWithinBounds && e.alive && e.inCombat);
        CHECK(e.threatCombatAndAttackersObservedOnlyVerifiedBots);
        CHECK(e.tapHolderObservedNoneOrVerifiedBot);
        CHECK(e.verifiedAttackers == 1 && !e.rosterUncertain);
        // Not proof: no pure participant proof, no candidate, every
        // authoritative creature field false.
        CHECK(!PolicyCandidate(r, s));
        CHECK(!r.IsConcealmentCandidate(s));
        V::CreatureSnapshot c;
        CHECK(r.BuildCreatureSnapshot(s, c));
        CHECK(NoAuthoritativeProof(c));
        CHECK(c.creatureIdentityToken == s && c.current && c.alive);
        // Hostility is never verified here: the pure "...HostileNpc" stays false.
        CHECK(!c.plainUnscriptedNonEliteHostileNpc);
        CHECK(e.plainUnscriptedNonEliteNpcScope);
        CHECK(!V::ShouldConcealCreatureFromObserver(c, V::ObserverSnapshot{}));
        CHECK(!V::ShouldConcealCreatureFromObserver(c, EveryObserverFactTrue(c)));
        // Snapshot never rebuilt the roster from participants.
        CHECK(!Has(r, s, BotB));
        CHECK(r.GetCounters().snapshotsOnlyVerifiedBots == 1);
    }

    void TestSnapshotsOnlyAddUncertainty()
    {
        R::Registry r;
        R::CreatureKey const k = Key(0, 0, 0xF130000101);
        std::uint64_t s = Begin(r, k, 0, BotA);
        r.ApplyParticipantSnapshot(k, s, Snap({BotA}, {Human}));
        CHECK(Uncertain(r, s));
        // A later all-bot snapshot does NOT clear uncertainty.
        r.ApplyParticipantSnapshot(k, s, Snap({BotA}));
        CHECK(Uncertain(r, s));
        CHECK(Ev(r, s).rosterUncertain);
        CHECK(Ev(r, s).threatCombatAndAttackersObservedOnlyVerifiedBots); // evidence only
        CHECK(!PolicyCandidate(r, s));
        // Expired/unknown roster member is never resurrected by evidence.
        CHECK(r.ExpireActor(BotA) == 1);
        r.ApplyParticipantSnapshot(k, s, Snap({BotA}));
        CHECK(!Has(r, s, BotA));
        CHECK(Uncertain(r, s));
    }

    // CS01 acceptance cases: zero-threat, DoT-only, no-attack-target, caster
    // damage, cross-map/offline loot group, unknown pet/minion owner, enemy
    // creature links, API unavailability. All tokens 0 = unknown => visible.
    void TestCs01UnknownParticipantCases()
    {
        struct Case { char const* name; R::ParticipantSnapshot snap; bool tapOnly; };
        std::vector<Case> cases;
        cases.push_back({"zero-threat human still on threat list", Snap({BotA}, {Human}), false});
        cases.push_back({"DoT-only human (no attack target)", Snap({BotA}, {Human}), false});
        cases.push_back({"human caster damage", Snap({BotA, Human}), false});
        cases.push_back({"unknown pet/minion owner", Snap({BotA, Human}), false});
        cases.push_back({"enemy creature link (PvE combat ref)", Snap({BotA}, {Human}), false});
        R::ParticipantSnapshot apiGone = Snap({BotA});
        apiGone.complete = false;   // no threat list / not in map / PvP ref
        cases.push_back({"API unavailable or ambiguous", apiGone, false});
        R::ParticipantSnapshot tooMany = Snap({BotA});
        tooMany.participantCount = R::MaxSnapshotParticipants + 1;
        cases.push_back({"participant overflow", tooMany, false});
        cases.push_back({"cross-map/offline loot group", Snap({BotA}, {}, /*tapOk*/ false), true});
        std::uint64_t guid = 0xF130000200;
        for (Case const& c : cases)
        {
            R::Registry r;
            R::CreatureKey const k = Key(0, 0, guid++);
            std::uint64_t s = Begin(r, k, 0, BotA);
            r.ApplyParticipantSnapshot(k, s, c.snap);
            R::ConservativeEvidence e = Ev(r, s);
            V::CreatureSnapshot cs;
            r.BuildCreatureSnapshot(s, cs);
            bool ok = NoAuthoritativeProof(cs) && !PolicyCandidate(r, s) &&
                      !V::ShouldConcealCreatureFromObserver(cs, EveryObserverFactTrue(cs));
            if (c.tapOnly)
                ok = ok && !e.tapHolderObservedNoneOrVerifiedBot;
            else
                ok = ok && !e.threatCombatAndAttackersObservedOnlyVerifiedBots && Uncertain(r, s);
            if (!ok)
                std::printf("case failed: %s\n", c.name);
            CHECK(ok);
        }
    }

    void TestScopeAliveAndAttackingFacets()
    {
        R::Registry r;
        R::CreatureKey const k1 = Key(0, 0, 0xF130000120);
        std::uint64_t s1 = Begin(r, k1, 0, BotA);
        r.ApplyParticipantSnapshot(k1, s1, Snap({BotA}, {}, true, /*plain*/ false));
        CHECK(!Ev(r, s1).plainUnscriptedNonEliteNpcScope); // elite/boss/scripted/NPC flag

        R::CreatureKey const k2 = Key(0, 0, 0xF130000121);
        std::uint64_t s2 = Begin(r, k2, 0, BotA);
        r.ApplyParticipantSnapshot(k2, s2, Snap({BotA}, {}, true, true, /*alive*/ false));
        CHECK(!Ev(r, s2).alive);
        CHECK(!Ev(r, s2).threatCombatAndAttackersObservedOnlyVerifiedBots);
        CHECK(Uncertain(r, s2));

        // Verified bot holds threat but is not attacking (caster/DoT/healer):
        // evidence notes only-bots, roster/attacker count is unchanged.
        R::CreatureKey const k3 = Key(0, 0, 0xF130000122);
        std::uint64_t s3 = Begin(r, k3, 0, BotA);
        r.OnParticipantEnd(k3, s3, BotA);
        r.ApplyParticipantSnapshot(k3, s3, Snap({}, {BotA}));
        CHECK(Ev(r, s3).verifiedAttackers == 0);
        CHECK(!PolicyCandidate(r, s3));
    }

    void TestEvidenceInvalidationAndNoSameEventSnapshot()
    {
        R::Registry r;
        R::CreatureKey const k = Key(0, 0, 0xF130000140);
        std::uint64_t s = Begin(r, k, 0, BotA);
        r.ApplyParticipantSnapshot(k, s, Snap({BotA}));
        // Rostered verified source: no change, evidence stays current.
        CHECK(!r.OnParticipantEvidence(k, s, BotA));
        CHECK(!r.NeedsSnapshot(k, s));
        CHECK(Ev(r, s).current);
        // Unrostered verified caster: uncertain, evidence stale.
        CHECK(r.OnParticipantEvidence(k, s, BotB));
        CHECK(Uncertain(r, s));
        CHECK(!Ev(r, s).current);
        CHECK(r.NeedsSnapshot(k, s));
        // Human / environment / creature source: uncertain.
        R::CreatureKey const k2 = Key(0, 0, 0xF130000141);
        std::uint64_t s2 = Begin(r, k2, 0, BotA);
        CHECK(r.OnParticipantEvidence(k2, s2, Human));
        CHECK(Uncertain(r, s2));
        // Untracked creature: nothing created.
        CHECK(!r.OnParticipantEvidence(Key(0, 0, 0xF130000142), 0, Human));
        r.ApplyParticipantSnapshot(Key(0, 0, 0xF130000142), 0, Snap({BotA}));
        CHECK(r.Size() == 2);
    }

    void TestStaleGenerationAfterAnyEvent()
    {
        R::Registry r;
        R::CreatureKey const k = Key(0, 0, 0xF130000150);
        std::uint64_t s = Begin(r, k, 0, BotA);
        r.ApplyParticipantSnapshot(k, s, Snap({BotA}));
        R::ConservativeEvidence before = Ev(r, s);
        CHECK(before.current);
        Begin(r, k, s, BotB, {BotA});
        R::ConservativeEvidence after = Ev(r, s);
        CHECK(!after.current);
        CHECK(after.generation != before.generation);
        CHECK(!after.threatCombatAndAttackersObservedOnlyVerifiedBots);
        CHECK(!after.tapHolderObservedNoneOrVerifiedBot);
        V::CreatureSnapshot c;
        r.BuildCreatureSnapshot(s, c);
        V::ObserverSnapshot o = EveryObserverFactTrue(c);
        o.checkedAgainstCreatureGeneration = before.generation;
        CHECK(!V::ShouldConcealCreatureFromObserver(c, o));
    }

    void TestAssistInvalidation()
    {
        R::Registry r;
        R::CreatureKey const c1 = Key(0, 0, 0xF130000160);
        R::CreatureKey const c2 = Key(0, 0, 0xF130000161);
        std::uint64_t s1 = Begin(r, c1, 0, BotA);
        std::uint64_t s2 = Begin(r, c2, 0, BotB);
        CHECK(r.OnAssistOfActor(BotA, BotA) == 0);
        CHECK(!Uncertain(r, s1));
        CHECK(r.OnAssistOfActor(BotA, Human) == 1);
        CHECK(Uncertain(r, s1));
        CHECK(!Uncertain(r, s2));
        CHECK(r.OnAssistOfActor(BotB, BotA) == 1);   // unrostered verified healer
        CHECK(Uncertain(r, s2));
        CHECK(r.OnAssistOfActor(0, Human) == 0);
        CHECK(r.GetCounters().assistInvalidations == 2);
    }

    void TestLogoutRaceAndCombatEnd()
    {
        R::Registry r;
        R::CreatureKey const k = Key(0, 0, 0xF130000170);
        std::uint64_t s = Begin(r, k, 0, BotA);
        r.ApplyParticipantSnapshot(k, s, Snap({BotA}));
        CHECK(Ev(r, s).current);
        CHECK(r.ExpireActor(BotA) == 1);
        CHECK(!Ev(r, s).current);
        CHECK(Uncertain(r, s));
        // Caller re-reads identity under the lock: an expired bot reports 0.
        r.ApplyParticipantSnapshot(k, s, Snap({Human}));
        CHECK(!Ev(r, s).threatCombatAndAttackersObservedOnlyVerifiedBots);
        CHECK(r.OnCreatureLeftCombat(k, s));
        CHECK(!r.OnCreatureLeftCombat(k, s));
        r.OnCreatureCorpseOrRespawn(k, s, false);
        r.OnCreatureRemovedFromWorld(k, s);
        CHECK(r.Size() == 0);
        CHECK(r.GetCounters().leftCombatRetired == 1);
        CHECK(r.GetCounters().corpseRetired == 0);
        V::CreatureSnapshot c;
        CHECK(!r.BuildCreatureSnapshot(s, c));
        CHECK(c.creatureIdentityToken == 0 && !c.current);
    }

    void TestSnapshotOverflowAndPoison()
    {
        R::Registry r;
        R::CreatureKey const k = Key(0, 0, 0xF130000180);
        std::uint64_t s = Begin(r, k, 0, BotA);
        R::ParticipantSnapshot snap = Snap({BotA});
        snap.attackerCount = P::MaxTrackedAttackers + 1;
        r.ApplyParticipantSnapshot(k, s, snap);
        CHECK(!Ev(r, s).enumeratedWithinBounds);
        CHECK(Uncertain(r, s));
        r.ApplyParticipantSnapshot(Key(0, 1, 0xF130000180), s, Snap({BotA}));
        CHECK(r.IsPoisoned());
        R::ConservativeEvidence e;
        CHECK(!r.GetConservativeEvidence(s, e));
        CHECK(!r.NeedsSnapshot(k, s));
        CHECK(!r.OnParticipantEvidence(k, s, Human));
        CHECK(r.OnAssistOfActor(BotA, Human) == 0);
    }

    // ---- Bounded enumeration (CS01: total visit budget under global lock) ----

    void TestCollectorDuplicateFloodStopsAtBudget()
    {
        R::SnapshotCollector c;
        std::size_t iterations = 0;
        for (int i = 0; i < 100000; ++i)
        {
            ++iterations;
            if (!c.AddParticipant(BotA))  // same token over and over
                break;
        }
        CHECK(iterations == R::MaxSnapshotVisits + 1);
        CHECK(c.Stopped());
        CHECK(c.Visits() == R::MaxSnapshotVisits + 1);
        R::ParticipantSnapshot snap = c.Take();
        CHECK(!snap.complete);
        CHECK(snap.participantCount == 1);
        CHECK(!c.AddParticipant(BotB));     // stays stopped
        CHECK(!c.AddAttacker(BotB));
        // Fails visible in the registry.
        R::Registry r;
        R::CreatureKey const k = Key(0, 0, 0xF130000300);
        std::uint64_t s = Begin(r, k, 0, BotA);
        r.ApplyParticipantSnapshot(k, s, snap);
        CHECK(Uncertain(r, s));
        CHECK(!Ev(r, s).enumeratedWithinBounds);
    }

    void TestCollectorBudgetAcrossThreeRanges()
    {
        // Mixed duplicates across "threat", "combat" and "attackers" share
        // ONE budget: 30 + 30 + 10 visits > 64.
        R::SnapshotCollector c;
        bool ok = true;
        for (int i = 0; ok && i < 30; ++i) ok = c.AddParticipant(BotA);
        CHECK(ok);
        for (int i = 0; ok && i < 30; ++i) ok = c.AddParticipant(BotB);
        CHECK(ok);
        std::size_t attackerCalls = 0;
        for (int i = 0; ok && i < 10; ++i) { ++attackerCalls; ok = c.AddAttacker(BotA); }
        CHECK(!ok);
        CHECK(attackerCalls == R::MaxSnapshotVisits - 60 + 1);
        CHECK(!c.Take().complete);
    }

    void TestCollectorStopsOnUnknownAndOverflow()
    {
        R::SnapshotCollector u;
        CHECK(u.AddParticipant(BotA));
        CHECK(!u.AddParticipant(Human));     // unknown: stop immediately
        CHECK(!u.AddParticipant(BotB));
        CHECK(u.Visits() == 2);
        CHECK(!u.Take().complete);

        R::SnapshotCollector p;              // > MaxSnapshotParticipants distinct
        std::size_t calls = 0;
        for (std::uint64_t t = 1; t < 1000; ++t) { ++calls; if (!p.AddParticipant(5000 + t)) break; }
        CHECK(calls == R::MaxSnapshotParticipants + 1);
        CHECK(p.Take().participantCount == R::MaxSnapshotParticipants);
        CHECK(!p.Take().complete);

        R::SnapshotCollector a;              // > MaxTrackedAttackers distinct
        calls = 0;
        for (std::uint64_t t = 1; t < 1000; ++t) { ++calls; if (!a.AddAttacker(7000 + t)) break; }
        CHECK(calls == P::MaxTrackedAttackers + 1);
        CHECK(a.Take().attackerCount == P::MaxTrackedAttackers);
        CHECK(!a.Take().complete);

        R::SnapshotCollector f;              // API unavailable / PvP ref
        f.Fail();
        CHECK(f.Stopped());
        CHECK(!f.AddParticipant(BotA));
        CHECK(f.Visits() == 0);
        CHECK(!f.Take().complete);
    }

    void TestCollectorZeroAndNormal()
    {
        // Zero participants for a tracked creature is unknown => visible.
        R::Registry r;
        R::CreatureKey const k0 = Key(0, 0, 0xF130000310);
        std::uint64_t s0 = Begin(r, k0, 0, BotA);
        R::SnapshotCollector empty;
        R::ParticipantSnapshot z = empty.Take();
        z.alive = true;
        z.inCombat = true;
        z.tapHolderNoneOrVerifiedBot = true;
        CHECK(z.complete && z.participantCount == 0);
        r.ApplyParticipantSnapshot(k0, s0, z);
        CHECK(Uncertain(r, s0));
        CHECK(!Ev(r, s0).threatCombatAndAttackersObservedOnlyVerifiedBots);

        // Ordinary bounded case: evidence recorded, still never proof.
        R::CreatureKey const k1 = Key(0, 0, 0xF130000311);
        std::uint64_t s1 = Begin(r, k1, 0, BotA);
        R::SnapshotCollector c;
        CHECK(c.AddParticipant(BotB));
        CHECK(c.AddParticipant(BotA));
        CHECK(c.AddAttacker(BotA));
        R::ParticipantSnapshot n = c.Take();
        n.alive = true;
        n.inCombat = true;
        n.tapHolderNoneOrVerifiedBot = true;
        CHECK(n.complete && n.participantCount == 2 && n.attackerCount == 1);
        r.ApplyParticipantSnapshot(k1, s1, n);
        CHECK(Ev(r, s1).threatCombatAndAttackersObservedOnlyVerifiedBots);
        CHECK(!r.IsConcealmentCandidate(s1));
        CHECK(!Uncertain(r, s1));
    }

    // ---- Stage 3: verified-owner minion evidence (never proof) ----

    constexpr std::uint64_t OwnerGuid = 0x0000000000000101ULL;   // player GUID raw
    constexpr std::uint64_t OtherGuid = 0x0000000000000202ULL;

    R::MinionLineage GoodLineage()
    {
        R::MinionLineage l;
        l.minionType = true;
        l.vehicleOrPossessed = false;
        l.ownerIsPlayer = true;
        l.owner = OwnerGuid;
        l.creator = OwnerGuid;
        return l;
    }

    void TestMinionLineageAllMismatchesFail()
    {
        CHECK(R::LineageMatchesSingleOwner(GoodLineage()));
        CHECK(R::MinionOwnerEvidence(GoodLineage(), BotA) == BotA);
        // Owner logged out / human / Altbot / GM / unselected: epoch 0.
        CHECK(R::MinionOwnerEvidence(GoodLineage(), 0) == 0);
        R::MinionLineage l;
        CHECK(!R::LineageMatchesSingleOwner(l));                     // defaults fail
        l = GoodLineage(); l.minionType = false;            CHECK(R::MinionOwnerEvidence(l, BotA) == 0);
        l = GoodLineage(); l.vehicleOrPossessed = true;     CHECK(R::MinionOwnerEvidence(l, BotA) == 0);
        l = GoodLineage(); l.owner = 0;                     CHECK(R::MinionOwnerEvidence(l, BotA) == 0);
        l = GoodLineage(); l.ownerIsPlayer = false;         CHECK(R::MinionOwnerEvidence(l, BotA) == 0);
        l = GoodLineage(); l.charmer = OtherGuid;           CHECK(R::MinionOwnerEvidence(l, BotA) == 0);
        l = GoodLineage(); l.charmer = OwnerGuid;           CHECK(R::MinionOwnerEvidence(l, BotA) == BotA);
        l = GoodLineage(); l.creator = OtherGuid;           CHECK(R::MinionOwnerEvidence(l, BotA) == 0);
        l = GoodLineage(); l.creator = 0;                   CHECK(R::MinionOwnerEvidence(l, BotA) == BotA);
        l = GoodLineage(); l.summon = true;                 CHECK(R::MinionOwnerEvidence(l, BotA) == 0); // no summon data
        l.summonDataAvailable = true;                       CHECK(R::MinionOwnerEvidence(l, BotA) == BotA);
        l.summoner = OtherGuid;                             CHECK(R::MinionOwnerEvidence(l, BotA) == 0);
        l.summoner = OwnerGuid;                             CHECK(R::MinionOwnerEvidence(l, BotA) == BotA);
    }

    R::ParticipantSnapshot WithBasics(R::ParticipantSnapshot snap)
    {
        snap.alive = true;
        snap.inCombat = true;
        snap.tapHolderNoneOrVerifiedBot = true;
        snap.plainUnscriptedNonEliteNpcScope = true;
        return snap;
    }

    void TestMinionEvidenceSeparateAndNeverProof()
    {
        R::Registry r;
        R::CreatureKey const k = Key(0, 0, 0xF130000400);
        std::uint64_t s = Begin(r, k, 0, BotA);
        R::SnapshotCollector c;
        CHECK(c.AddAttacker(BotA));
        CHECK(c.AddMinion(BotA));        // bot A's own pet
        CHECK(c.AddMinion(BotB));        // bot B's totem; B is not even a participant
        CHECK(c.AddMinion(BotA));        // duplicate owner: one more visit only
        R::ParticipantSnapshot snap = WithBasics(c.Take());
        CHECK(snap.complete && snap.minionOwnerCount == 2 && snap.minionVisits == 3);
        CHECK(snap.participantCount == 1 && snap.attackerCount == 1);   // minions not participants
        r.ApplyParticipantSnapshot(k, s, snap);
        R::ConservativeEvidence e = Ev(r, s);
        CHECK(e.current);
        CHECK(e.observedOnlyVerifiedBotsOrVerifiedOwnerMinions);
        CHECK(e.verifiedOwnerMinionOwners == 2);
        CHECK(!e.threatCombatAndAttackersObservedOnlyVerifiedBots); // first-party stays false
        CHECK(Uncertain(r, s));          // minion presence still adds uncertainty
        CHECK(!Has(r, s, BotB));         // minion owner never enters the roster
        CHECK(e.verifiedAttackers == 1);
        CHECK(!r.IsConcealmentCandidate(s));
        CHECK(!PolicyCandidate(r, s));
        V::CreatureSnapshot cs;
        r.BuildCreatureSnapshot(s, cs);
        CHECK(NoAuthoritativeProof(cs));
        CHECK(!V::ShouldConcealCreatureFromObserver(cs, EveryObserverFactTrue(cs)));
    }

    void TestMinionUnknownOwnerStops()
    {
        R::SnapshotCollector c;
        CHECK(c.AddParticipant(BotA));
        CHECK(!c.AddMinion(0));          // unknown lineage / owner: stop at once
        CHECK(!c.AddMinion(BotA));
        CHECK(!c.AddParticipant(BotB));
        CHECK(c.Visits() == 2);
        R::ParticipantSnapshot snap = WithBasics(c.Take());
        CHECK(!snap.complete);
        R::Registry r;
        R::CreatureKey const k = Key(0, 0, 0xF130000401);
        std::uint64_t s = Begin(r, k, 0, BotA);
        r.ApplyParticipantSnapshot(k, s, snap);
        CHECK(!Ev(r, s).observedOnlyVerifiedBotsOrVerifiedOwnerMinions);
        CHECK(Uncertain(r, s));
    }

    void TestMinionBudgetAndOverflow()
    {
        R::SnapshotCollector flood;      // one pet flooding the lists
        std::size_t calls = 0;
        for (int i = 0; i < 10000; ++i) { ++calls; if (!flood.AddMinion(BotA)) break; }
        CHECK(calls == R::MaxSnapshotVisits + 1);
        CHECK(!flood.Take().complete);

        R::SnapshotCollector mixed;      // participants and minions share one budget
        bool ok = true;
        for (int i = 0; ok && i < 40; ++i) ok = mixed.AddParticipant(BotA);
        std::size_t minionCalls = 0;
        for (int i = 0; ok && i < 40; ++i) { ++minionCalls; ok = mixed.AddMinion(BotB); }
        CHECK(!ok && minionCalls == R::MaxSnapshotVisits - 40 + 1);

        R::SnapshotCollector many;       // > MaxSnapshotMinionOwners distinct owners
        calls = 0;
        for (std::uint64_t t = 1; t < 1000; ++t) { ++calls; if (!many.AddMinion(9000 + t)) break; }
        CHECK(calls == R::MaxSnapshotMinionOwners + 1);
        CHECK(many.Take().minionOwnerCount == R::MaxSnapshotMinionOwners);
        CHECK(!many.Take().complete);
    }

    void TestMinionOwnerExpiryMakesEvidenceStale()
    {
        R::Registry r;
        R::CreatureKey const k = Key(0, 0, 0xF130000402);
        std::uint64_t s = Begin(r, k, 0, BotA);
        R::SnapshotCollector c;
        CHECK(c.AddAttacker(BotA));
        CHECK(c.AddMinion(BotB));        // owner B is NOT rostered here
        r.ApplyParticipantSnapshot(k, s, WithBasics(c.Take()));
        CHECK(Ev(r, s).current && Ev(r, s).observedOnlyVerifiedBotsOrVerifiedOwnerMinions);
        std::uint32_t const before = Ev(r, s).generation;
        // Owner B logs out: the evidence that depended on B is stale.
        CHECK(r.ExpireActor(BotB) == 1);
        R::ConservativeEvidence e = Ev(r, s);
        CHECK(!e.current);
        CHECK(e.generation != before);
        CHECK(!e.observedOnlyVerifiedBotsOrVerifiedOwnerMinions);
        CHECK(e.verifiedOwnerMinionOwners == 0);
        CHECK(Has(r, s, BotA));          // first-party roster untouched
        CHECK(r.GetCounters().minionOwnerExpiries == 1);
        // A later snapshot re-reads owner identity under the lock: a logged
        // out owner reports 0, so the minion is unknown and stops.
        R::SnapshotCollector again;
        CHECK(again.AddAttacker(BotA));
        CHECK(!again.AddMinion(0));
        r.ApplyParticipantSnapshot(k, s, WithBasics(again.Take()));
        CHECK(!Ev(r, s).observedOnlyVerifiedBotsOrVerifiedOwnerMinions);
        // Expiring an unrelated epoch touches nothing.
        CHECK(r.ExpireActor(0x999999) == 0);
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
    TestOnlyBotEvidenceIsNeverProof();
    TestSnapshotsOnlyAddUncertainty();
    TestCs01UnknownParticipantCases();
    TestScopeAliveAndAttackingFacets();
    TestEvidenceInvalidationAndNoSameEventSnapshot();
    TestStaleGenerationAfterAnyEvent();
    TestAssistInvalidation();
    TestLogoutRaceAndCombatEnd();
    TestSnapshotOverflowAndPoison();
    TestCollectorDuplicateFloodStopsAtBudget();
    TestCollectorBudgetAcrossThreeRanges();
    TestCollectorStopsOnUnknownAndOverflow();
    TestCollectorZeroAndNormal();
    TestMinionLineageAllMismatchesFail();
    TestMinionEvidenceSeparateAndNeverProof();
    TestMinionUnknownOwnerStops();
    TestMinionBudgetAndOverflow();
    TestMinionOwnerExpiryMakesEvidenceStale();

    unsigned const total = g_pass + g_fail;
    std::printf("F14 LIFECYCLE REGISTRY %u/%u passed\n", g_pass, total);
    return g_fail == 0 ? 0 : 1;
}
