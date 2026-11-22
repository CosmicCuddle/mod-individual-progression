# F14 leader eligibility and invalid-view recovery

Owner: CosmicCuddle. Child of IP1929e9b97b516c516069411db3f160cee2f96792139.

## Observed update-boundary defect

The prior owned-leader update returned early for some ineligible session roles,
without invalidating a previously published group stage. It also requested a
refresh only when a valid read differed in stage. An initially ineligible leader
consumed pending work without publishing; later eligible updates could see the
invalid entry indefinitely without requesting a new publication.

Refresh and owned-leader update now use the same existing eligible-human-leader
predicate. At the owned update callback, ineligible current facts request view
invalidation; an invalid/missing published read requests world-thread refresh.
Restored eligibility can republish without requiring an unrelated quest/group
change. Only own-player/current-group facts are read in that callback; no global
player lookup or foreign leader read is added. World refresh still resolves and
checks current leader/group identity before publication. No snapshots/earned
quest model, group admission, Paid or title rules are rewritten.

## Evidence and limits

Existing actual-source adapter expanded for headless/closed socket, GM/non-normal
account, out-of-world and initially invalid leader recovery. Tests retain earned17
and zero writes while the view falls back to8/re-publishes15. Existing join/leave,
leader change, relog/rebind, mutation fence and title/login tests remain included.
Exact parent adapter reproduces an executable stale-role failure. Engine objects,
classification, ScriptMgr ABI and group ownership remain explicit test doubles;
paired mode uses actual Core70 mutation/dispatch/getter bodies. Exact compilers,
counts and hosted results belong to draft PR/review queue.

This corrects an OBSERVED OWNED-UPDATE boundary, not instantaneous invalidation for
every external GM/security/headless edit. A role change before the callback can
still precede invalidation; a stronger atomic guarantee needs corresponding Core
lifecycle seams. Do not call this complete native concurrent authorization proof.
No hard-real-time, actual group/container locking or owner gameplay claim.

Core session hook dependency and rewarded-quest fence remain; unearned Wrath
borrowing retains Paid-OFF requirements. F14/Paid/visibility defaults stay OFF;
no DB/config/SQL/DBC/client/live change. Native build, ownership/classifier and
owner acceptance remain outstanding. Rollback source child/rebuild matching module;
no earned markers, persisted group/guild state or data migration were changed.
