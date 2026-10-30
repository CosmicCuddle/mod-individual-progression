"""F14 IP stage-write *contract model* only; NOT a runtime implementation.

CosmicCuddle's Blood Elf/Draenei RandomBot requirement:
- A positively identified RandomBot of race 10/11 must not use IP stage 0..7.
- During TBC ineligibility the saved bot is dormant, never deleted or up-leveled.
- Existing rewarded progression markers must survive relog/restart; no stage-0
  reset and no automatic migration of a saved stage-0..7 character.
- Other identities/races are NOT subject to this F14-specific rule.

The caller must provide independently verified identity and TBC availability.
This module does not classify Playerbots, inspect the database, infer an era
from character level/ProgressionLimit=0, alter quests, schedule bots, or touch
an owner host. It returns a proposed *test expectation* for future code review.
"""
from dataclasses import dataclass
from enum import Enum
from typing import Optional


class Identity(str, Enum):
    RANDOM_BOT = "RandomBot"
    HUMAN = "Human"
    ALT_BOT = "AltBot"
    SELF_BOT = "SelfBot"
    UNKNOWN = "Unknown"


class TbcEligibility(str, Enum):
    AVAILABLE = "verified_tbc_available"
    UNAVAILABLE = "verified_tbc_unavailable"
    UNKNOWN = "unverified"


class Route(str, Enum):
    NEW_SELECTION = "new_population_selection"
    SAVED_EVENT_RESTORE = "saved_add_event_restore"
    RELOGIN = "relogin"
    MANUAL_LOGIN = "manual_bot_login"
    GROUP_ACCEPT = "group_accept"
    GROUP_SYNC = "group_sync"
    GM_STAGE_WRITE = "gm_stage_write"
    PROGRESSION_EVENT = "progression_event"


class Disposition(str, Enum):
    DEFER_TO_EXISTING = "not_an_f14_rndbot"
    KEEP_DORMANT = "ineligible_or_unverified_tbc"
    HOLD_MIGRATION = "saved_invalid_or_missing_stage"
    INITIALIZE_NEW = "new_eligible_stage8"
    PRESERVE = "keep_existing_rewarded_stage_quests"
    REJECT_WRITE = "forbidden_stage_write"
    ALLOW_VALID_WRITE = "legal_tbc_plus_write"


@dataclass(frozen=True)
class Decision:
    disposition: Disposition
    # None means use OTHER existing game rules, not unconditional permission.
    may_activate: Optional[bool]
    permitted_stage_write: bool
    proposed_stage: Optional[int] = None


def decide(
    identity: Identity,
    race: int,
    tbc_eligibility: TbcEligibility,
    route: Route,
    existing_stage: Optional[int],
    requested_stage: Optional[int] = None,
) -> Decision:
    """Pure proposed guard, NOT plugged into IP/Playerbots.

    existing_stage=None is reserved for a genuinely *new* unsaved character.
    An offline or uncertain SAVED stage is never guessed from level or race.
    A proposed stage-write is checked on EVERY route, including group sync.
    """
    if identity is not Identity.RANDOM_BOT or race not in (10, 11):
        return Decision(Disposition.DEFER_TO_EXISTING, None, False)

    # Unverified eligibility is NOT equivalent to the distributable
    # ProgressionLimit=0 setting, nor to a level-60 server.
    if tbc_eligibility is not TbcEligibility.AVAILABLE:
        return Decision(Disposition.KEEP_DORMANT, False, False)

    if existing_stage is None:
        if route is Route.NEW_SELECTION and requested_stage == 8:
            return Decision(Disposition.INITIALIZE_NEW, True, True, 8)
        return Decision(Disposition.HOLD_MIGRATION, False, False)

    if not 8 <= existing_stage <= 18:
        return Decision(Disposition.HOLD_MIGRATION, False, False)

    if requested_stage is None or requested_stage == existing_stage:
        return Decision(Disposition.PRESERVE, True, False)

    if not 8 <= requested_stage <= 18:
        return Decision(Disposition.REJECT_WRITE, True, False)

    return Decision(Disposition.ALLOW_VALID_WRITE, True, True, requested_stage)
