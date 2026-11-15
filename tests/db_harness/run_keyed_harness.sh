#!/bin/bash
# Keyed save-ordering PROTOTYPE harness (design C rev2). THROWAWAY DB ONLY.
# Usage: run_keyed_harness.sh <core_src_root> <core_build_dir> <mysql_socket> <compile_cmd_prefix_file>
#   (same arguments as run_harness.sh; the server must be a disposable MySQL 8, root without
#    password, NEVER an owner/live/backup DB: this script sets innodb_lock_wait_timeout=1 on it)
set -euo pipefail
CORE=$1; BUILD=$2; SOCK=$3; PREFIX=$4
HERE=$(cd "$(dirname "$0")" && pwd); IP=$(cd "$HERE/../.." && pwd)
DB=ip_paid_harness; OUT=${OUT:-$(mktemp -d)}
M="mysql -uroot --socket=$SOCK"
$M -e "DROP DATABASE IF EXISTS $DB; CREATE DATABASE $DB DEFAULT CHARSET utf8mb4 COLLATE utf8mb4_unicode_ci;
       CREATE USER IF NOT EXISTS 'harness'@'127.0.0.1' IDENTIFIED BY 'harness';
       GRANT ALL ON $DB.* TO 'harness'@'127.0.0.1'; FLUSH PRIVILEGES;
       SET GLOBAL innodb_lock_wait_timeout = 1;"
for f in "$CORE"/data/sql/base/db_characters/*.sql; do $M $DB < "$f"; done
$M $DB < "$HERE/harness_setup.sql"
PORT=$($M -N -e "SELECT @@port")
eval "$(cat "$PREFIX") -Wall -Wformat=2 -I$IP/src -I$HERE $HERE/keyed_ordering_harness.cpp -o $OUT/keyed \
  $BUILD/src/server/database/libdatabase.a $BUILD/src/common/libcommon.a $BUILD/deps/fmt/libfmt.a \
  $BUILD/deps/argon2/libargon2.a $BUILD/deps/SFMT/libsfmt.a $BUILD/deps/g3dlite/libg3dlib.a \
  -lmysqlclient -lssl -lcrypto -lboost_system -lboost_filesystem -lboost_program_options \
  -lboost_iostreams -lboost_thread -lz -lpthread -ldl"
CONN="127.0.0.1;$PORT;harness;harness;$DB"
export HARNESS_MYSQL_SOCKET=$SOCK
rc_all=0
for w in 1 2 4 8; do
  rc=0; timeout 300 "$OUT/keyed" "$CONN" keyed $w || rc=$?
  echo "keyed workers=$w exit=$rc"; [ $rc -eq 0 ] || rc_all=1
done
# K11: a real process restart between the two halves.
rc=0; timeout 120 "$OUT/keyed" "$CONN" restart1 || rc=$?; echo "restart1 exit=$rc"; [ $rc -eq 0 ] || rc_all=1
rc=0; timeout 120 "$OUT/keyed" "$CONN" restart2 || rc=$?; echo "restart2 exit=$rc"; [ $rc -eq 0 ] || rc_all=1
echo "outputs in $OUT"
exit $rc_all
