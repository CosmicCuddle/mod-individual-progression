// F14 logout map-thread request queue: pure value-only regression tests.
// Compilation under C++17/C++20 does NOT run AzerothCore, clients or packets.
#include "IndividualProgressionF14LogoutRefreshQueue.h"
#include <cstdint>
#include <cstdio>

namespace Q = IndividualProgressionF14LogoutRefreshQueue;
namespace R = IndividualProgressionF14LifecycleRegistry;

static int passed = 0;
static int failed = 0;
#define CHECK(x) do { if (x) ++passed; else { ++failed; std::printf("FAIL %d: %s\n", __LINE__, #x); } } while (0)

int main()
{
    R::CreatureKey const a{0, 0, 11};
    R::CreatureKey const b{0, 0, 12};
    R::CreatureKey const c{1, 0, 11}; // different map; recycled raw guid
    R::CreatureKey const d{0, 1, 11}; // different instance
    R::CreatureKey const invalid{};

    Q::Queue<2> queue;
    CHECK(!queue.HasPending());
    CHECK(!queue.Remember(invalid, 100));
    CHECK(!queue.Remember(a, 0));
    CHECK(queue.Size() == 0);
    CHECK(queue.Remember(a, 100));
    CHECK(queue.Remember(a, 100)); // duplicate fanout is not duplicate work
    CHECK(queue.Size() == 1);
    CHECK(queue.Remember(b, 200));
    CHECK(queue.Size() == 2);
    CHECK(!queue.Take(c, 100));
    CHECK(!queue.Take(d, 100));
    CHECK(queue.Size() == 2);
    CHECK(queue.Take(a, 100));
    CHECK(!queue.Take(a, 100)); // one-shot, exactly once
    CHECK(!queue.Take(b, 201)); // same key, changed incarnation: deny
    CHECK(!queue.HasPending());
    CHECK(!queue.IsPoisoned());

    queue.Reset();
    CHECK(queue.Remember(a, 100));
    CHECK(queue.Remember(a, 101)); // overwrite stale identity
    CHECK(queue.Size() == 1);
    CHECK(!queue.Take(a, 100));
    CHECK(!queue.HasPending());
    CHECK(queue.Remember(a, 102));
    queue.Discard(a);
    CHECK(!queue.Take(a, 102));
    CHECK(!queue.HasPending());

    Q::Queue<1> bounded;
    CHECK(bounded.Remember(a, 100));
    CHECK(!bounded.Remember(b, 200)); // never silently evict first request
    CHECK(bounded.IsPoisoned());
    CHECK(!bounded.HasPending());
    CHECK(!bounded.Take(a, 100));
    CHECK(!bounded.Remember(a, 300));
    bounded.Reset();
    CHECK(!bounded.IsPoisoned());
    CHECK(bounded.Remember(b, 200));
    CHECK(bounded.Take(b, 200));

    // The *same* registry callback used at logout reports original
    // map/instance/GUID and incarnation, not any Unit or Creature pointer.
    R::Registry registry(2);
    auto const begun = registry.OnParticipantBegin(a, 0, 9001, nullptr, 0);
    CHECK(begun.stamp != 0);
    CHECK(begun.created);
    std::uint64_t copiedStamp = 0;
    R::CreatureKey copiedKey{};
    unsigned callbackCount = 0;
    CHECK(registry.ExpireActor(9001, [&](R::CreatureKey const& key, std::uint64_t stamp)
    {
        copiedKey = key;
        copiedStamp = stamp;
        ++callbackCount;
    }) == 1);
    CHECK(callbackCount == 1);
    CHECK(copiedKey == a);
    CHECK(copiedStamp == begun.stamp);
    CHECK(registry.ExpireActor(0, [&](R::CreatureKey const&, std::uint64_t)
    {
        ++callbackCount;
    }) == 0);
    CHECK(callbackCount == 1);

    std::printf("F14 logout queue tests %d/%d passed; NO GAMEPLAY.\n", passed, passed + failed);
    return failed == 0 ? 0 : 1;
}
