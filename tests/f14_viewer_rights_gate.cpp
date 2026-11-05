// PURE model regression: combined human rights + viewer restore proof gate.
// Every positive fact in this file is SYNTHETIC, not a real Core/game/client
// attestation. This test never sends packets or changes a live creature.
#include "IndividualProgressionF14ViewerRightsGate.h"

#include <cstdint>
#include <cstdio>

namespace G = IndividualProgressionF14ViewerRightsGate;
namespace V = IndividualProgressionF14CombatVisibilityPolicy;
namespace R = IndividualProgressionF14ViewerRestoreProtocol;

static unsigned ok = 0;
static unsigned bad = 0;
#define CHECK(c) do { if (c) ++ok; else { ++bad; \
    std::printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

using Protocol = R::Protocol<4>;
static R::ViewerKey const viewer{100, 10};
static R::CreatureKey const creature{0, 0, 901};
static std::uint64_t const incarnation = 9009;
static std::uint64_t const uniqueCreatureIdentity = 4555;

// All facts are fabricated for MODEL consistency tests. A real consumer has
// no authoritative source for full human rights / client restore today.
static G::JointFacts SyntheticHealthy()
{
    G::JointFacts f;
    f.viewer = viewer;
    f.creature = creature;
    f.creatureIncarnation = incarnation;
    f.onConfirmedOwningMapThread = true;
    f.exactVerifiedSelectedRandomBotIdentity = true;
    f.sameLiveCreatureIncarnationIndependentlyProven = true;
    f.independentFullHumanPairRightsProven = true;

    auto& c = f.combat;
    c.complete = true;
    c.current = true;
    c.plainUnscriptedNonEliteHostileNpc = true;
    c.alive = true;
    c.inCombat = true;
    c.eligibleHiddenRandomBotCurrentlyAttackingThisCreature = true;
    c.everyParticipantVerifiedTbcRaceRandomBotOrItsMinion = true;
    c.noRealPlayerCombatThreatTapOrRights = true;
    c.noPendingThreatOrCombatantChanges = true;
    c.creatureIdentityToken = uniqueCreatureIdentity;
    c.generation = 7;

    auto& o = f.observer;
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
    o.checkedAgainstCreatureIdentityToken = uniqueCreatureIdentity;
    o.checkedAgainstCreatureGeneration = 7;
    o.targetIsAliveAtThisEvaluation = true;
    o.eligibleHiddenAttackerStillAttackingThisTargetAtThisEvaluation = true;

    auto& d = f.decision;
    d.configEnabled = true;
    d.actorPositivelyVerifiedRandomBot = true;
    d.actorEntitled = true;
    // Even a malicious prefilled true is ignored unless independent
    // full human rights authority and combat/observer model are proven.
    d.pairRightsProven = false;
    d.creatureAlive = true;
    d.currentViewerEpoch = viewer.loginEpoch;
    d.currentCreatureIncarnation = incarnation;
    d.eventMapId = creature.mapId;
    d.eventInstanceId = creature.instanceId;
    return f;
}

static void TestDefaultFailsVisible()
{
    Protocol p;
    G::JointFacts f;
    CHECK(!G::IsLiveCreatureConcealmentAuthorized());
    CHECK(G::CheckHypotheticalSuppressionOnly(p, f) == G::GateReason::InvalidPair);
    f = SyntheticHealthy();
    f.onConfirmedOwningMapThread = false;
    CHECK(G::CheckHypotheticalSuppressionOnly(p, f) == G::GateReason::WrongOrUnverifiedMapThread);
    f = SyntheticHealthy();
    f.decision.eventInstanceId = 2;
    CHECK(G::CheckHypotheticalSuppressionOnly(p, f) == G::GateReason::WrongOrUnverifiedMapThread);
    f = SyntheticHealthy();
    f.exactVerifiedSelectedRandomBotIdentity = false; // human/Altbot/unverified
    CHECK(G::CheckHypotheticalSuppressionOnly(p, f) == G::GateReason::MissingActorIdentityOrIncarnationAuthority);
    f = SyntheticHealthy();
    f.sameLiveCreatureIncarnationIndependentlyProven = false; // recycled GUID
    CHECK(G::CheckHypotheticalSuppressionOnly(p, f) == G::GateReason::MissingActorIdentityOrIncarnationAuthority);
    f = SyntheticHealthy();
    f.decision.currentViewerEpoch += 1; // relog
    CHECK(G::CheckHypotheticalSuppressionOnly(p, f) == G::GateReason::MissingActorIdentityOrIncarnationAuthority);
    f = SyntheticHealthy();
    f.decision.currentCreatureIncarnation += 1; // re-used spawn
    CHECK(G::CheckHypotheticalSuppressionOnly(p, f) == G::GateReason::MissingActorIdentityOrIncarnationAuthority);
}

