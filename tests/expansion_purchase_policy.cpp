// Standalone tests for the real header used by Individual Progression's
// paid expansion entitlement. No AzerothCore, database, Playerbots or live
// server is needed. Section "M" is a labelled MODEL of the purchase sequence
// in IndividualProgressionExpansion.cpp, not the AzerothCore code itself.
#include "../src/IndividualProgressionExpansionPolicy.h"

#include <cstdint>
#include <iostream>
#include <string>

using namespace IndividualProgressionExpansionPolicy;

namespace
{
    int failures = 0;
    int checks = 0;

    template <typename T>
    void Check(T actual, T expected, std::string const& context)
    {
        ++checks;
        if (!(actual == expected))
        {
            ++failures;
            std::cerr << "FAIL: " << context << " (actual=" << static_cast<int>(actual)
                      << ", expected=" << static_cast<int>(expected) << ")\n";
        }
    }

    CharacterFacts Human(std::uint8_t stage, bool tbcPaid = false, bool wrathPaid = false)
    {
        CharacterFacts f;
        f.featureActive = true;
        f.gated = true;
        f.rawStage = stage;
        f.tbcPaid = tbcPaid;
        f.wrathPaid = wrathPaid;
        return f;
    }

    constexpr std::uint8_t TBC = static_cast<std::uint8_t>(Era::Tbc);
    constexpr std::uint8_t WRATH = static_cast<std::uint8_t>(Era::Wrath);

    // ---- MODEL of IndividualProgression::PurchaseExpansion (labelled) ----
    struct ModelCharacter
    {
        CharacterFacts facts;
        std::uint32_t money = 0;
        int charges = 0;
    };

    PurchaseResult ModelPurchase(ModelCharacter& c, std::uint8_t eraValue, std::uint32_t cost, bool entryAvailable = true)
    {
        PurchaseResult const r = CheckPurchase(c.facts, eraValue, c.money >= cost, entryAvailable);
        if (r != PurchaseResult::Success)
            return r;
        c.money -= cost;
        ++c.charges;
        Era const era = static_cast<Era>(eraValue);
        if (era == Era::Tbc)
        {
            c.facts.tbcPaid = true;
            if (c.facts.rawStage < PurchaseGrantStage(era))
                c.facts.rawStage = PurchaseGrantStage(era);
        }
        else
            c.facts.wrathPaid = true;
        return r;
    }
}

// Compile-time anchors to the established IP stage numbers.
static_assert(VanillaFinalStage == 7 && TbcEntryStage == 8, "Vanilla/TBC boundary");
static_assert(TbcFinalStage == 13 && WrathFirstCompletionStage == 14, "TBC/Wrath boundary");
static_assert(PurchaseGrantStage(Era::Wrath) == 0, "Wrath purchase must not write stage 14");
static_assert(PurchaseGrantStage(Era::Tbc) == 8, "TBC purchase opens the start of TBC only");

