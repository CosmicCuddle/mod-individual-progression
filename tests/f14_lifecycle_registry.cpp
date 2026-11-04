// Native tests for the F14 attack/corpse lifecycle registry (pure header).
// Built with -Werror under GCC/Clang, C++17/C++20, with and without NDEBUG.
// These prove bookkeeping semantics only, not in-game behaviour.
#include "IndividualProgressionF14LifecycleRegistry.h"

#include <cstdint>
#include <cstdio>
#include <vector>

namespace R = IndividualProgressionF14LifecycleRegistry;
namespace P = IndividualProgressionF14MultiAttackerPolicy;

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

    unsigned const total = g_pass + g_fail;
    std::printf("F14 LIFECYCLE REGISTRY %u/%u passed\n", g_pass, total);
    return g_fail == 0 ? 0 : 1;
}
