#!/usr/bin/env python3
"""COMMIT-drop TCP proxy for the keyed save-ordering harness (THROWAWAY DB ONLY).

Usage: commit_drop_proxy.py <listen_port> <mysql_port>

Forwards MySQL protocol traffic unchanged (the client must connect WITHOUT TLS)
except for transactions tagged by the harness with a raw statement:

  DO 'harness-drop-after-commit'   the COMMIT is forwarded, the server's reply
                                   is read and dropped, then both sockets close:
                                   the server HAS committed, the client sees a
                                   lost connection during COMMIT;
  DO 'harness-drop-before-commit'  both sockets close instead of forwarding the
                                   COMMIT: the server rolls the transaction back,
                                   the client sees a lost connection during COMMIT.

Core's MySQLConnection then reconnects (through this proxy again) and reports
"may or may not have been written" as a failed transaction: a real network
loss at COMMIT, not a test hook. One line per action is printed to stdout.
"""
import asyncio
import sys

AFTER = b"harness-drop-after-commit"
BEFORE = b"harness-drop-before-commit"


def log(msg):
    print(msg, flush=True)


async def handle(client_reader, client_writer, mysql_port):
    server_reader, server_writer = await asyncio.open_connection("127.0.0.1", mysql_port)
    state = {"tag": None, "drop_reply": False}

    def close_all():
        for w in (client_writer, server_writer):
            try:
                w.close()
            except Exception:
                pass

    async def server_to_client():
        try:
            while True:
                data = await server_reader.read(65536)
                if not data:
                    break
                if state["drop_reply"]:
                    log("proxy: COMMIT reply received from server and DROPPED; closing")
                    break
                client_writer.write(data)
                await client_writer.drain()
        except Exception:
            pass
        finally:
            close_all()

    async def client_to_server():
        buf = b""
        try:
            while True:
                data = await client_reader.read(65536)
                if not data:
                    break
                buf += data
                out = b""
                while len(buf) >= 4:
                    length = int.from_bytes(buf[:3], "little")
                    if len(buf) < 4 + length:
                        break
                    packet, buf = buf[:4 + length], buf[4 + length:]
                    payload = packet[4:]
                    if payload[:1] == b"\x03":  # COM_QUERY
                        # With CLIENT_QUERY_ATTRIBUTES (MySQL 8 client) the text follows
                        # parameter_count = 0 and parameter_set_count = 1.
                        query = (payload[3:] if payload[1:3] == b"\x00\x01" else payload[1:]).strip()
                        if AFTER in query:
                            state["tag"] = "after"
                        elif BEFORE in query:
                            state["tag"] = "before"
                        elif query.upper() == b"ROLLBACK":
                            state["tag"] = None
                        elif query.upper() == b"COMMIT" and state["tag"]:
                            tag, state["tag"] = state["tag"], None
                            if tag == "before":
                                if out:
                                    server_writer.write(out)
                                    await server_writer.drain()
                                log("proxy: COMMIT NOT forwarded; closing (server rolls back)")
                                close_all()
                                return
                            state["drop_reply"] = True
                            server_writer.write(out + packet)
                            await server_writer.drain()
                            log("proxy: COMMIT forwarded; its reply will be dropped")
                            out = b""
                            continue
                    out += packet
                if out:
                    server_writer.write(out)
                    await server_writer.drain()
        except Exception:
            pass
        finally:
            if not state["drop_reply"]:
                close_all()

    await asyncio.gather(server_to_client(), client_to_server())


async def main():
    listen_port, mysql_port = int(sys.argv[1]), int(sys.argv[2])
    server = await asyncio.start_server(lambda r, w: handle(r, w, mysql_port), "127.0.0.1", listen_port)
    log(f"proxy: listening on {listen_port} -> {mysql_port}")
    async with server:
        await server.serve_forever()


if __name__ == "__main__":
    asyncio.run(main())
