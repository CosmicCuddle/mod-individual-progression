# F14 deferred guild-removal eligibility recheck

Owner: CosmicCuddle. Child of PR190 aecba064eb2e9d8d7043d454b887787407816433.
Owner guild policy remains #54 comment6947910783: confirmed still-Vanilla human
login/join may remediate that guild's positively restricted bots; a downgrade
itself does not trigger immediate removal. No new trigger/sweep is introduced.

The queued work previously retained guild/human GUID values but rechecked only
human membership. If the human advanced to TBC or disconnected before world drain,
the old removal path could still remove restricted bots. A prior DeleteMember
callback could also change a later target's identity/progression/option state;
old helper checked membership but did not re-prove these current facts.

Immediately before each DeleteMember, require option/world-thread context,
distinct human/target, current connected-human policy and guild agreement, and
current restricted target policy/guild agreement. Offline targets retain existing
last-logout-confirmed policy only. Live facts override remembered identity. The
original member/survivor check and native DeleteMember flags remain unchanged.

Executable tests compile the actual ScanGuild/RemoveRestrictedIfSafe/RemediateGuild
bodies with the real policy header. Guild/Player/classifier/world/cache are explicit
doubles, including reentrant deletion callbacks. The exact parent reproduces a
now-TBC human causing removal; current bodies protect that case and identity,
thread/option, disconnect, stale guild, offline unknown and reentrant changes.
This is not actual GuildScript ABI registration, native bot classifier, persisted
guild history or gameplay proof. Core guild API v2 remains a dependency; defaults
stay OFF. No off-thread admission/LANG_ADDON/visual-concealment behavior changes.

Exact compiler/counts/CI results belong to the draft PR and review queue. Native
Core/IP build and owner-approved gameplay tests remain required. No SQL/config/
DBC/client/live guild action. Rollback: revert child, rebuild compatible modules,
keep pilot disabled; no persisted guild changes or migration were performed.
