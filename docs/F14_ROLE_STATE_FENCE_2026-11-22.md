# F14 native role-state fence integration

Owner: CosmicCuddle. Child of IP1933fffa397fd6d678cfb515313b12a2ac54a0b0ac5.
Paired Core71bc2460ea16de7108493d81ef1e5ac54e21bae213 (child of Core70c6e3bf04).

## Native change boundary

Optional role APIv1 WorldScript notifications are pointer-free and synchronous.
IP subscribes both halves even while the pilot is OFF, counts nesting with the
existing mutation fence, and clears all value-only group snapshots before a
supported native role setter writes. No session->Player lookup or retained actor
pointer is needed. End preserves other active quest/role pairs. Query during the
pair cannot consume a cached era; clear/reload cannot erase active pairs/poison.
No role permissions, native setter side effects or gameplay policy is changed by
this consumer. Rare global invalidation trades some refresh work for safe identity.

Refresh and owned-leader callback check mutation gating and read eligible role/
stage facts under the same view lock. The account-pattern matcher can load an Auth
DB cache miss, so it is evaluated OUTSIDE that lock between gate checks. Publication
still uses the original work revision: a role/config invalidation between account
classification and publication cannot resurrect earlier work. Stage readers used
inside the critical section must not invoke writers; ownership/classifier/reader
contracts still need native composition and concurrency QA.

Session security uses explicit AccountMgr::IsPlayerAccount, matching existing
social human eligibility. IP isNormalAccount is a separate account-pattern
classification, not a session-security check. Existing cache/unknown-account
semantics are not silently rewritten here. Unearned Wrath borrowing requires role
APIv1 plus paired quest and session capabilities, Paid OFF and normal current
leader facts; older Core retains conservative TBC compatibility fallback. Earned
markers, solo state, group admission, titles and other races remain unchanged.

## Executable evidence and limits

Core tests compile actual guard/full GM setter/security setter/dispatch:26 checks,
source contracts4. IP adapter compiles actual Core security setter/guard/dispatch,
actual IP callbacks/queries and real snapshot model with explicit engine/registry/
classifier/group/session doubles. Exact IP193 reproduces native role-fence gap.
The expanded session/quest/role capability matrix covers reads/publication forced
inside before notification, nested role pairs, config reload, option changes,
security elevation/restoration and previous quest/group/relog/earned tests.
Exact compiler/counts/results are in draft PRs/review queue.

This is NOT native Worldserver/ABI, actual account classifier, ownership, group
locking, sanitizers or threaded gameplay proof. Core hydration/direct writes,
untracked fields/setters and unsupported callbacks remain outside API coverage.
Headless construction role/session rebind/disconnect retain existing lifecycle
requirements. The global mutex is a cache fence, not general Player authority or
field synchronization for unrelated readers. Full rebuild of matching Core/modules
and owner acceptance remain required; no runtime options are enabled here.

## Safety register delta and rollback

Feature: optional native role fence for temporary group era views. Dependencies:
Core71/Core70 and IP193 lineage; WorldScript ABI changes require matching rebuild.
No config/SQL/DBC/client/data migration, live role or server change. Defaults OFF.
Rollback paired source children/rebuild compatible tuple, preserve earned quests
and guild/group history. Older SDK must retain the capability fallback rather
than claiming new atomic coverage. No merge/deployment approval implied.
