#!/bin/bash
# Paid purchase injected-persistence-failure harness (THROWAWAY DB ONLY).
# Usage: run_harness.sh <core_src_root> <core_build_dir> <mysql_socket> <compile_cmd_prefix_file>
#   core_src_root : AzerothCore checkout (for data/sql/base/db_characters)
#   core_build_dir: a configured+built Core tree (libdatabase.a, libcommon.a, deps)
#   mysql_socket  : a disposable MySQL 8 server (root, no password) - NEVER an owner DB
#   compile prefix: file holding the compiler command + include flags of that build
set -euo pipefail
CORE=$1; BUILD=$2; SOCK=$3; PREFIX=$4
HERE=$(cd "$(dirname "$0")" && pwd); IP=$(cd "$HERE/../.." && pwd)
DB=ip_paid_harness; OUT=${OUT:-$(mktemp -d)}
M="mysql -uroot --socket=$SOCK"
$M -e "DROP DATABASE IF EXISTS $DB; CREATE DATABASE $DB DEFAULT CHARSET utf8mb4 COLLATE utf8mb4_unicode_ci;
       CREATE USER IF NOT EXISTS 'harness'@'127.0.0.1' IDENTIFIED BY 'harness';
       GRANT ALL ON $DB.* TO 'harness'@'127.0.0.1'; FLUSH PRIVILEGES;"
for f in "$CORE"/data/sql/base/db_characters/*.sql; do $M $DB < "$f"; done
$M $DB < "$HERE/harness_setup.sql"
PORT=$($M -N -e "SELECT @@port")
eval "$(cat "$PREFIX") -I$IP/src $HERE/paid_persistence_db_harness.cpp -o $OUT/dbh \
  $BUILD/src/server/database/libdatabase.a $BUILD/src/common/libcommon.a $BUILD/deps/fmt/libfmt.a \
  $BUILD/deps/argon2/libargon2.a $BUILD/deps/SFMT/libsfmt.a $BUILD/deps/g3dlite/libg3dlib.a \
  -lmysqlclient -lssl -lcrypto -lboost_system -lboost_filesystem -lboost_program_options \
  -lboost_iostreams -lboost_thread -lz -lpthread -ldl"
timeout 120 "$OUT/dbh" "127.0.0.1;$PORT;harness;harness;$DB"
# The release audit over the DB the harness left behind (read-only exports).
for q in audit anomalies schema; do
  $M -B --skip-column-names $DB < "$IP/tools/ip_paid_receipt_$q.sql" > "$OUT/$q.tsv"
done
python3 "$IP/tools/ip_paid_receipt_audit.py" "$OUT/audit.tsv" --anomalies "$OUT/anomalies.tsv" \
  --schema "$OUT/schema.tsv" --release || echo "release audit exit=$?"
echo "outputs in $OUT"
