#!/bin/bash
# Runs the CS01 Core child test round against a THROWAWAY MySQL 8 only.
S=${SCRATCH:?set SCRATCH to the review scratch dir}; cd $S
SOCK=$S/my8/run/m.sock; export HARNESS_MYSQL_SOCKET=$SOCK
if ! mysql -uroot --socket=$SOCK -e "SELECT 1" >/dev/null 2>&1; then
  rm -f $SOCK*; mysqld --no-defaults --user=root --datadir=$S/my8/data --socket=$SOCK --port=33306 --bind-address=127.0.0.1 --mysqlx=OFF --secure-file-priv="" --log-error=$S/my8/err.log >/dev/null 2>&1 &
  for i in $(seq 30); do mysql -uroot --socket=$SOCK -e "SELECT 1" >/dev/null 2>&1 && break; sleep 1; done
fi
M="mysql -uroot --socket=$SOCK"
reset_pool() { $M -e "DROP DATABASE IF EXISTS tx_pool; CREATE DATABASE tx_pool DEFAULT CHARSET utf8mb4 COLLATE utf8mb4_unicode_ci; GRANT ALL ON tx_pool.* TO 'harness_np'@'127.0.0.1';"
  for f in $S/txdraft/src_fa/data/sql/base/db_characters/*.sql; do $M tx_pool < "$f"; done
  $M tx_pool -e "CREATE TABLE txt (k VARCHAR(32) NOT NULL PRIMARY KEY, v INT) ENGINE=InnoDB; CREATE TABLE txc (id INT PRIMARY KEY, v INT UNSIGNED) ENGINE=InnoDB; INSERT INTO txc VALUES (1,0); CREATE TABLE txo (seq INT AUTO_INCREMENT PRIMARY KEY, k VARCHAR(32)) ENGINE=InnoDB;"; }
P=$(sed "s|$S/coresync|$S/txdraft/src_fa|g" dbh_prefix.txt); B=buildsync
L="$B/src/common/libcommon.a $B/deps/fmt/libfmt.a $B/deps/argon2/libargon2.a $B/deps/SFMT/libsfmt.a $B/deps/g3dlite/libg3dlib.a -lmysqlclient -lssl -lcrypto -lboost_system -lboost_filesystem -lboost_program_options -lboost_iostreams -lboost_thread -lz -lpthread -ldl"
if [ "${BUILD:-1}" = 1 ]; then
  eval "$P -fsanitize=address,undefined -g txdraft/t/tx_pool_test.cpp -o txdraft/t/pool_fa_asan txdraft/libdatabase_fa_asan.a $L" || exit 3
fi
grep -q txdbhost /etc/hosts || echo "127.0.0.1 txdbhost" >> /etc/hosts
cd txdraft/t; export ASAN_OPTIONS=detect_leaks=0
python3 err_proxy.py 33318 33306 > proxy_round2.log 2>&1 & PX=$!; sleep 1
PI="127.0.0.1;33318;harness_np;harness;tx_pool;nossl"; DI="127.0.0.1;33306;harness_np;harness;tx_pool;nossl"
# 1. Prior real-DB matrix on the latest child (sync connection level, ASan/UBSan).
$M -e "DELETE FROM tx_boundary.txt; UPDATE tx_boundary.txc SET v=0"
timeout 200 ./tx_fa_asan "127.0.0.1;33306;harness_np;harness;tx_boundary;nossl" "127.0.0.1;33318;harness_np;harness;tx_boundary;nossl" draft > r2_matrix.log 2>&1; echo "matrix exit=$?"
for m in direct async notready drain cancel; do reset_pool; timeout 200 ./pool_fa_asan "$PI" $m > r2_$m.log 2>&1; echo "$m exit=$?"; done
for o in refuse dns; do reset_pool; HI="$PI"; [ $o = dns ] && HI="txdbhost;33318;harness_np;harness;tx_pool;nossl"; timeout 200 ./pool_fa_asan "$HI" outage $o > r2_outage_$o.log 2>&1; echo "outage-$o exit=$?"; done
reset_pool; s=$(date +%s); timeout 200 ./pool_fa_asan "$PI" outage dead > r2_outage_dead.log 2>&1; rc=$?; echo "outage-dead exit=$rc secs=$(( $(date +%s)-s ))"
kill $PX 2>/dev/null; wait $PX 2>/dev/null
grep -q txdbhost /etc/hosts || echo "127.0.0.1 txdbhost" >> /etc/hosts
sed -i '/txdbhost/d' /etc/hosts
