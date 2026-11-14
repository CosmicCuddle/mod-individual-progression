/*
 * CosmicCuddle Individual Progression — F14 verified RandomBot TBC floor:
 * post re-roll repair (default OFF; see IndividualProgressionF14TbcFloorPolicy.h).
 *
 * Playerbots' full re-roll (PlayerbotFactory::Randomize(false) ->
 * ResetQuests) removes every rewarded quest, including IP's stage markers
 * 66001-66018, so a verified BE/Draenei RandomBot drops to stage 0 while
 * online. This world-thread pass re-applies the floor. It runs from the
 * world update, after the map update has finished, visits only GUIDs that
 * already passed the login-thread identity check, and writes only through
 * IndividualProgression::ForceUpdateProgressionState.
 */

#include "IndividualProgression.h"
#include "ScriptMgr.h"
#include "WorldScript.h"

namespace
{
    constexpr uint32 F14TbcFloorRepairIntervalMs = 5000;

    class F14TbcFloorRepairWorldScript final : public WorldScript
    {
    public:
        F14TbcFloorRepairWorldScript() : WorldScript("IndividualProgression_F14TbcFloorRepair",
            {WORLDHOOK_ON_UPDATE}) { }

        void OnUpdate(uint32 diff) override
        {
            if (!sIndividualProgression->enabled || !sIndividualProgression->f14VerifiedRandomBotTbcFloor)
            {
                _elapsed = 0;
                return;
            }
            _elapsed += diff;
            if (_elapsed < F14TbcFloorRepairIntervalMs)
                return;
            _elapsed = 0;
            sIndividualProgression->RepairF14VerifiedRandomBotTbcFloors();
        }

    private:
        uint32 _elapsed = 0;
    };
}

void AddSC_mod_individual_progression_f14_tbc_floor()
{
    new F14TbcFloorRepairWorldScript();
}
