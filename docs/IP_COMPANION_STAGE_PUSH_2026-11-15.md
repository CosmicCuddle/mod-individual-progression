# Companion raw-stage notification — isolated Stage A

Owner: CosmicCuddle. Coordination issue #29, owner-approved bounded Stage A only. Base: owner-approved Phase B master 05ff032b625f6e8abb6b2b5be496bb8a3ee65c96. Core API reference 21ff1973bc75216f8a0d0f3d7731eb3e8894ff97.

## Changed behavior
After the existing automatic or forced progression writer completes, compare actual raw marker stage before/after. Only a changed valid raw stage produces one final self-targeted CHAT_MSG_WHISPER/LANG_ADDON with the legacy string IPPSTATE\t1\t<rawStage>. Require in-world player, matching interactive WorldSession, not headless and not logging out. No ACK, retry, DB lookup, command, entitlement inference or gameplay dependency. Do not send inside marker loops. Existing cap, paid refusal, stage floor and attunement logic unchanged. Out-of-band Core marker writers not routed through these functions are outside this slice; shared snapshot/quest hook work remains separate.

## Evidence
Payload header blob 92ccdcdbd0185934ebc872e2c91368268cc3ccae and test blob 2720de6894096b07b561ef9a3dfa793549b7345f: GCC 14.2 and Clang 17 compiled and executed all 65,536 uint8 before/after pairs, including zero/reset, multi-stage jumps, identical stage and out-of-range. Command:
```sh
g++ -std=c++20 -Wall -Wextra -Werror -Isrc tests/test_companion_push_payload.cpp -o /tmp/ip29-push && /tmp/ip29-push
clang++ -std=c++20 -Wall -Wextra -Werror -Isrc tests/test_companion_push_payload.cpp -o /tmp/ip29-push-clang && /tmp/ip29-push-clang
```
Source preservation check: removing the three includes, helper, before-stage capture and two final notification calls restores the exact original IndividualProgression.cpp text. This is not a native module compile or packet-delivery test.

## Integration / rollback
Current separate IP composition 43c355800668a81cb7517cc742bea32503438336 is a descendant of the baseline and adds pre-marker cache invalidation in the same functions. Compose carefully; retain those invalidations and paid durability/other custom behavior. No edit to Claude's branch. Verify exact native IP+Core build and 3.3.5 client event shape with approved disposable character; never use real character .ip set as a test. Companion PR9 child is separate; N-Questie B blocked until native Companion acceptance. No source merge, package, configuration, live server or DB change. Rollback discard/revert isolated draft; no data migration.
