# Runtime save/logout test procedure (disposable test realm only)

These are the runtime checks for the PB59 character-save evidence code. They could not run in the review container, because there's no WoW 3.3.5a client data (DBC/maps).

**Run them only on a disposable test realm** with:
- copies of the auth/world/characters/playerbots databases made from empty base SQL;
- **never** the live server, its config, databases or backups.

## Build tuple
Use the same tuple that was built in review:
- Core `10b0ec9a` (contains `28110cd`);
- PB `3d0f5529`;
- IP `4a604378` (IP #187, paid features OFF);
- CS `f43bc2c9`.

Standard AzerothCore build, `MODULES=static`, `SCRIPTS=static`.

## Setup
1. Extract client data with the tools from the same build (`map_extractor`, `vmap4_extractor`/`assembler`, `mmaps_generator`). Point `DataDir` at it.
2. Create fresh databases. Let worldserver auto-update them (`Updates.EnableDatabases`).
3. Create a GM test account and one character. Set Playerbots to a small RandomBot count (e.g. 20).
4. Set `CharacterDatabase.WorkerThreads = 1`, the same as the owner's server.
5. For the fault cases, put `err_proxy.py` (this folder) between worldserver and MySQL for the **characters** connection only:
   ```
   CharacterDatabaseInfo = "127.0.0.1;33318;<user>;<pass>;<db>;nossl"
   ```
   Use a `mysql_native_password` test user, because the proxy has no TLS.

## Cases (run each with `PB59.CharacterSaveEvidence.Enable` = 0, then = 1)
| # | Steps | Expected with 0 | Expected with 1 |
|---|---|---|---|
| R1 parity | Log in, loot an item, finish a quest, `.save`, log out, restart, log in | all persisted | the same; no `PB59` error lines |
| R2 IP marker | `.quest add 66001` / `.quest complete 66001` / reward, then `.save` | row in `character_queststatus_rewarded` | the same; nothing logged on success |
| R3 COMMIT error | With the proxy: `DO 'harness-err-commit'` is needed inside the transaction, so add a temporary debug command, or use the proxy in a mode that fails the **next** COMMIT (add `--fail-next-commit`). Then `.save` | rows missing; nothing logged (legacy reports success) | within 5 s, `PB59 character save uncertain/failed ... quest 66001 ...` for that GUID and sequence. No replay. The next save writes current state. |
| R4 ordered bot saves | Invite a bot, log the master out (`LogoutAllBots`) | both bot saves committed in order | the same, plus two sequences per bot; no PB59 errors |
| R5 saturation | 300+ bots online, shut down | — | `diagnostic reservation/capture unavailable` lines for the overflow, native saves still committed |
| R6 shutdown drain | `.server shutdown 1` during bot activity | clean shutdown | final drain logs any uncertain saves; nothing hangs |
| R7 strategy save | Change a bot strategy (`co +dps`), log out; repeat with a COMMIT error | strategy row persisted / not | the same, plus `PB59 strategy save GUID ...` on failure; the character logout save is still committed |

## Pass criteria
- Every case with the flag set to 0 behaves exactly like the unmodified tuple.
- With the flag set to 1: no replays, no blocked logouts, and every failed or uncertain save leaves a log line naming GUID, sequence and IP marker IDs.
- No sanitizer/crash output (a Debug or ASan build is recommended).
