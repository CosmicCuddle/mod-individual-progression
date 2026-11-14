// F14 source-attributed suppression-decision identity — PURE TEST DATA.
// No positive F14 listener exists, no Worldserver launched, no client ACK.
#include "IndividualProgressionF14SuppressionDecisionIdentity.h"
#include <cstdio>
#include <cstdint>

namespace F = IndividualProgressionF14SuppressionDecisionIdentity;
static unsigned passed = 0, failed = 0;
#define CHECK(test) do { if (test) ++passed; else { ++failed; \
  std::printf("FAIL line %d: %s\n", __LINE__, #test); } } while (0)

static F::Facts CurrentSynthetic()
{
    F::Facts f{};
    f.explicitlySourceAttributedF14Decision = true;
    f.currentViewerIsHumanNormalAccount = true;
    f.viewerFullGuid = 0x10000000002ULL;
    f.viewerLoginEpoch = 85;
    f.viewerInWorld = true;
    f.creatureIsWorldNpc = true;
    f.creatureInWorld = true;
    f.creatureFullGuid = 0x30000000555ULL;
    f.creatureIncarnationStamp = 120;
    f.sameLiveCoreMapPointer = true;
    f.viewerMapId = 1;
    f.viewerInstanceId = 77;
    f.creatureMapId = 1;
    f.creatureInstanceId = 77;
    f.registryNotPoisonedAndMatched = true;
    f.registryCreatureFullGuid = f.creatureFullGuid;
    f.registryIncarnationStamp = f.creatureIncarnationStamp;
    f.registryMapId = 1;
    f.registryInstanceId = 77;
    return f;
}

static void AllMissing()
{
    F::Facts f{};
    CHECK(F::Validate(f) == F::Result::NotSourceAttributedF14Decision);
    f.explicitlySourceAttributedF14Decision = true;
    CHECK(F::Validate(f) == F::Result::MissingVerifiedHumanViewer);
}

static void Viewer()
{
    auto f = CurrentSynthetic();
    CHECK(F::Validate(f) == F::Result::CurrentSourceDecisionIdentitySnapshotOnly);
    f.explicitlySourceAttributedF14Decision = false;
    CHECK(F::Validate(f) == F::Result::NotSourceAttributedF14Decision);
    f = CurrentSynthetic();
    f.currentViewerIsHumanNormalAccount = false;
    CHECK(F::Validate(f) == F::Result::MissingVerifiedHumanViewer);
    f = CurrentSynthetic();
    f.viewerFullGuid = 0;
    CHECK(F::Validate(f) == F::Result::MissingVerifiedHumanViewer);
    f = CurrentSynthetic();
    f.viewerLoginEpoch = 0;
    CHECK(F::Validate(f) == F::Result::MissingVerifiedHumanViewer);
    f = CurrentSynthetic();
    f.viewerInWorld = false;
    CHECK(F::Validate(f) == F::Result::MissingVerifiedHumanViewer);
}

static void Creature()
{
    auto f = CurrentSynthetic();
    f.creatureIsWorldNpc = false;
    CHECK(F::Validate(f) == F::Result::MissingLiveCreatureIdentity);
    f = CurrentSynthetic();
    f.creatureInWorld = false;
    CHECK(F::Validate(f) == F::Result::MissingLiveCreatureIdentity);
    f = CurrentSynthetic();
    f.creatureFullGuid = 0;
    CHECK(F::Validate(f) == F::Result::MissingLiveCreatureIdentity);
    f = CurrentSynthetic();
    f.creatureIncarnationStamp = 0;
    CHECK(F::Validate(f) == F::Result::MissingLiveCreatureIdentity);
}

static void Map()
{
    auto f = CurrentSynthetic();
    f.sameLiveCoreMapPointer = false;
    CHECK(F::Validate(f) == F::Result::MismatchedMapOrInstance);
    f = CurrentSynthetic();
    f.viewerMapId = 2;
    CHECK(F::Validate(f) == F::Result::MismatchedMapOrInstance);
    f = CurrentSynthetic();
    f.viewerInstanceId = 76;
    CHECK(F::Validate(f) == F::Result::MismatchedMapOrInstance);
    f = CurrentSynthetic();
    f.creatureMapId = 2;
    CHECK(F::Validate(f) == F::Result::MismatchedMapOrInstance);
    f = CurrentSynthetic();
    f.creatureInstanceId = 76;
    CHECK(F::Validate(f) == F::Result::MismatchedMapOrInstance);
    f = CurrentSynthetic();
    f.registryMapId = 2;
    CHECK(F::Validate(f) == F::Result::MismatchedMapOrInstance);
    f = CurrentSynthetic();
    f.registryInstanceId = 76;
    CHECK(F::Validate(f) == F::Result::MismatchedMapOrInstance);
}

static void Registry()
{
    auto f = CurrentSynthetic();
    f.registryNotPoisonedAndMatched = false;
    CHECK(F::Validate(f) == F::Result::StaleOrUnknownRegistryIncarnation);
    f = CurrentSynthetic();
    f.registryCreatureFullGuid += 1;
    CHECK(F::Validate(f) == F::Result::StaleOrUnknownRegistryIncarnation);
    f = CurrentSynthetic();
    f.registryCreatureFullGuid = 0;
    CHECK(F::Validate(f) == F::Result::StaleOrUnknownRegistryIncarnation);
    f = CurrentSynthetic();
    f.registryIncarnationStamp += 1; // creature GUID reused after respawn
    CHECK(F::Validate(f) == F::Result::StaleOrUnknownRegistryIncarnation);
    f = CurrentSynthetic();
    f.registryIncarnationStamp = 0;
    CHECK(F::Validate(f) == F::Result::StaleOrUnknownRegistryIncarnation);
    f = CurrentSynthetic();
    f.creatureIncarnationStamp = 121;
    CHECK(F::Validate(f) == F::Result::StaleOrUnknownRegistryIncarnation);
}

static void NeverAuthority()
{
    static_assert(!F::ProvesOwningMapThread(), "map pointer != thread proof");
    static_assert(!F::ProvesClientVisibilityOrRestoration(), "no object ACK");
    static_assert(!F::AuthorizesConcealment(), "never positive F14 policy");
    static_assert(!F::AuthorizesPriorHiddenLedgerMutation(), "no hidden ledger");
    CHECK(!F::ProvesOwningMapThread());
    CHECK(!F::ProvesClientVisibilityOrRestoration());
    CHECK(!F::AuthorizesConcealment());
    CHECK(!F::AuthorizesPriorHiddenLedgerMutation());
}

int main()
{
    AllMissing();
    Viewer();
    Creature();
    Map();
    Registry();
    NeverAuthority();
    std::printf("F14 current decision identity source model: %u passed, %u failed\n", passed, failed);
    return failed ? 1 : 0;
}
