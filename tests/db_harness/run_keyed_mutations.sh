#!/bin/bash
# Mutation check for the keyed save-ordering prototype (rev2). THROWAWAY DB ONLY.
# Usage: run_keyed_mutations.sh <core_src_root> <core_build_dir> <mysql_socket> <compile_cmd_prefix_file>
# Run run_keyed_harness.sh first (it creates the harness DB and sets innodb_lock_wait_timeout=1).
# Optional: NAMES="M2_... M3_..." to run a subset. Prints KILLED/SURVIVED per mutant; exit 1 if any survived.
set -uo pipefail
CORE=$1; BUILD=$2; SOCK=$3; PREFIX=$4
HERE=$(cd "$(dirname "$0")" && pwd); IP=$(cd "$HERE/../.." && pwd); OUT=${OUT:-$(mktemp -d)}
PORT=$(mysql -uroot --socket="$SOCK" -N -e "SELECT @@port"); CONN="127.0.0.1;$PORT;harness;harness;ip_paid_harness"
export HARNESS_MYSQL_SOCKET=$SOCK PYTHONDONTWRITEBYTECODE=1
survived=0
for name in ${NAMES:-$(cd "$HERE" && python3 -c "import keyed_mutants; print(' '.join(keyed_mutants.M))")}; do
  D=$OUT/$name; rm -rf "$D"; mkdir -p "$D"; cp "$HERE/keyed_ordering_harness.cpp" "$HERE/keyed_dispatcher_prototype.h" "$D/"
  (cd "$HERE" && python3 - "$D/keyed_dispatcher_prototype.h" "$name" <<'PY'
import sys, keyed_mutants
f, n = sys.argv[1], sys.argv[2]; s = open(f).read()
for old, new in keyed_mutants.M[n]:
    if old not in s: sys.exit("mutant %s: pattern not found: %r" % (n, old))
    s = s.replace(old, new)
open(f, "w").write(s)
PY
  ) || { echo "$name PATTERN-ERROR"; survived=1; continue; }
  eval "$(cat "$PREFIX") -I$IP/src -I$D $D/keyed_ordering_harness.cpp -o $D/keyed \
    $BUILD/src/server/database/libdatabase.a $BUILD/src/common/libcommon.a $BUILD/deps/fmt/libfmt.a \
    $BUILD/deps/argon2/libargon2.a $BUILD/deps/SFMT/libsfmt.a $BUILD/deps/g3dlite/libg3dlib.a \
    -lmysqlclient -lssl -lcrypto -lboost_system -lboost_filesystem -lboost_program_options \
    -lboost_iostreams -lboost_thread -lz -lpthread -ldl" > "$D/build.log" 2>&1 || { echo "$name BUILD-FAIL"; survived=1; continue; }
  timeout 400 "$D/keyed" "$CONN" keyed 4 > "$D/run.log" 2>&1; rc=$?
  if [ $rc -eq 2 ] || grep -q "cannot open/prepare" "$D/run.log"; then echo "$name ERROR (harness could not run, rc=$rc)"; survived=1
  elif [ $rc -ne 0 ]; then echo "$name KILLED rc=$rc first: $(grep -m1 '^FAIL' "$D/run.log" | sed 's/.*harness.cpp://')"
  else echo "$name SURVIVED"; survived=1; fi
  sleep 5 # let any external lock holder finish
done
exit $survived
