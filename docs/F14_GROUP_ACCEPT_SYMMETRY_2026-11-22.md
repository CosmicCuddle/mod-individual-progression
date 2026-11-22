# F14 group-accept symmetry and social hook integration

Owner: CosmicCuddle. Isolated child of IP1916f029a4ef8b2dcc54adfdfccf4a0a3430017cf60.

The leader check was bidirectional, but the non-leader check returned immediately
unless the acceptor was a still-Vanilla human. A positively verified restricted bot
could therefore accept a TBC-led group containing a non-leader Vanilla human.
The actual parent hook reproduces this bypass in the executable adapter.

Use existing AllowPairIn for each safely resolved current member when the acceptor
is a Vanilla human OR restricted bot. Retain leader handling, skip unresolved/
foreign members, never infer identity from slot/race alone. Human-to-bot attempts
retain existing reply behavior; bot-initiated blocked acceptance stays silent.
Nonrestricted acceptors retain the earlier fast allow. No new social/gameplay rule.

The adapter compiles actual ReadHuman/ReadBot, owned-map records/ReadScope/current
scope, MayReadPair/AllowPairIn and actual whisper/trade/invite/group-accept/who
method bodies with the real policy header. Player/Map/Group/session/classifier and
packet sink are explicit doubles; registration/override is not native-ABI proof.
Foreign Player getters abort in tests, so accidental foreign state reads fail.
The target identity registry and raw-stage adapter remain doubles; this does not
prove production classification, group ownership/locking, session lifetime, rate
limiting, packet delivery, native Core build or gameplay. Group container access
still inherits the existing caller ownership contract; no PB dispatch change.

Tests cover parent failure, both pair directions, non-leader cases, unknown/GM/
ordinary race/real-client protections, stale ticks, owned vs foreign maps, local
name resolution without global access and existing addon behavior. The latter is
preserved, not approved or expanded by this patch. Visual concealment stays paused.

Exact compiler/counts/CI outcomes are in draft PR/review queue. Pilot defaults OFF;
no config/SQL/DBC/client or live operation. Core/IP native integration and owner
acceptance remain open. Rollback: revert child/rebuild matching module; no data
migration or persisted group/guild changes were made.
