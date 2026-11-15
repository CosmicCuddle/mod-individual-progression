# Mutants of keyed_dispatcher_prototype.h (revision 2) for run_keyed_mutations.sh:
# name -> [(old, new)]. Every old text must occur (all occurrences are replaced);
# each mutant must make the keyed harness fail.
UNCERTAIN = """                    key.state = KeyState::Uncertain;
                    ++_stats.reconciliations;"""
DELIVER = """            k.state = KeyState::Delivering;
            std::vector<Waiter> waiters = TakeWaiters(k.current);
            for (auto& w : waiters)
                if (w.done)
                    Invoke(k, [&] { w.done(w.purchase ? forPurchase : forSaves); });
            k.running = false;
            k.current = Op();
            k.state = KeyState::Idle;
            k.stallReported = false;
            Pump(guid, k);"""
DELIVER_LATE = """            std::vector<Waiter> waiters = TakeWaiters(k.current);
            k.running = false;
            k.current = Op();
            k.state = KeyState::Idle;
            k.stallReported = false;
            Pump(guid, k);
            for (auto& w : waiters)
                if (w.done)
                    Invoke(k, [&] { w.done(w.purchase ? forPurchase : forSaves); });"""
M = {
 # ambiguity / proof (H1)
 "M2_advance_on_ambiguous": [(UNCERTAIN, "                    DeliverAndAdvance(guid, key, Outcome::NotCommitted, Outcome::NotCommitted);\n                    return;")],
 "M3_reconcile_without_claim": [("                ClaimUpdate(guid, k.current.fence, true);\n                return;",
                                 "                ClaimVerify(guid, k.current.fence, std::string(\"none\"), true);\n                return;"),
                                ("if (owner != token && fence == expected)", "if (!reconcile && owner != token && fence == expected)")],
 "M4_higher_marker_is_proof": [("""bool const logMine = !f[3].IsNull() && f[3].Get<uint64_t>() == op.fence
                        && f[4].Get<std::string>() == op.opToken;""", "bool const logMine = seq >= op.seq;")],
 "M14_assume_committed_on_false": [(UNCERTAIN, "                    key.lastSeq = seq;\n                    DeliverAndAdvance(guid, key, Outcome::Committed, Outcome::Committed);\n                    return;")],
 "M22_write_without_fence": [('"UPDATE harness_write_seq SET seq = {}, fence = {} WHERE guid = {}",\n                op.seq, op.fence, guid)',
                              '"UPDATE harness_write_seq SET seq = {} WHERE guid = {}",\n                op.seq, guid)')],
 "M23_fenced_admission_ignored": [("if (write && it->second.fenced)\n                return Admit::Fenced;", "")],
 # duplicate purchase / id ownership
 "M7_no_duplicate_detection": [("forPurchase = Outcome::DuplicatePurchase;", "forPurchase = Outcome::NotCommitted;")],
 "M7b_other_character_id_attributed": [("if (f[5].Get<uint32_t>() == guid)", "if (true)")],
 # lease / session
 "M6_no_epoch_check": [("if (it->second.leaseEpoch != epoch)", "if (false)")],
 "M16_closed_lease_ignored": [("if (!it->second.leaseOpen)\n                return Admit::NoSession;", "")],
 # callbacks before next dispatch (H2)
 "M17_deliver_after_advance": [(DELIVER, DELIVER_LATE)],
 # caps / coalescing (H3)
 "M11_no_per_key_cap": [("k.queue.size() >= _limits.maxPendingPerKey ||", "false ||")],
 "M12_no_global_cap": [("|| TotalPending() >= _limits.maxTotalPending)", "|| false)"),
                       ("_shuttingDown || TotalPending() >= _limits.maxTotalPending ||", "_shuttingDown ||")],
 "M13_no_coalescing": [("if (tail.kind == OpKind::Write && !tail.dispatched && tail.purchaseId.empty() && tail.epoch == epoch)", "if (false)")],
 "M18_no_per_op_waiter_cap": [("if (tail.waiters.size() >= _limits.maxWaitersPerOp)", "if (false)")],
 "M19_no_global_waiter_cap": [("            if (_totalWaiters >= _limits.maxTotalWaiters)\n                return Refuse();\n", "")],
 "M20_cross_session_coalescing": [(" && tail.epoch == epoch)", ")")],
 "M21_purchase_coalescing": [("if (!purchase && !k.queue.empty())", "if (!k.queue.empty())"),
                             (" && tail.purchaseId.empty() && ", " && ")],
 # freeze on inconsistent proof / callback failure (CS01 6895011029)
 "M24_no_freeze_on_inconsistent_proof": [("                        if (!journalMine)\n                            FreezeForProof(key, op, FreezeKind::CurrentProof);", "                        if (false)\n                            FreezeForProof(key, op, FreezeKind::CurrentProof);")],
 "M25_journal_fence_ignored": [("\n                            && f[8].Get<uint64_t>() == op.fence);", ");")],
 "M26_frozen_ignored_by_pump": [(" && !k.abandoned && !k.frozen)", " && !k.abandoned)")],
 "M27_frozen_admission_ignored": [("            if (it->second.frozen)\n                return Admit::Frozen;\n", "")],
 "M28_callback_exception_propagates": [("                ++_stats.callbackExceptions;\n                Freeze(k);", "                ++_stats.callbackExceptions;\n                throw;")],
 # proof-gated unfreeze, shutdown containment, duplicate proof (CS01 6895440140)
 "M29_blind_unfreeze": [("            if (k.freezeKind == FreezeKind::Callback)\n            {", "            if (true)\n            {")],
 "M30_shutdown_callback_uncontained": [("                            Invoke(k, [&] { w.done(Outcome::Unresolved); });", "                            w.done(Outcome::Unresolved);")],
 "M31_duplicate_without_proof": [("                            if (earlierProven)", "                            if (true)")],
 # resolve lifecycle + duplicate recovery (CS01 6896262413)
 "M32_resolve_ignores_freeze_generation": [("ok = ok && key.frozen && key.freezeGen == gen && key.freezeKind == kind;", "ok = ok && key.frozen;")],
 "M33_resolve_not_single_flight": [(" || it->second.resolveToken || _shuttingDown)", " || _shuttingDown)")],
 "M34_resolve_completion_uncontained": [("            if (done)\n                Invoke(k, [&] { done(ok); });", "            if (done)\n                done(ok);")],
 "M35_shutdown_leaves_resolve_pending": [("                    Complete(k, cb, false); // settled exactly once", "                    (void)cb;")],
 "M36_duplicate_resolved_by_current_proof": [("FreezeForProof(key, op, FreezeKind::EarlierDuplicate);", "FreezeForProof(key, op, FreezeKind::CurrentProof);")],
 # lifecycle
 "M8_no_shutdown_drain": [("while (!Idle() && Clock::now() < deadline)", "while (false)")],
 "M9_running_cancel_ignored": [("k.current.discard = true;", "k.current.discard = false;")],
 "M10_build_failure_no_advance": [("                k.deferredOutcome = Outcome::BuildFailed;\n", "")],
 "M15_no_stall_report": [("k.stallReported = true;\n", "\n")],
}