int main()
{
    // ---- 1. Feature OFF: exact established behaviour for every stage ----
    for (int s = 0; s <= LastSupportedStage; ++s)
    {
        CharacterFacts off = Human(static_cast<std::uint8_t>(s));
        off.featureActive = false;
        std::string const c = "OFF stage " + std::to_string(s);
        Check(EffectiveStage(off), static_cast<std::uint8_t>(s), c + " effective==raw");
        Check(AutomaticGrantAllowed(off, 18), true, c + " grants unchanged");
        Check(IsEligible(off, Era::Tbc) || IsEligible(off, Era::Wrath), false, c + " no reminders");
        Check(CheckPurchase(off, TBC, true, true), PurchaseResult::FeatureInactive, c + " no purchase");
        Check(MaxServiceGrantableStage(off), LastSupportedStage, c + " no extra service cap");
    }

    // ---- 2. Bots / excluded accounts keep established behaviour ----
    for (int s = 0; s <= LastSupportedStage; ++s)
    {
        CharacterFacts bot = Human(static_cast<std::uint8_t>(s));
        bot.gated = false;
        std::string const c = "bot stage " + std::to_string(s);
        Check(EffectiveStage(bot), static_cast<std::uint8_t>(s), c + " effective==raw");
        Check(AutomaticGrantAllowed(bot, 14), true, c + " automatic grants unchanged");
        Check(CheckPurchase(bot, WRATH, true, true), PurchaseResult::NotApplicable, c + " never asked to pay");
    }

    // ---- 3. Vanilla -> TBC ----
    // Not finished Vanilla: cannot buy even with plenty of gold.
    for (std::uint8_t s = 0; s < VanillaFinalStage; ++s)
    {
        std::string const c = "vanilla stage " + std::to_string(s);
        Check(CheckPurchase(Human(s), TBC, true, true), PurchaseResult::NotEligible, c + " wealthy without boss denied");
        Check(IsEligible(Human(s), Era::Tbc), false, c + " not eligible");
        Check(AutomaticGrantAllowed(Human(s), TbcEntryStage), false, c + " cannot skip into TBC");
        Check(AutomaticGrantAllowed(Human(s), VanillaFinalStage), true, c + " can still earn Vanilla stages");
    }
    // Boss killed (stage 7): eligible, reminded, still Vanilla.
    Check(IsEligible(Human(7), Era::Tbc), true, "KT40 makes TBC purchasable");
    Check(ShouldRemind(Human(7), Era::Tbc), true, "KT40 unpaid gets reminder");
    Check(HasTbc(Human(7)), false, "boss alone does not open TBC");
    Check(HasPassed(Human(7), TbcEntryStage), false, "boss alone: no TBC XP/travel");
    Check(AutomaticGrantAllowed(Human(7), TbcEntryStage), false, "Into the Breach no longer opens TBC unpaid");
    Check(CheckPurchase(Human(7), TBC, false, true), PurchaseResult::InsufficientFunds, "eligible but poor");
    Check(CheckPurchase(Human(7), TBC, true, true), PurchaseResult::Success, "eligible and funded");
    Check(CheckPurchase(Human(7), TBC, true, false), PurchaseResult::EntryUnavailable, "missing marker template fails closed");
    Check(CheckPurchase(Human(7), WRATH, true, true), PurchaseResult::NotEligible, "cannot buy Wrath from Vanilla");
    // Paid: TBC open at its start only.
    Check(HasTbc(Human(8, true)), true, "paid TBC owned");
    Check(HasPassed(Human(8, true), TbcEntryStage), true, "paid TBC: TBC access");
    Check(HasPassed(Human(8, true), 9), false, "paid TBC: no raid tier awarded");
    Check(ShouldRemind(Human(8, true), Era::Tbc), false, "reminder stops after purchase");
    Check(CheckPurchase(Human(8, true), TBC, true, true), PurchaseResult::AlreadyOwned, "repeat TBC purchase");
    // Historical TBC characters (stage 8+ before the feature) keep access.
    Check(HasTbc(Human(8)), true, "historic stage 8 keeps TBC");
    Check(CheckPurchase(Human(10), TBC, true, true), PurchaseResult::AlreadyOwned, "historic TBC never charged");

    // ---- 4. TBC -> Wrath ----
    for (std::uint8_t s = TbcEntryStage; s < TbcFinalStage; ++s)
    {
        std::string const c = "tbc stage " + std::to_string(s);
        Check(CheckPurchase(Human(s, true), WRATH, true, true), PurchaseResult::NotEligible, c + " wealthy without KJ denied");
        Check(AutomaticGrantAllowed(Human(s, true), TbcFinalStage), true, c + " can still earn KJ (13)");
        Check(AutomaticGrantAllowed(Human(s, true), WrathFirstCompletionStage), false, c + " cannot skip into Wrath");
    }
    // KJ killed (stage 13), unpaid: eligible, reminded, still TBC for access.
    Check(IsEligible(Human(13), Era::Wrath), true, "KJ makes Wrath purchasable");
    Check(ShouldRemind(Human(13), Era::Wrath), true, "KJ unpaid gets reminder");
    Check(EffectiveStage(Human(13)), UnpaidWrathAccessCap, "unpaid 13 acts as 12");
    Check(HasPassed(Human(13), WrathAccessStage), false, "unpaid 13: no Northrend / 70+ XP");
    Check(HasPassed(Human(13), 12), true, "unpaid 13 keeps Sunwell access");
    Check(AutomaticGrantAllowed(Human(13), WrathFirstCompletionStage), false, "unpaid 13 cannot gain Wrath tiers");
    Check(CheckPurchase(Human(13), TBC, true, true), PurchaseResult::AlreadyOwned, "stale TBC replay at 13");
    Check(CheckPurchase(Human(13), WRATH, true, true), PurchaseResult::Success, "eligible and funded Wrath");
    // Paid Wrath: access at 13, no tier skipped.
    Check(EffectiveStage(Human(13, false, true)), TbcFinalStage, "paid Wrath restores 13");
    Check(HasPassed(Human(13, false, true), WrathAccessStage), true, "paid Wrath: Northrend open");
    Check(HasPassed(Human(13, false, true), WrathFirstCompletionStage), false, "paid Wrath: Ulduar tier not awarded");
    Check(CheckPurchase(Human(13, false, true), WRATH, true, true), PurchaseResult::AlreadyOwned, "repeat Wrath purchase");
    Check(ShouldRemind(Human(13, false, true), Era::Wrath), false, "Wrath reminder stops");
    // Already completed Wrath content is never revoked.
    for (std::uint8_t s = WrathFirstCompletionStage; s <= LastSupportedStage; ++s)
        Check(EffectiveStage(Human(s)), s, "stage " + std::to_string(s) + " keeps Wrath");

    // ---- 5. Forged / invalid requests: never charged ----
    for (int v : {0, 3, 4, 7, 8, 13, 14, 200, 201, 255})
        Check(CheckPurchase(Human(13), static_cast<std::uint8_t>(v), true, true), PurchaseResult::InvalidRequest,
              "forged era value " + std::to_string(v));

    // ---- 6. Account alt / catch-up cannot buy eligibility ----
    Check(MaxServiceGrantableStage(Human(2)), static_cast<std::uint8_t>(6), "Vanilla catch-up stops before KT40");
    Check(MaxServiceGrantableStage(Human(9, true)), UnpaidWrathAccessCap, "TBC catch-up stops before KJ");
    Check(MaxServiceGrantableStage(Human(13, false, true)), LastSupportedStage, "Wrath-owned: no extra cap");
    Check(CheckPurchase(Human(0), TBC, true, true), PurchaseResult::NotEligible, "fresh alt of a progressed main denied");

    // ---- 7. Start privileges (DK / TBC races / server start stage) ----
    CharacterFacts dk = Human(13);
    dk.startStage = 13;
    Check(HasWrath(dk), true, "DK start privilege keeps Wrath");
    Check(EffectiveStage(dk), static_cast<std::uint8_t>(13), "DK not capped");
    Check(CheckPurchase(dk, WRATH, true, true), PurchaseResult::AlreadyOwned, "DK never charged");
    CharacterFacts draenei = Human(8);
    draenei.startStage = 8;
    Check(HasTbc(draenei), true, "TBC race start keeps TBC");
    Check(IsEligible(draenei, Era::Wrath), false, "TBC race still needs KJ for Wrath");

    // ---- 8. Progression limit ----
    CharacterFacts capped = Human(7);
    capped.progressionLimit = 7;
    Check(CheckPurchase(capped, TBC, true, true), PurchaseResult::BlockedByProgressionLimit, "cap 7 blocks TBC");
    CharacterFacts cappedW = Human(13, true);
    cappedW.progressionLimit = 13;
    Check(CheckPurchase(cappedW, WRATH, true, true), PurchaseResult::BlockedByProgressionLimit, "cap 13 blocks Wrath");

    // ---- 9. Grandfathering (owner option, default OFF) ----
    Check(ShouldGrandfatherWrath(Human(13), false, false), false, "grandfather OFF by default");
    Check(ShouldGrandfatherWrath(Human(13), true, false), true, "grandfather existing 13 once");
    Check(ShouldGrandfatherWrath(Human(13), true, true), false, "never re-evaluated");
    Check(ShouldGrandfatherWrath(Human(12), true, false), false, "below 13 not grandfathered");

    // ---- M. MODEL of the purchase sequence (labelled, not AzerothCore) ----
    {
        ModelCharacter c{Human(7), 5000, 0};
        Check(ModelPurchase(c, TBC, 1000), PurchaseResult::Success, "M: TBC bought");
        Check(c.money, static_cast<std::uint32_t>(4000), "M: charged once");
        Check(c.facts.rawStage, TbcEntryStage, "M: stage 8 only");
        Check(ModelPurchase(c, TBC, 1000), PurchaseResult::AlreadyOwned, "M: double click");
        Check(c.money, static_cast<std::uint32_t>(4000), "M: double click not charged");
        Check(ModelPurchase(c, WRATH, 3000), PurchaseResult::NotEligible, "M: Wrath before KJ");
        Check(c.money, static_cast<std::uint32_t>(4000), "M: ineligible not charged");
        c.facts.rawStage = TbcFinalStage; // earned KJ
        Check(ModelPurchase(c, WRATH, 3000), PurchaseResult::Success, "M: Wrath bought");
        Check(c.facts.rawStage, TbcFinalStage, "M: Wrath purchase writes no stage");
        Check(c.money, static_cast<std::uint32_t>(1000), "M: Wrath charged once");
        Check(c.charges, 2, "M: exactly two charges overall");
        ModelCharacter poor{Human(13, true), 10, 0};
        Check(ModelPurchase(poor, WRATH, 3000), PurchaseResult::InsufficientFunds, "M: poor");
        Check(poor.money, static_cast<std::uint32_t>(10), "M: poor not charged");
        ModelCharacter broken{Human(7), 5000, 0};
        Check(ModelPurchase(broken, TBC, 1000, false), PurchaseResult::EntryUnavailable, "M: missing template");
        Check(broken.money, static_cast<std::uint32_t>(5000), "M: missing template not charged");
    }

    // ---- 10. Effective account ceiling for unlocks (DK / TBC-race creation) ----
    for (int raw = 0; raw <= LastSupportedStage; ++raw)
    {
        std::uint8_t const r = static_cast<std::uint8_t>(raw);
        std::uint8_t const unpaid = raw == 13 ? UnpaidWrathAccessCap : r;
        Check(EffectiveAccountStage(r, false), unpaid, "account stage " + std::to_string(raw) + " unpaid");
        Check(EffectiveAccountStage(r, true), r, "account stage " + std::to_string(raw) + " paid");
    }
    // A DK unlock at 13 needs a paid (or 14+) character on the account.
    Check(EffectiveAccountStage(13, false) >= 13, false, "unpaid 13 cannot unlock a 13-level DK");
    Check(EffectiveAccountStage(13, true) >= 13, true, "paid 13 unlocks it");
    Check(EffectiveAccountStage(14, false) >= 13, true, "completed Wrath tier unlocks it");
    // character_settings data parsing (Core writes "v0 v1 v2 ").
    Check(SettingsSlotSet("0 1 0 ", 1), true, "slot 1 set");
    Check(SettingsSlotSet("0 2 1 ", 1), true, "slot 1 grandfathered");
    Check(SettingsSlotSet("1 0 1 ", 1), false, "slot 1 clear");
    Check(SettingsSlotSet("1 ", 1), false, "slot missing");
    Check(SettingsSlotSet("", 1), false, "empty data");
    Check(SettingsSlotSet("  0   10  ", 1), true, "extra spaces, multi-digit");
    Check(SettingsSlotSet("0 00 ", 1), false, "zero with leading zero");
    Check(SettingsSlotSet("0 x ", 1), false, "non-numeric is not set");
    Check(SettingsSlotSet("1 0 0 ", 0), true, "slot 0 set");

    // ---- L. MODEL of the config / store lifecycle (labelled) ----
    // Uses the real header rules; models Core's EnablePlayerSettings switch,
    // .reload config and the login-time purchase cache.
    {
        struct Server
        {
            bool requestedAtBoot = false;
            bool store = false;
            bool active = false;
            void Boot(bool requested, bool storeOn)
            {
                requestedAtBoot = requested;
                store = storeOn;
                active = FeatureEnabledAfterConfigLoad(false, requested, storeOn);
            }
            void Reload(bool storeOn, bool requestedInFile)
            {
                (void)requestedInFile; // Enable is startup-only: the file value is ignored on reload
                store = storeOn;
                active = FeatureEnabledAfterConfigLoad(true, active, storeOn);
            }
        };
        struct Character
        {
            std::uint8_t raw = 13;
            bool storedWrath = false; // value in character_settings
            bool cachedWrath = false; // value loaded at login
            void Login(Server const& srv) { cachedWrath = srv.store && storedWrath; }
            CharacterFacts Facts(Server const& srv) const
            {
                CharacterFacts f = Human(raw, true, cachedWrath);
                f.featureActive = srv.active;
                return f;
            }
        };

        Server srv;
        srv.Boot(true, true);
        Check(srv.active, true, "L: boot ON with store ON activates");
        Character unpaid; unpaid.Login(srv);
        Character paid; paid.storedWrath = true; paid.Login(srv);
        Check(EffectiveStage(unpaid.Facts(srv)), UnpaidWrathAccessCap, "L: unpaid 13 -> 12");
        Check(EffectiveStage(paid.Facts(srv)), TbcFinalStage, "L: paid 13 -> 13");

        // N1: store switched OFF and .reload config.
        srv.Reload(false, true);
        Check(srv.active, true, "L: reload with store OFF keeps the gate ON");
        Check(EffectiveStage(unpaid.Facts(srv)), UnpaidWrathAccessCap, "L: store OFF + reload: unpaid stays 12");
        Check(HasPassed(unpaid.Facts(srv), WrathAccessStage), false, "L: store OFF + reload: no Northrend");
        Check(AutomaticGrantAllowed(unpaid.Facts(srv), WrathFirstCompletionStage), false, "L: store OFF + reload: cannot earn 14");
        Check(StoreWritesAllowed(srv.active, srv.store), false, "L: store OFF + reload: purchases refused");
        Check(StoreWritesAllowed(srv.active, srv.store) && ShouldGrandfatherWrath(unpaid.Facts(srv), true, false), false,
              "L: store OFF + reload: grandfather never runs on an unset-looking marker");
        Check(EffectiveStage(paid.Facts(srv)), TbcFinalStage, "L: store OFF + reload: online paid holder keeps Wrath");
        Character relog = paid; relog.Login(srv);
        Check(EffectiveStage(relog.Facts(srv)), UnpaidWrathAccessCap, "L: store OFF: relogging paid holder fails closed (12)");
        Check(AutomaticGrantAllowed(relog.Facts(srv), WrathFirstCompletionStage), false, "L: store OFF: relog cannot earn 14");

        // Store restored and reloaded: purchases allowed again, paid holder restored at next login.
        srv.Reload(true, true);
        Check(srv.active, true, "L: store restored: gate still ON");
        Check(StoreWritesAllowed(srv.active, srv.store), true, "L: store restored: purchases allowed");
        relog.Login(srv);
        Check(EffectiveStage(relog.Facts(srv)), TbcFinalStage, "L: store restored: paid holder 13 after relog");
        Check(EffectiveStage(unpaid.Facts(srv)), UnpaidWrathAccessCap, "L: store restored: unpaid still 12");

        // Boot with the store OFF declines activation (historical behaviour, nobody locked out).
        Server off; off.Boot(true, false);
        Check(off.active, false, "L: boot with store OFF stays inactive");
        Check(EffectiveStage(unpaid.Facts(off)), static_cast<std::uint8_t>(13), "L: inactive = historical raw stage");
        off.Reload(true, true);
        Check(off.active, false, "L: reload never turns a boot-inactive feature ON");

        // Feature OFF at boot: parity, and reload cannot enable it.
        Server none; none.Boot(false, true);
        Check(none.active, false, "L: boot OFF");
        none.Reload(true, true);
        Check(none.active, false, "L: reload ignores Enable = 1 written later");
        Check(EffectiveStage(unpaid.Facts(none)), static_cast<std::uint8_t>(13), "L: OFF parity raw 13");
    }

    std::cout << "Paid expansion policy: " << (checks - failures) << "/" << checks << " checks passed\n";
    return failures == 0 ? 0 : 1;
}
