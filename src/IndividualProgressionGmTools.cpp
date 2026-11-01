/*
 * CosmicCuddle Individual Progression — helpers for .ip get / .ip set.
 */

#include "IndividualProgressionGmTools.h"
#include "IndividualProgression.h"
#include "IndividualProgressionGmPolicy.h"
#include "WorldSession.h"

using Acore::ChatCommands::PlayerIdentifier;
using IndividualProgressionExpansionPolicy::Era;

namespace IndividualProgressionGmTools
{
    bool ResolveGetTarget(ChatHandler* handler, Optional<PlayerIdentifier>& player)
    {
        WorldSession* session = handler->GetSession();
        bool const console = !session;
        bool const gameMaster = session && session->GetSecurity() >= SEC_GAMEMASTER;
        Player* self = session ? session->GetPlayer() : nullptr;
        bool const requestedIsSelf = player && self && player->GetGUID() == self->GetGUID();

        switch (IndividualProgressionGmPolicy::DecideGetTarget(console, gameMaster, player.has_value(), requestedIsSelf))
        {
            case IndividualProgressionGmPolicy::GetTarget::RequestedOrSelection:
                return true;
            case IndividualProgressionGmPolicy::GetTarget::Self:
                // Ignore any selected target: an ordinary player sees only themselves.
                player = PlayerIdentifier::FromSelf(handler);
                if (!player)
                {
                    handler->SendSysMessage("Player not found.");
                    return false;
                }
                return true;
            case IndividualProgressionGmPolicy::GetTarget::RefuseOther:
            default:
                handler->SendSysMessage("You can only view your own progression.");
                return false;
        }
    }

    void SendAccessStage(ChatHandler* handler, Player* target)
    {
        if (!target || !sIndividualProgression->IsPaidExpansionActive() || !sIndividualProgression->isNormalAccount(target))
            return;

        handler->PSendSysMessage("Earned stage |cff00ffff{}|r, current access stage |cff00ffff{}|r (paid expansions)",
            sIndividualProgression->GetPlayerProgressionFromQuests(target),
            sIndividualProgression->GetEffectiveProgression(target));
    }

    SetAudit BeginSetAudit(Player* target)
    {
        SetAudit audit;
        audit.active = sIndividualProgression->IsPaidExpansionActive();
        if (!audit.active || !target)
            return audit;

        audit.gated = sIndividualProgression->isNormalAccount(target);
        if (!audit.gated)
            return audit;

        audit.tbcBefore = sIndividualProgression->HasExpansionEntitlement(target, Era::Tbc);
        audit.wrathBefore = sIndividualProgression->HasExpansionEntitlement(target, Era::Wrath);
        return audit;
    }

    void FinishSetAudit(ChatHandler* handler, Player* target, SetAudit const& audit, uint32 oldStage, uint32 newStage)
    {
        if (!audit.active || !audit.gated || !target)
            return;

        bool const tbcAfter = sIndividualProgression->HasExpansionEntitlement(target, Era::Tbc);
        bool const wrathAfter = sIndividualProgression->HasExpansionEntitlement(target, Era::Wrath);
        bool const grantsTbc = IndividualProgressionGmPolicy::NewlyGranted(audit.tbcBefore, tbcAfter);
        bool const grantsWrath = IndividualProgressionGmPolicy::NewlyGranted(audit.wrathBefore, wrathAfter);
        if (!grantsTbc && !grantsWrath)
            return;

        char const* eras = grantsTbc && grantsWrath ? "The Burning Crusade and Wrath of the Lich King"
            : grantsTbc ? "The Burning Crusade" : "Wrath of the Lich King";
        handler->PSendSysMessage("|cffff8000Warning:|r this grants {} to |cff00ffff{}|r without a purchase (paid expansions are on). No purchase record was created.",
            eras, target->GetName());

        WorldSession* session = handler->GetSession();
        std::string const issuer = session
            ? Acore::StringFormat("{} (account {})", session->GetPlayerName(), session->GetAccountId())
            : std::string("console");
        LOG_INFO("module", "IndividualProgression GM audit: {} set character {} from stage {} to {}; TBC {} -> {}, Wrath {} -> {}; no purchase recorded",
            issuer, target->GetGUID().GetCounter(), oldStage, newStage,
            audit.tbcBefore ? "owned" : "not owned", tbcAfter ? "owned" : "not owned",
            audit.wrathBefore ? "owned" : "not owned", wrathAfter ? "owned" : "not owned");
    }
}
