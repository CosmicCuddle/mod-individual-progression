#!/usr/bin/env python3
"""ERR-injecting MySQL proxy (THROWAWAY DB ONLY). Usage: err_proxy.py <listen_port> <mysql_port> [--fail-next-commit [N]]
Tags (sent as DO '<tag>'):
  harness-err-commit    next COMMIT is not forwarded; client gets ERR 1180; connection kept
  harness-err-begin     next START TRANSACTION is not forwarded; client gets ERR 1180
  harness-err-rollback  next ROLLBACK is not forwarded; client gets ERR 1180; connection kept
  harness-plan-a4       1st time: next UPDATE gets fake ERR 1213 (not forwarded);
                        2nd time: next COMMIT forwarded, reply dropped, both sockets closed
"""
import asyncio, sys
def log(m): print(m, flush=True)
G = {"server": None, "lp": 0, "mp": 0, "fail_next_commit": 0}
HOSTS = "/etc/hosts"; HOSTLINE = "127.0.0.1 txdbhost\n"
def hosts_set(on):
    lines = [l for l in open(HOSTS).read().splitlines(True) if "txdbhost" not in l]
    if on: lines.append(HOSTLINE)
    open(HOSTS, "w").write("".join(lines))
async def refuse(seconds, dns=False):
    if dns:
        hosts_set(False); log(f"proxy: DNS entry removed for {seconds}s")
        await asyncio.sleep(seconds); hosts_set(True); log("proxy: DNS entry restored"); return
    G["server"].close(); log(f"proxy: listener closed for {seconds}s")
    if seconds < 0: return
    await asyncio.sleep(seconds)
    G["server"] = await asyncio.start_server(lambda r, w: handle(r, w, G["mp"]), "127.0.0.1", G["lp"])
    log("proxy: listener reopened")
def err_packet(seq, code, msg):
    p = b"\xff" + code.to_bytes(2, "little") + b"#HY000" + msg.encode()
    return len(p).to_bytes(3, "little") + bytes([seq]) + p
async def handle(cr, cw, mport):
    sr, sw = await asyncio.open_connection("127.0.0.1", mport)
    st = {"tags": set(), "a4": 0, "a4used": 0, "drop": False}
    def close_all():
        for w in (cw, sw):
            try: w.close()
            except Exception: pass
    async def s2c():
        try:
            while True:
                d = await sr.read(65536)
                if not d: break
                if st["drop"]:
                    log("proxy: COMMIT reply dropped; closing"); break
                cw.write(d); await cw.drain()
        except Exception: pass
        finally: close_all()
    async def c2s():
        buf = b""
        try:
            while True:
                d = await cr.read(65536)
                if not d: break
                buf += d
                while len(buf) >= 4:
                    n = int.from_bytes(buf[:3], "little")
                    if len(buf) < 4 + n: break
                    pkt, buf = buf[:4+n], buf[4+n:]
                    pl = pkt[4:]; seq = pkt[3]
                    if pl[:1] == b"\x03":
                        q = (pl[3:] if pl[1:3] == b"\x00\x01" else pl[1:]).strip()
                        u = q.upper()
                        for t in (b"harness-err-commit", b"harness-err-begin", b"harness-err-rollback", b"harness-refuse-after", b"harness-dns-after", b"harness-dead-after"):
                            if t in q: st["tags"].add(t)
                        if b"harness-plan-a4" in q: st["a4"] += 1
                        fake = None
                        if u == b"COMMIT" and G["fail_next_commit"] > 0:
                            G["fail_next_commit"] -= 1; fake = (1180, "injected COMMIT error (--fail-next-commit)")
                        elif u == b"COMMIT" and st["tags"] & {b"harness-refuse-after", b"harness-dns-after", b"harness-dead-after"}:
                            t = st["tags"]; dns = b"harness-dns-after" in t
                            secs = -1 if b"harness-dead-after" in t else 7
                            t.difference_update({b"harness-refuse-after", b"harness-dns-after", b"harness-dead-after"})
                            fake = (1180, "injected COMMIT error before outage")
                            asyncio.get_running_loop().create_task(refuse(secs, dns))
                        elif u == b"COMMIT" and b"harness-err-commit" in st["tags"]:
                            st["tags"].discard(b"harness-err-commit"); fake = (1180, "injected COMMIT error")
                        elif u.startswith(b"START TRANSACTION") and b"harness-err-begin" in st["tags"]:
                            st["tags"].discard(b"harness-err-begin"); fake = (1180, "injected BEGIN error")
                        elif u == b"ROLLBACK" and b"harness-err-rollback" in st["tags"]:
                            st["tags"].discard(b"harness-err-rollback"); fake = (1180, "injected ROLLBACK error")
                        elif u.startswith(b"UPDATE") and st["a4"] == 1 and st["a4used"] == 0:
                            st["a4used"] = 1; fake = (1213, "injected deadlock")
                        elif u == b"COMMIT" and st["a4"] == 2 and st["a4used"] == 1:
                            st["a4used"] = 2; st["drop"] = True
                            sw.write(pkt); await sw.drain()
                            log("proxy: a4 COMMIT forwarded, reply will be dropped"); continue
                        if fake:
                            log(f"proxy: fake ERR {fake[0]} for {u[:30]!r}")
                            cw.write(err_packet(seq + 1, *fake)); await cw.drain(); continue
                    sw.write(pkt); await sw.drain()
        except Exception: pass
        finally:
            if not st["drop"]: close_all()
    await asyncio.gather(s2c(), c2s())
async def main():
    lp, mp = int(sys.argv[1]), int(sys.argv[2])
    # Optional: --fail-next-commit [N] fails the next N COMMITs on any connection.
    if "--fail-next-commit" in sys.argv:
        i = sys.argv.index("--fail-next-commit")
        G["fail_next_commit"] = int(sys.argv[i + 1]) if len(sys.argv) > i + 1 and sys.argv[i + 1].isdigit() else 1
    G["lp"], G["mp"] = lp, mp
    G["server"] = await asyncio.start_server(lambda r, w: handle(r, w, mp), "127.0.0.1", lp)
    log(f"proxy: listening {lp} -> {mp}")
    while True: await asyncio.sleep(3600)
asyncio.run(main())
