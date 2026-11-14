// F14 pure session provenance: no Worldserver, no live session, no hiding.
#include "IndividualProgressionF14HumanViewerEpoch.h"
#include <cstdint>
#include <cstdio>
#include <limits>

namespace E = IndividualProgressionF14HumanViewerEpoch;
static unsigned ok = 0, fail = 0;
#define CHECK(condition) do { if (condition) ++ok; else { ++fail; \
    std::printf("FAIL line %d: %s\n", __LINE__, #condition); } } while (0)

static void IssuerCases()
{
    E::Issuer i;
    CHECK(i.LastIssued() == 0);
    CHECK(i.Issue() == 1);
    CHECK(i.Issue() == 2);
    CHECK(i.Issue() == 3);
    CHECK(i.LastIssued() == 3);

    constexpr std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
    E::Issuer almost(maximum - 2);
    CHECK(almost.Issue() == maximum - 1);
    CHECK(almost.Issue() == maximum);
    CHECK(almost.Issue() == 0); // permanently exhausted, never reuse zero/one
    CHECK(almost.Issue() == 0);
    CHECK(almost.LastIssued() == maximum);
    E::Issuer exhausted(maximum);
    CHECK(exhausted.Issue() == 0);
    CHECK(exhausted.LastIssued() == maximum);
}

static void StampCases()
{
    E::Issuer i;
    E::Stamp s;
    CHECK(s.playerGuidRaw == 0 && s.loginEpoch == 0);
    CHECK(!s.Matches(101, 1));

    const auto epoch1 = i.Issue();
    s.Bind(101, epoch1);
    CHECK(s.Matches(101, epoch1));
    CHECK(!s.Matches(102, epoch1));
    CHECK(!s.Matches(101, epoch1 + 1));
    CHECK(!s.Matches(0, epoch1));
    CHECK(!s.Matches(101, 0));

    s.Clear(); // player logout revokes previous session
    CHECK(!s.Matches(101, epoch1));
    CHECK(s.playerGuidRaw == 0 && s.loginEpoch == 0);

    const auto epoch2 = i.Issue();
    s.Bind(101, epoch2); // same character, different login
    CHECK(epoch2 > epoch1);
    CHECK(s.Matches(101, epoch2));
    CHECK(!s.Matches(101, epoch1));
    s.Bind(0, epoch2); // invalid GUID must clear previous session
    CHECK(!s.Matches(101, epoch2));
    CHECK(s.playerGuidRaw == 0 && s.loginEpoch == 0);
    s.Bind(101, 0);
    CHECK(!s.Matches(101, epoch2));
    s.Bind(102, i.Issue());
    CHECK(!s.Matches(101, epoch1));
    CHECK(!s.Matches(101, epoch2));

    E::Stamp other;
    other.Bind(102, i.Issue());
    CHECK(!other.Matches(s.playerGuidRaw, s.loginEpoch)); // independent epoch
    CHECK(other.Matches(other.playerGuidRaw, other.loginEpoch));
}

static void SafetyCases()
{
    static_assert(!E::AuthorizesCreatureConcealment(), "never authorize hide");
    static_assert(!E::HasProvenClientRestoration(), "no client ACK");
    static_assert(!E::HasProvenOwningMapThread(), "no map-thread authority");
    CHECK(!E::AuthorizesCreatureConcealment());
    CHECK(!E::HasProvenClientRestoration());
    CHECK(!E::HasProvenOwningMapThread());
}

int main()
{
    IssuerCases();
    StampCases();
    SafetyCases();
    std::printf("F14 human viewer epoch pure provenance model: %u passed, %u failed\n", ok, fail);
    return fail ? 1 : 0;
}
