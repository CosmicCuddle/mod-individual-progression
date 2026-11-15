// IP-side binding fixture (CS01 Part11 6896196735): the IP v1 snapshot epoch
// and Core's PB59 equipment incarnation are INDEPENDENT counters. A command
// authority record carries both; B is valid only if each still matches its own
// source. No equality between the two is assumed. Pure test; no runtime use.
//   - real IP Registry: ../../src/IndividualProgressionStageSnapshot.h
//   - real Core EquipmentCaptureState (pinned copy of Core 82d340c1, checked by run.sh)
#include "../../src/IndividualProgressionStageSnapshot.h"
#include "EquipmentCaptureState.h"
#include <cstdio>
#include <optional>

namespace IP = IndividualProgressionStageSnapshot;
static int pass = 0, fail = 0;
#define CHECK(e) do { if (e) ++pass; else { ++fail; std::printf("FAIL %d: %s\n", __LINE__, #e); } } while (0)

// The proposed explicit binding (what PB would capture at command enqueue).
struct Binding
{
    std::uint64_t guid = 0;
    IP::Snapshot ip;        // full Known snapshot (epoch, generation, revision, stages)
    PB59::Stamp core;       // Core incarnation + equipment revision
};

// Capture on the sender's owner context: both sources must be current.
static std::optional<Binding> Capture(std::uint64_t guid, IP::Registry const& reg, PB59::EquipmentCaptureState const& eq)
{
    IP::Snapshot const s = reg.Get(guid);
    auto stamp = eq.Capture();
    if (s.status != IP::Status::Known || !stamp)
        return std::nullopt;
    return Binding{guid, s, *stamp};
}

// Final check B: IP bracket reads around the Core check, each against its own source.
static bool ValidAtB(Binding const& b, std::uint64_t guid, IP::Registry const& reg, PB59::EquipmentCaptureState const& eq)
{
    if (b.guid != guid)
        return false;
    IP::Snapshot const first = reg.Get(guid);
    bool const coreCurrent = eq.Current(b.core);
    IP::Snapshot const last = reg.Get(guid);
    return coreCurrent && IP::SameKnownSnapshot(first, last) && IP::SameKnownSnapshot(first, b.ip);
}

static IP::PublishResult Publish(IP::Registry& reg, std::uint64_t guid, std::uint8_t stage)
{
    IP::Ticket t = reg.BeginPublish(guid, reg.EpochOf(guid));
    return reg.Publish(guid, t, IP::PublishInput{true, true, true, stage, stage});
}

int main()
{
    std::uint64_t const guid = 0x0000000000004242ull;
    IP::Registry reg;
    PB59::EquipmentCaptureState eq;
    // Burn some counters so the two tokens are deliberately UNEQUAL.
    for (int i = 0; i < 5; ++i) { reg.Register(0x9000 + i); }
    // 1. Login gap: Core binds (SetPlayer) before IP registers (OnPlayerLogin) -> refuse.
    eq.Bind();
    CHECK(!Capture(guid, reg, eq));
    reg.Register(guid);
    CHECK(!Capture(guid, reg, eq));                 // registered but not yet published
    CHECK(Publish(reg, guid, 3) == IP::PublishResult::Published);
    // 2. Deliberately unequal valid counters pass.
    auto b = Capture(guid, reg, eq);
    CHECK(b && b->ip.sessionEpoch != b->core.session);
    CHECK(b && ValidAtB(*b, guid, reg, eq));
    // 3. GUID mismatch refuses.
    CHECK(b && !ValidAtB(*b, guid + 1, reg, eq));
    // 4. Core revision mutation (level-up / equip) with IP unchanged -> refuse; recapture passes.
    { PB59::EquipmentCaptureState::Mutation m(eq); CHECK(!eq.Capture()); } // nested mutation in progress
    CHECK(b && !ValidAtB(*b, guid, reg, eq));
    b = Capture(guid, reg, eq);
    CHECK(b && ValidAtB(*b, guid, reg, eq));
    // 5. IP revision change (marker invalidation + republish) with Core unchanged -> refuse.
    reg.Invalidate(guid);
    CHECK(b && !ValidAtB(*b, guid, reg, eq));
    CHECK(Publish(reg, guid, 3) == IP::PublishResult::Published);
    CHECK(b && !ValidAtB(*b, guid, reg, eq));        // same values, new revision
    b = Capture(guid, reg, eq);
    CHECK(b && ValidAtB(*b, guid, reg, eq));
    // 6. Reconnect while in world: Core Detach + Bind (new incarnation), IP same epoch -> old refuses.
    std::uint64_t const ipEpochBefore = reg.EpochOf(guid);
    eq.Detach();
    eq.Bind();
    CHECK(reg.EpochOf(guid) == ipEpochBefore);       // IP keeps its epoch (no script hook fires)
    CHECK(b && !ValidAtB(*b, guid, reg, eq));
    auto b2 = Capture(guid, reg, eq);
    CHECK(b2 && b2->ip.sessionEpoch == ipEpochBefore && b2->core.session != b->core.session);
    CHECK(b2 && ValidAtB(*b2, guid, reg, eq));
    // 7. Logout gap: Core detaches (LogoutPlayer) before IP erases (OnPlayerLogout) -> refuse.
    eq.Detach();
    CHECK(reg.Get(guid).status == IP::Status::Known);
    CHECK(b2 && !ValidAtB(*b2, guid, reg, eq));
    CHECK(!Capture(guid, reg, eq));
    reg.Erase(guid);
    CHECK(b2 && !ValidAtB(*b2, guid, reg, eq));
    // 8. Stale IP epoch after a full relog (new IP epoch, new Core incarnation) -> old refuses.
    eq.Bind();
    reg.Register(guid);
    CHECK(Publish(reg, guid, 3) == IP::PublishResult::Published);
    CHECK(b2 && !ValidAtB(*b2, guid, reg, eq));
    auto b3 = Capture(guid, reg, eq);
    CHECK(b3 && ValidAtB(*b3, guid, reg, eq));
    // 9. Config reload (IP generation) -> refuse.
    reg.InvalidateAll();
    CHECK(Publish(reg, guid, 3) == IP::PublishResult::Published);
    CHECK(b3 && !ValidAtB(*b3, guid, reg, eq));
    std::printf("IP/CORE BINDING CHECK %d/%d passed\n", pass, pass + fail);
    return fail ? 1 : 0;
}
