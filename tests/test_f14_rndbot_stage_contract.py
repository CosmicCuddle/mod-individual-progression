"""Deterministic F14 policy regression model. Synthetic data ONLY."""
import itertools
import unittest

from f14_rndbot_stage_contract import (
    Disposition, Identity, Route, TbcEligibility, decide,
)


class F14StageContractTests(unittest.TestCase):
    def test_human_alt_self_unknown_are_never_overridden(self):
        for identity, race, stage, requested, eligibility, route in itertools.product(
            (Identity.HUMAN, Identity.ALT_BOT, Identity.SELF_BOT, Identity.UNKNOWN),
            (1, 10, 11), (0, 7, 8, 18, None),
            (0, 7, 8, None), tuple(TbcEligibility), tuple(Route)
        ):
            with self.subTest(identity=identity, race=race, route=route):
                d = decide(identity, race, eligibility, route, stage, requested)
                self.assertEqual(d.disposition, Disposition.DEFER_TO_EXISTING)
                self.assertIsNone(d.may_activate)
                self.assertFalse(d.permitted_stage_write)

    def test_vanilla_race_random_bots_remain_stock(self):
        for race in (1, 2, 3, 4, 5, 6, 7, 8, 9):
            for route in Route:
                d = decide(Identity.RANDOM_BOT, race, TbcEligibility.UNAVAILABLE,
                           route, 0, requested_stage=0)
                self.assertEqual(d.disposition, Disposition.DEFER_TO_EXISTING)
                self.assertIsNone(d.may_activate)

    def test_unavailable_or_unverified_tbc_preserves_every_saved_stage(self):
        for race, eligibility, stage, route in itertools.product(
            (10, 11), (TbcEligibility.UNAVAILABLE, TbcEligibility.UNKNOWN),
            (0, 7, 8, 12, 13, 18, None), tuple(Route)
        ):
            d = decide(Identity.RANDOM_BOT, race, eligibility, route, stage, 8)
            self.assertEqual(d.disposition, Disposition.KEEP_DORMANT)
            self.assertFalse(d.may_activate)
            self.assertFalse(d.permitted_stage_write)
            self.assertIsNone(d.proposed_stage)

    def test_saved_invalid_stages_never_get_silent_migration(self):
        for race, stage, route in itertools.product(
            (10, 11), (0, 1, 7, -1, 19, 255), tuple(Route)
        ):
            for requested in (None, 0, 8, 13):
                d = decide(Identity.RANDOM_BOT, race, TbcEligibility.AVAILABLE,
                           route, stage, requested)
                self.assertEqual(d.disposition, Disposition.HOLD_MIGRATION)
                self.assertFalse(d.may_activate)
                self.assertFalse(d.permitted_stage_write)

    def test_missing_stage_is_not_assumed_new_in_restored_paths(self):
        for route in Route:
            if route == Route.NEW_SELECTION:
                continue
            d = decide(Identity.RANDOM_BOT, 10, TbcEligibility.AVAILABLE,
                       route, None, 8)
            self.assertEqual(d.disposition, Disposition.HOLD_MIGRATION)
            self.assertFalse(d.may_activate)

    def test_only_first_verified_eligible_new_selection_can_initialize(self):
        for race in (10, 11):
            d = decide(Identity.RANDOM_BOT, race, TbcEligibility.AVAILABLE,
                       Route.NEW_SELECTION, None, 8)
            self.assertEqual(d.disposition, Disposition.INITIALIZE_NEW)
            self.assertTrue(d.may_activate)
            self.assertEqual(d.proposed_stage, 8)
            self.assertTrue(d.permitted_stage_write)

        for stage in (None, 0, 1, 7, 9, 19):
            d = decide(Identity.RANDOM_BOT, 11, TbcEligibility.AVAILABLE,
                       Route.NEW_SELECTION, None, stage)
            self.assertEqual(d.disposition, Disposition.HOLD_MIGRATION)
            self.assertFalse(d.permitted_stage_write)

    def test_existing_stage8_plus_relog_never_rewrites_markers(self):
        for race, stage, route in itertools.product(
            (10, 11), range(8, 19),
            (Route.RELOGIN, Route.SAVED_EVENT_RESTORE, Route.MANUAL_LOGIN)
        ):
            d = decide(Identity.RANDOM_BOT, race, TbcEligibility.AVAILABLE,
                       route, stage)
            self.assertEqual(d.disposition, Disposition.PRESERVE)
            self.assertTrue(d.may_activate)
            self.assertFalse(d.permitted_stage_write)
            self.assertIsNone(d.proposed_stage)

    def test_all_routes_reject_vanilla_or_out_of_range_stage_writes(self):
        for race, current, requested, route in itertools.product(
            (10, 11), (8, 9, 12, 13, 18),
            (-2, -1, 0, 1, 7, 19, 255), tuple(Route)
        ):
            d = decide(Identity.RANDOM_BOT, race, TbcEligibility.AVAILABLE,
                       route, current, requested)
            self.assertEqual(d.disposition, Disposition.REJECT_WRITE)
            self.assertTrue(d.may_activate)
            self.assertFalse(d.permitted_stage_write)
            self.assertIsNone(d.proposed_stage)

    def test_same_stage_is_an_idempotent_no_write(self):
        for race, stage, route in itertools.product((10, 11), range(8, 19), tuple(Route)):
            d = decide(Identity.RANDOM_BOT, race, TbcEligibility.AVAILABLE,
                       route, stage, stage)
            self.assertEqual(d.disposition, Disposition.PRESERVE)
            self.assertFalse(d.permitted_stage_write)

    def test_valid_tbc_wotlk_advances_or_downshifts_within_floor(self):
        for race, old, new, route in itertools.product(
            (10, 11), range(8, 19), range(8, 19), tuple(Route)
        ):
            if old == new:
                continue
            d = decide(Identity.RANDOM_BOT, race, TbcEligibility.AVAILABLE,
                       route, old, new)
            self.assertEqual(d.disposition, Disposition.ALLOW_VALID_WRITE)
            self.assertTrue(d.permitted_stage_write)
            self.assertEqual(d.proposed_stage, new)

    def test_level_and_default_ip_limit_not_inputs_to_eligibility(self):
        # Explicit TBC availability is mandatory. Character level, core
        # CONFIG_EXPANSION, IP .conf.dist, and ProgressionLimit=0 are NOT
        # alternate implicit eligibility proofs.
        d = decide(Identity.RANDOM_BOT, 10, TbcEligibility.UNKNOWN,
                   Route.RELOGIN, 8, 0)
        self.assertEqual(d.disposition, Disposition.KEEP_DORMANT)
        self.assertFalse(d.permitted_stage_write)

    def test_caller_data_never_mutated(self):
        request = (Identity.RANDOM_BOT, 11, TbcEligibility.AVAILABLE,
                   Route.GROUP_SYNC, 12, 7)
        expected = tuple(request)
        decide(*request)
        self.assertEqual(request, expected)

    def test_positive_identity_required_not_race_or_bot_account_guess(self):
        for identity in (Identity.HUMAN, Identity.ALT_BOT, Identity.SELF_BOT,
                         Identity.UNKNOWN):
            self.assertEqual(
                decide(identity, 10, TbcEligibility.UNAVAILABLE,
                       Route.SAVED_EVENT_RESTORE, 0).disposition,
                Disposition.DEFER_TO_EXISTING,
            )


if __name__ == "__main__":
    unittest.main()
