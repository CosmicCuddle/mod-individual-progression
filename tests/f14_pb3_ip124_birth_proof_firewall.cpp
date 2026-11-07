// Native TEST-ONLY composition of two ACTUAL header implementations:
// PB #3 F14NewRandomBotBirthProbe and IP #124 first-start policy model.
// This fixture cannot change real bot/character/quest/paid/client/DB state.
// A same-process creation-candidate + headless login observation is NOT
// sufficient to assert a committed DB birth or an empty pre-existing row.
#include "F14NewRandomBotBirthProbe.h"
#include "f14_first_start_eligibility_model.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

using CosmicF14::NewRandomBotBirthProbe;
using CosmicF14FirstStartModel::Facts;
using CosmicF14FirstStartModel::Verdict;
using CosmicF14FirstStartModel::Decide;

static void Check(bool ok, char const* label)
{
    if (!ok)
    {
        std::fprintf(stderr, "FAIL PB3 x IP124 first-start safety: %s\n", label);
        std::exit(1);
    }
}

// TEST FIXTURE ONLY. Every other policy input is deliberately maximally
// permissive to challenge the boundary between OBSERVED and DURABLE.
// Never use this function as a real server input adapter or stage writer.
static Facts OnlyDiagnosticEvidence(NewRandomBotBirthProbe const& probe,
                                    std::uint32_t guid, std::uint32_t account,
                                    std::uint8_t race, std::uint8_t savedStage,
                                    std::int32_t limit = 18)
{
    Facts facts;
    facts.PilotEnabled = true; // synthetic; F14 remains OFF in actual configs
    facts.PositiveVerifiedRandomBot = probe.HasUnretiredAdmission(guid, account, race);
    facts.Headless = true; // from this synthetic fixture, NOT a live session
    facts.Race = race;
    facts.SavedStage = savedStage;
    facts.ProgressionLimit = limit;
    facts.WorldTbcEnabled = true;
    facts.OwnerApprovedBotStartingException = true; // hypothetical, NOT owner policy
    facts.PaidEraFeatureEnabled = false;

    // Explicit trust firewall: PB #3 does NOT know whether the queued
    // SaveToDB transaction committed, nor whether the character already
    // has stored progression, paid purchases or quest markers.
    facts.GenuineFirstCreationEvidence = false;
    facts.ExistingCharacterData = true; // unknown MUST remain existing
    return facts;
}

