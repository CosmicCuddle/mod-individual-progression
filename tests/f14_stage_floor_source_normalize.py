"""Exactly subtract the opt-in F14 login stage-floor pilot from reviewed
historical IP source contracts. Parent source comparison below remains exact:
new unrelated changes in login, policy, or header fail the inherited tests.
This helper does not execute gameplay code or grant progression.
"""

FROZEN_PARENT = "a6ac77d9fae9690be91f6ddb97422476164c0ca2"

START = "            // F14 NEW owner-priority SOURCE PILOT, default OFF."
END = "            // END F14 new verified RandomBot stage-floor block.\n"
NEW_GATE = "if (!preserveVerifiedRandomBotStage && !verifiedRandomBotTbcFloorHandled)"
ORIGINAL_GATE = "if (!preserveVerifiedRandomBotStage)"

POLICY_BEGIN = "    // F14 new-bot source candidate: a positively verified Blood Elf or\n"
POLICY_END = "    // F19-1: never preserve a stage below the original level stage."

# Frozen exact F14 policy body. Prevent normalization from hiding unrelated
# changes between the F14 and F19 markers (Grok Low-2 / mutation M8).
EXPECTED_POLICY_BODY = """    // Draenei RandomBot needs at least TBC stage 8, even when level <=60.
    // At level 71+ the original bot stage floor of 13 is retained.
    // This is a RAW awarded-marker target, not a claim the global Vanilla
    // ProgressionLimit makes the bot eligible for TBC gameplay.
    constexpr std::uint8_t TbcRaceFloorStage(std::uint32_t level)
    {
        return LevelStage(level) < 8 ? 8 : LevelStage(level);
    }

    constexpr bool NeedsTbcRaceFloor(std::uint8_t savedStage, std::uint32_t level)
    {
        return savedStage < TbcRaceFloorStage(level);
    }

"""

HEADER_OLD = "enforceGroupRules, f14PreserveVerifiedRandomBotStageOnLogin, EnableSetRepCommand"
HEADER_NEW = "enforceGroupRules, f14PreserveVerifiedRandomBotStageOnLogin, f14EnforceVerifiedRandomBotTbcFloorOnLogin, EnableSetRepCommand"


def strip_login_pilot(source: str) -> str:
    """Undo only the well-delimited pilot and its single gate substitution."""
    assert source.count(START) == 1, "F14 pilot start missing/duplicated"
    assert source.count(END) == 1, "F14 pilot end missing/duplicated"
    prefix, body = source.split(START, 1)
    _, suffix = body.split(END, 1)
    result = prefix + suffix.lstrip("\n")
    assert result.count(NEW_GATE) == 1, "F14 pilot combined gate drift"
    result = result.replace(NEW_GATE, ORIGINAL_GATE, 1)
    assert "verifiedRandomBotTbcFloorHandled" not in result
    return result


def strip_policy_pilot(source: str) -> str:
    assert source.count(POLICY_BEGIN) == 1, "F14 pilot policy start missing/duplicated"
    assert source.count(POLICY_END) == 1, "F19 policy region missing/duplicated"
    prefix, body = source.split(POLICY_BEGIN, 1)
    pilot, suffix = body.split(POLICY_END, 1)
    assert pilot == EXPECTED_POLICY_BODY, "F14 policy body drift outside reviewed pilot"
    assert "TbcRaceFloorStage" not in prefix + suffix
    return prefix + POLICY_END + suffix


def strip_header_pilot(source: str) -> str:
    assert source.count(HEADER_NEW) == 1, "F14 bool declaration not uniquely matched"
    return source.replace(HEADER_NEW, HEADER_OLD, 1)
