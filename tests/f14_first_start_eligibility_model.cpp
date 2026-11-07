// Portable TEST-ONLY F14 first-start decision and negative-case matrix.
// No Core/IP/Playerbots runtime code or database is invoked or mutated.
#include "f14_first_start_eligibility_model.h"
#include <cstdint>
#include <iostream>

using namespace CosmicF14FirstStartModel;

constexpr Facts Good()
{
    Facts f;
    f.PilotEnabled = true;
    f.PositiveVerifiedRandomBot = true;
    f.Headless = true;
    f.GenuineFirstCreationEvidence = true;
    f.ExistingCharacterData = false;
    f.WorldTbcEnabled = true;
    f.OwnerApprovedBotStartingException = true;
    f.Race = 10;
    f.SavedStage = 0;
    f.ProgressionLimit = 8;
    return f;
}

static_assert(Decide(Good()) == Verdict::EligibleNew, "fully explicitly eligible new random-bot");
static_assert(Decide(Facts{}) == Verdict::NoAction, "unset input fails closed");

int main()
{
    int checks = 0, errors = 0;
    auto check = [&](Verdict actual, Verdict expected, char const* context) {
        ++checks;
        if (actual != expected)
        {
            ++errors;
            std::cerr << "FAIL " << context << " (" << int(actual) << " vs " << int(expected) << ")\n";
        }
    };
    Facts base = Good();
    auto checkNoGrant = [&](Facts f, char const* why) {
        Verdict value = Decide(f);
        ++checks;
        if (value == Verdict::EligibleNew)
        {
            ++errors;
            std::cerr << "FAIL unsafe first-start grant: " << why << "\n";
        }
    };
    for (int race = 1; race <= 12; ++race)
        for (int saved = 0; saved <= 19; ++saved)
            for (int limit : {-1, 0, 7, 8, 12, 13, 18, 19})
                for (int existing = 0; existing <= 1; ++existing)
                    for (int first = 0; first <= 1; ++first)
                    {
                        Facts f = base;
                        f.Race = static_cast<std::uint8_t>(race);
                        f.SavedStage = static_cast<std::uint8_t>(saved);
                        f.ProgressionLimit = limit;
                        f.ExistingCharacterData = existing != 0;
                        f.GenuineFirstCreationEvidence = first != 0;
                        Verdict expected = Verdict::NeedsReview;
                        if (race != 10 && race != 11)
                            expected = Verdict::NoAction;
                        else if (limit < 0 || limit > 18 || saved > 18 ||
                                 (limit != 0 && limit < 8))
                            expected = Verdict::NeedsReview;
                        else if (saved >= 8)
                            expected = limit != 0 && saved > limit ?
                                Verdict::NeedsReview : Verdict::PreserveEarned;
                        else if (saved == 0 && !existing && first)
                            expected = Verdict::EligibleNew;
                        check(Decide(f), expected, "full race/stage/cap/existing/first matrix");
                    }

    for (int bit = 0; bit < 9; ++bit)
    {
        Facts f = base;
        switch (bit)
        {
        case 0: f.PilotEnabled = false; break;
        case 1: f.PositiveVerifiedRandomBot = false; break;
        case 2: f.Headless = false; break;
        case 3: f.GenuineFirstCreationEvidence = false; break;
        case 4: f.ExistingCharacterData = true; break;
        case 5: f.WorldTbcEnabled = false; break;
        case 6: f.OwnerApprovedBotStartingException = false; break;
        case 7: f.PaidEraFeatureEnabled = true; break;
        case 8: f.ProgressionLimit = -1; break;
        }
        checkNoGrant(f, "missing positive proof / exception");
    }
    Facts paid = base;
    paid.PaidEraFeatureEnabled = true;
    paid.OwnerApprovedPaidBotException = true;
    check(Decide(paid), Verdict::EligibleNew, "hypothetical separately approved paid exemption only");
    paid.OwnerApprovedPaidBotException = false;
    checkNoGrant(paid, "paid feature cannot bypass receipt without specific exception");
    Facts saved = base;
    saved.SavedStage = 12;
    saved.ExistingCharacterData = true;
    saved.GenuineFirstCreationEvidence = false;
    saved.ProgressionLimit = 13;
    check(Decide(saved), Verdict::PreserveEarned, "higher earned tier is never overwritten");
    saved.ProgressionLimit = 8;
    check(Decide(saved), Verdict::NeedsReview, "saved stage above cap not normalized silently");
    saved.SavedStage = 4;
    saved.ProgressionLimit = 13;
    check(Decide(saved), Verdict::NeedsReview, "existing tier4 requires review; no automatic migration");
    std::cout << "PASS test-only F14 first-start eligibility model: "
              << (checks-errors) << "/" << checks << " combinations\n";
    return errors ? 1 : 0;
}
