#!/usr/bin/env python3
"""A minimal AOL rendezvous proxy (ARS) for tests.

OSCAR file transfers fall back to a proxy when neither side can reach the
other. Open OSCAR Server has no proxy, so the tests run this one.

Protocol (each packet): u16 length of the rest, u16 version 0x044A,
u16 command, u32 0, u16 flags, data.
  2 init send  (sender):   BUIN screen name, 8-byte cookie, TLV 1 capability
  3 ack        (to sender): u16 port check, u32 proxy IPv4
  4 init recv  (receiver): BUIN screen name, u16 port check, cookie, TLV 1
  5 ready      (to both); after that the proxy pipes bytes between them
  1 error      u16 code (0x0D bad request)

usage: fake-ars.py <listen-ip> <port>
Part of harbour_icq_nina; GPL-3.0-or-later.
"""
import asyncio
import random
import socket
import struct
import sys

PENDING = {}  # cookie -> (port check, reader, writer, ready future)


def packet(command, data=b""):
    body = struct.pack(">HHIH", 0x044A, command, 0, 0) + data
    return struct.pack(">H", len(body)) + body


async def read_packet(reader):
    (length,) = struct.unpack(">H", await reader.readexactly(2))
    body = await reader.readexactly(length)
    _version, command, _unknown, _flags = struct.unpack(">HHIH", body[:10])
    return command, body[10:]


async def pipe(reader, writer):
    try:
        while True:
            data = await reader.read(65536)
            if not data:
                break
            writer.write(data)
            await writer.drain()
    except (ConnectionError, asyncio.CancelledError):
        pass
    finally:
        writer.close()


async def handle(reader, writer, listen_ip):
    peer = writer.get_extra_info("peername")
    try:
        command, data = await read_packet(reader)
        name_len = data[0]
        name = data[1:1 + name_len].decode("latin-1")
        rest = data[1 + name_len:]
        if command == 2:
            cookie = rest[:8]
            check = random.randint(1024, 65000)
            ready = asyncio.get_running_loop().create_future()
            PENDING[cookie] = (check, reader, writer, ready)
            writer.write(packet(3, struct.pack(">H", check) + socket.inet_aton(listen_ip)))
            await writer.drain()
            print(f"ARS send from {name} {peer}: port check {check}", flush=True)
            await ready  # the receiver's handler pipes both ways
        elif command == 4:
            (check,) = struct.unpack(">H", rest[:2])
            cookie = rest[2:10]
            entry = PENDING.pop(cookie, None)
            if not entry or entry[0] != check:
                print(f"ARS recv from {name}: no matching send", flush=True)
                writer.write(packet(1, struct.pack(">H", 0x0D)))
                await writer.drain()
                writer.close()
                return
            _, s_reader, s_writer, ready = entry
            print(f"ARS recv from {name} {peer}: connected", flush=True)
            for w in (s_writer, writer):
                w.write(packet(5))
                await w.drain()
            await asyncio.gather(pipe(s_reader, writer), pipe(reader, s_writer))
            ready.set_result(True)
        else:
            writer.write(packet(1, struct.pack(">H", 0x0D)))
            writer.close()
    except (asyncio.IncompleteReadError, ConnectionError) as e:
        print(f"ARS connection {peer} ended: {e!r}", flush=True)
        writer.close()


async def main(listen_ip, port):
    server = await asyncio.start_server(lambda r, w: handle(r, w, listen_ip), listen_ip, port)
    print(f"ARS listening on {listen_ip}:{port}", flush=True)
    async with server:
        await server.serve_forever()


if __name__ == "__main__":
    asyncio.run(main(sys.argv[1], int(sys.argv[2])))
