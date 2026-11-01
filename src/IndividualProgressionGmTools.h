/*
 * CosmicCuddle Individual Progression — helpers for .ip get / .ip set.
 * Rules: IndividualProgressionGmPolicy.h. Paid expansion API: #23.
 */
#ifndef INDIVIDUAL_PROGRESSION_GM_TOOLS_H
#define INDIVIDUAL_PROGRESSION_GM_TOOLS_H

#include "Chat.h"
#include "ChatCommand.h"

class Player;

namespace IndividualProgressionGmTools
{
    // .ip get: resolves who the issuer may query. Ordinary players are
    // limited to themselves; returns false (message sent) when refused.
    bool ResolveGetTarget(ChatHandler* handler, Optional<Acore::ChatCommands::PlayerIdentifier>& player);

    // .ip get: with paid expansions ON, adds the earned and current access
    // stages for a normal-account target. Nothing is added when OFF.
    void SendAccessStage(ChatHandler* handler, Player* target);

    struct SetAudit
    {
        bool active = false;
        bool gated = false;
        bool tbcBefore = true;
        bool wrathBefore = true;
    };

    // .ip set: era ownership before the existing write.
    SetAudit BeginSetAudit(Player* target);

    // .ip set: warns the GM and writes one audit line when the write newly
    // granted TBC or Wrath without a purchase. Never writes purchase flags.
    void FinishSetAudit(ChatHandler* handler, Player* target, SetAudit const& audit, uint32 oldStage, uint32 newStage);
}

#endif
