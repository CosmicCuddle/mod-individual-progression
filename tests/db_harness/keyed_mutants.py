# Mutants of keyed_dispatcher_prototype.h for run_keyed_mutations.sh: name -> [(old, new)].
# Every old text must occur; each mutant must make the keyed harness fail.
M = {
 "M2_advance_on_ambiguous": [("""key.state = KeyState::Uncertain;
                    key.uncertainSince = Clock::now();
                    key.reconcileNotBefore = key.uncertainSince;
                    StartReconcile(guid, key);""", "Resolve(guid, key, false, false);")],
 "M3_reconcile_plain_read": [('WHERE guid = {} FOR SHARE", _marker, guid)\n                : ', 'WHERE guid = {}", _marker, guid)\n                : '),
                             ('FOR SHARE OF w"', '"')],
 "M4_init_plain_read": [('"SELECT seq FROM {} WHERE guid = {} FOR SHARE"', '"SELECT seq FROM {} WHERE guid = {}"')],
 "M6_no_epoch_check": [("if (it->second.leaseEpoch != epoch)", "if (false)")],
 "M7_no_duplicate_detection": [("Resolve(guid, key, committed, !committed && journalHasId);", "Resolve(guid, key, committed, false);")],
 "M8_no_shutdown_drain": [("while (!Idle() && Clock::now() < deadline)", "while (false)")],
 "M9_running_cancel_ignored": [("k.current.discard = true;", "k.current.discard = false;")],
 "M10_build_failure_no_advance": [("""                Finish(guid, k);
                return;
            }
            k.state = KeyState::Running;""", """                k.state = KeyState::Running;
                return;
            }
            k.state = KeyState::Running;""")],
 "M11_no_per_key_cap": [("k.queue.size() >= _limits.maxPendingPerKey ||", "false ||")],
 "M12_no_global_cap": [("|| TotalPending() >= _limits.maxTotalPending)", "|| false)")],
 "M13_no_coalescing": [("if (!k.queue.empty() && k.queue.back().kind == OpKind::Write && !k.queue.back().dispatched)", "if (false)")],
 "M14_assume_committed_on_false": [("""key.state = KeyState::Uncertain;
                    key.uncertainSince = Clock::now();
                    key.reconcileNotBefore = key.uncertainSince;
                    StartReconcile(guid, key);""", "key.lastSeq = seq; Resolve(guid, key, true, false);")],
 "M15_no_stall_report": [("k.stallReported = true;\n", "\n")],
 "M16_closed_lease_ignored": [("if (!it->second.leaseOpen)\n                return Admit::NoSession;", "")],
}