int main()
{
    NewRandomBotBirthProbe& probe = NewRandomBotBirthProbe::Instance();
    constexpr std::uint32_t guid = 8421, account = 733;
    constexpr std::uint8_t be = 10, draenei = 11;

    // Simulate ordinary lifecycle of the REAL default-OFF PB #3 header.
    Check(!probe.HasUnretiredAdmission(guid, account, be), "unseen bot not admitted");
    Check(Decide(OnlyDiagnosticEvidence(probe, guid, account, be, 0)) ==
          Verdict::NoAction, "unborn actor cannot receive first-start stage");
    Check(!probe.ObserveAdmission(guid, account, be, true, true, true, true),
          "login alone cannot invent creation");
    Check(probe.RecordCandidate(guid, account, be),
          "valid newly generated candidate accepted in fixture");
    Check(!probe.HasUnretiredAdmission(guid, account, be),
          "candidate alone cannot prove login");
    Check(Decide(OnlyDiagnosticEvidence(probe, guid, account, be, 0)) ==
          Verdict::NoAction, "queued async SaveToDB does not prove committed birth");
    Check(!probe.ObserveAdmission(guid, account, be, false, true, true, true),
          "non-randombot not admitted");
    Check(!probe.ObserveAdmission(guid, account, be, true, false, true, true),
          "nonheadless login not admitted");
    Check(!probe.ObserveAdmission(guid, account, be, true, true, false, true),
          "not in world login not admitted");
    Check(!probe.ObserveAdmission(guid, account, be, true, true, true, false),
          "no PlayerbotAI not admitted");
    Check(!probe.ObserveAdmission(guid, account+1, be, true, true, true, true),
          "other account cannot steal candidate");
    Check(!probe.ObserveAdmission(guid, account, draenei, true, true, true, true),
          "other race cannot steal candidate");

    Check(probe.ObserveAdmission(guid, account, be, true, true, true, true),
          "observed genuine in-process diagnostic handoff");
    Check(probe.HasUnretiredAdmission(guid, account, be),
          "same-process observation recorded");

    // Crucial cross-module invariant: even the REAL positive PB #3 observer
    // does not supply IP #124's required durable first-birth/absence proofs.
    for (unsigned saved = 0; saved < 8; ++saved)
    {
        Facts f = OnlyDiagnosticEvidence(probe, guid, account, be,
                                          static_cast<std::uint8_t>(saved));
        Check(Decide(f) != Verdict::EligibleNew,
              "observed PB3 + saved stage0..7 MUST NOT auto-grant stage8");
    }
    Check(Decide(OnlyDiagnosticEvidence(probe, guid, account, be, 8)) ==
          Verdict::PreserveEarned, "already earned stage8 preserved, not granted");
    Check(Decide(OnlyDiagnosticEvidence(probe, guid, account, be, 13)) ==
          Verdict::PreserveEarned, "already earned stage13 preserved, not downgraded");
    Check(Decide(OnlyDiagnosticEvidence(probe, guid, account, be, 13, 8)) ==
          Verdict::NeedsReview, "saved stage above current cap is not rewritten");
    Check(Decide(OnlyDiagnosticEvidence(probe, guid, account, be, 0, 0)) ==
          Verdict::NeedsReview, "uncapped is not durable creation proof");
    Check(Decide(OnlyDiagnosticEvidence(probe, guid, account, be, 0, 7)) ==
          Verdict::NeedsReview, "Vanilla cap never permits selected TBC start");

    Facts paid = OnlyDiagnosticEvidence(probe, guid, account, be, 0);
    paid.PaidEraFeatureEnabled = true;
    Check(Decide(paid) == Verdict::NeedsReview,
          "paid expansion feature cannot be bypassed by PB observer");
    paid.OwnerApprovedPaidBotException = true; // STILL not a durable birth
    Check(Decide(paid) == Verdict::NeedsReview,
          "even hypothetical paid exception cannot replace birth proof");

    // A positive result in the IP MODEL exists ONLY when both privileged
    // facts are supplied by a separate trusted future owner-approved source.
    Facts hypothetical = OnlyDiagnosticEvidence(probe, guid, account, be, 0);
    hypothetical.GenuineFirstCreationEvidence = true;
    hypothetical.ExistingCharacterData = false;
    Check(Decide(hypothetical) == Verdict::EligibleNew,
          "pure model requires BOTH missing separate trusted prerequisites");
    hypothetical.ExistingCharacterData = true;
    Check(Decide(hypothetical) == Verdict::NeedsReview,
          "durable birth alone does not prove empty existing character data");
    hypothetical.ExistingCharacterData = false;
    hypothetical.GenuineFirstCreationEvidence = false;
    Check(Decide(hypothetical) == Verdict::NeedsReview,
          "absent old data alone does not prove genuine first creation");

    probe.Retire(guid);
    Check(!probe.HasUnretiredAdmission(guid, account, be),
          "logout retires observation");
    Check(Decide(OnlyDiagnosticEvidence(probe, guid, account, be, 0)) ==
          Verdict::NoAction, "retired actor cannot regain eligibility");
    Check(!probe.ObserveAdmission(guid, account, be, true, true, true, true),
          "relog cannot replay retired admission");
    Check(!probe.RecordCandidate(guid, account, be),
          "repeated GUID cannot replace retained retired record");

    // Other race has no candidate, no verified admission, no stage grant.
    Check(Decide(OnlyDiagnosticEvidence(probe, 123, account, 1, 0)) ==
          Verdict::NoAction, "normal human character excluded");
    Check(Decide(OnlyDiagnosticEvidence(probe, 123, account, draenei, 0)) ==
          Verdict::NoAction, "unobserved Draenei excluded");

    std::puts("PASS: actual PB #3 observer + IP #124 first-start model only; "
              "no durable creation proof and NO stage writer");
    return 0;
}
