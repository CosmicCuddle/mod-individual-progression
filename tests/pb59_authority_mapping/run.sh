#!/bin/bash
# IP v1 snapshot -> CS01 PB59 CombinedAuthorityPrototype mapping fixture.
# The CS01 header is a pinned copy of Core branch
# feature/cs01-pb59-equipment-invalidation-audit-20261115, tests/pb59/CombinedAuthorityPrototype.h,
# git blob e4780c36643fd341e60b9ac7be6a60bda3b8d769 (reviewed at Core 4938dbf6). Refuse to run on drift.
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
PIN=e4780c36643fd341e60b9ac7be6a60bda3b8d769
GOT=$(git hash-object "$HERE/CombinedAuthorityPrototype.h")
[ "$GOT" = "$PIN" ] || { echo "pinned CS01 header drifted: $GOT != $PIN"; exit 2; }
OUT=${OUT:-$(mktemp -d)}
for cxx in g++ clang++; do
  command -v $cxx >/dev/null || continue
  $cxx -std=c++20 -Wall -Wextra -Werror -I"$HERE" "$HERE/ip_authority_mapping_test.cpp" -o "$OUT/map_$cxx"
  "$OUT/map_$cxx"
done