static void TestHumanRightsMustBeIndependent()
{
    Protocol p;
    G::JointFacts f = SyntheticHealthy();
    f.independentFullHumanPairRightsProven = false;
    f.decision.pairRightsProven = true; // malicious caller cannot bypass
    CHECK(G::CheckHypotheticalSuppressionOnly(p, f) == G::GateReason::MissingIndependentHumanRightsAuthority);
    f = SyntheticHealthy();
    f.observer.noObserverOrGroupQuestLootTapSkinOrKillCredit = false;
    CHECK(G::CheckHypotheticalSuppressionOnly(p, f) == G::GateReason::CombatOrObserverPolicyDenied);
    f = SyntheticHealthy();
    f.observer.noObserverOrOwnedUnitThreatCombatOrPendingAssist = false;
    CHECK(G::CheckHypotheticalSuppressionOnly(p, f) == G::GateReason::CombatOrObserverPolicyDenied);
    f = SyntheticHealthy();
    f.observer.noOwnTargetAuraPetTotemGuardianOrSpellInFlight = false;
    CHECK(G::CheckHypotheticalSuppressionOnly(p, f) == G::GateReason::CombatOrObserverPolicyDenied);
    f = SyntheticHealthy();
    f.observer.eligibleHiddenAttackerStillAttackingThisTargetAtThisEvaluation = false;
    CHECK(G::CheckHypotheticalSuppressionOnly(p, f) == G::GateReason::CombatOrObserverPolicyDenied);
    f = SyntheticHealthy();
    f.observer.targetIsAliveAtThisEvaluation = false; // death/respawn
    CHECK(G::CheckHypotheticalSuppressionOnly(p, f) == G::GateReason::CombatOrObserverPolicyDenied);
    f = SyntheticHealthy();
    f.combat.noPendingThreatOrCombatantChanges = false;
    CHECK(G::CheckHypotheticalSuppressionOnly(p, f) == G::GateReason::CombatOrObserverPolicyDenied);
    f = SyntheticHealthy();
    f.observer.checkedAgainstCreatureIdentityToken = 99;
    CHECK(G::CheckHypotheticalSuppressionOnly(p, f) == G::GateReason::CombatOrObserverPolicyDenied);
    f = SyntheticHealthy();
    f.observer.checkedAgainstCreatureGeneration = 6;
    CHECK(G::CheckHypotheticalSuppressionOnly(p, f) == G::GateReason::CombatOrObserverPolicyDenied);
}

static void TestPriorHiddenObligationsAreNeverBypassed()
{
    Protocol p;
    G::JointFacts f = SyntheticHealthy();
    CHECK(G::CheckHypotheticalSuppressionOnly(p, f) == G::GateReason::HypotheticalModelOnly);
    CHECK(p.Size() == 0 && p.LedgerView().Size() == 0); // pure query side-effect free
    CHECK(!G::IsLiveCreatureConcealmentAuthorized());

    // Synthetic stage-2 F14 suppression then invalidation. Only the pure
    // protocol records obligation; this gate has NO ledger clearance API.
    R::DecisionFacts d = f.decision;
    d.pairRightsProven = true;
    CHECK(p.ObserveF14SuppressionDecision(viewer, creature, incarnation, true,
                                          R::VisibilityPath::Batched, d) ==
          R::Outcome::SuppressionRecorded);
    p.ObserveCreatureRetired(creature);
    CHECK(p.NeedsRestore(viewer, creature));
    CHECK(G::CheckHypotheticalSuppressionOnly(p, f) == G::GateReason::PriorHiddenProtocolDenied);
    CHECK(p.NeedsRestore(viewer, creature));
    CHECK(p.LedgerView().PendingRestores() == 1);
    CHECK(!G::IsLiveCreatureConcealmentAuthorized());
}

int main()
{
    TestDefaultFailsVisible();
    TestHumanRightsMustBeIndependent();
    TestPriorHiddenObligationsAreNeverBypassed();
    std::printf("F14 joint policy model: %u passed, %u failed\n", ok, bad);
    return bad ? 1 : 0;
}
