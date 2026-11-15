// IP-side contract fixture (CS01 6895011029): CS01 PB59 CombinedAuthorityPrototype (Core 4938dbf6 blob
// e4780c36, pinned copy checked by run.sh) driven by the REAL IP v1 snapshot
// Registry in src/IndividualProgressionStageSnapshot.h. Pure test; no runtime use.
#include "../../src/IndividualProgressionStageSnapshot.h"
#include "CombinedAuthorityPrototype.h"
#include <cstdio>
namespace IP = IndividualProgressionStageSnapshot;
using namespace PB59Prototype;
static int pass = 0, fail = 0;
#define CHECK(e) do { if (e) ++pass; else { ++fail; std::printf("FAIL %d: %s\n", __LINE__, #e); } } while (0)
// The proposed mapping: Status::Known -> known; every other field 1:1.
static ProgressionPublication Map(std::uint64_t guid, IP::Snapshot const& s)
{
    return {guid, s.sessionEpoch, s.revision, s.generation, s.rawStage, s.effectiveStage, s.gatedHuman,
            s.status == IP::Status::Known};
}
int main()
{
    IP::Registry reg;
    std::uint64_t const guid = 0x0000000000001234ull; // full GUID raw value
    EquipmentPublication eq{};
    auto rp = [&](std::uint64_t g) { return Map(g, reg.Get(g)); };
    auto re = [&](std::uint64_t) { return eq; };
    auto publish = [&](std::uint8_t raw, std::uint8_t eff, bool gated)
    {
        IP::Ticket t = reg.BeginPublish(guid, reg.EpochOf(guid));
        return reg.Publish(guid, t, IP::PublishInput{true, true, gated, raw, eff});
    };
    // 1. Login: registered but not yet published (dirty, Unknown) -> refuse.
    std::uint64_t const epoch = reg.Register(guid);
    eq = {guid, epoch, 1, 30, 45, true};
    CHECK(!ReadCoherent(guid, rp, re));
    // 2. Published Known: non-zero session/revision/generation, coherent.
    CHECK(publish(13, 12, true) == IP::PublishResult::Published); // unpaid gated human: paid cap 13 -> 12
    IP::Snapshot const s = reg.Get(guid);
    CHECK(s.sessionEpoch != 0 && s.revision != 0 && s.generation != 0);
    auto auth = ReadCoherent(guid, rp, re);
    CHECK(auth && auth->progression.effectiveStage == 12 && auth->progression.rawStage == 13);
    CHECK(auth && MatchesFinalB(*auth, rp, re));
    // 3. Same-value republish after invalidation: new revision -> B rejects the old authority.
    reg.Invalidate(guid);
    CHECK(!ReadCoherent(guid, rp, re));                    // Unknown in between
    CHECK(publish(13, 12, true) == IP::PublishResult::Published);
    CHECK(auth && !MatchesFinalB(*auth, rp, re));
    // 4. Invalidation BETWEEN the bracketing reads (marker change mid-command) -> refuse.
    auth = ReadCoherent(guid, rp, re);
    CHECK(auth.has_value());
    auto invalidateMid = [&](std::uint64_t) { reg.Invalidate(guid); return eq; };
    CHECK(!ReadCoherent(guid, rp, invalidateMid));
    CHECK(publish(13, 12, true) == IP::PublishResult::Published);
    // 5. Paid purchase: IP invalidates before the bit; the republish raises effective -> old B rejected.
    auth = ReadCoherent(guid, rp, re);
    reg.Invalidate(guid);
    CHECK(publish(13, 13, true) == IP::PublishResult::Published);
    CHECK(auth && !MatchesFinalB(*auth, rp, re));
    // 6. Config reload: generation bump invalidates everything -> refuse until republished.
    auth = ReadCoherent(guid, rp, re);
    reg.InvalidateAll();
    CHECK(!ReadCoherent(guid, rp, re));
    CHECK(publish(13, 13, true) == IP::PublishResult::Published);
    CHECK(reg.Get(guid).generation == s.generation + 1);
    CHECK(auth && !MatchesFinalB(*auth, rp, re));
    // 7. Relog: new IP epoch; equipment still carrying the old epoch -> refuse; republished equipment -> ok.
    reg.Erase(guid);
    std::uint64_t const epoch2 = reg.Register(guid);
    CHECK(epoch2 != epoch);
    CHECK(publish(13, 13, true) == IP::PublishResult::Published);
    CHECK(!ReadCoherent(guid, rp, re));                    // eq.session == old epoch
    eq.session = reg.EpochOf(guid);
    eq.revision++;
    CHECK(ReadCoherent(guid, rp, re).has_value());
    // 8. Wrong GUID (low part equal, high part different) never maps.
    CHECK(!ReadCoherent(guid | (1ull << 48), rp, re));
    // 9. IP disabled / account unresolved publish Unknown -> refuse.
    reg.Invalidate(guid);
    { IP::Ticket t = reg.BeginPublish(guid, reg.EpochOf(guid)); reg.Publish(guid, t, IP::PublishInput{false, true, true, 5, 5}); }
    CHECK(!ReadCoherent(guid, rp, re));
    // 10. Paid entitlement is NOT derivable: raw 12/effective 12 is the same snapshot for paid and unpaid.
    reg.Invalidate(guid);
    CHECK(publish(12, 12, true) == IP::PublishResult::Published);
    auto unpaidOrPaid = ReadCoherent(guid, rp, re);
    CHECK(unpaidOrPaid && unpaidOrPaid->progression.rawStage == unpaidOrPaid->progression.effectiveStage);
    std::printf("MAPPING CHECK %d/%d passed\n", pass, pass + fail);
    return fail ? 1 : 0;
}
