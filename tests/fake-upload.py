#!/usr/bin/env python3
"""Stands in for Litterbox in tests: accepts a multipart upload and answers
with a download link on files.test.

usage: fake-upload.py <port>
Part of harbour_icq_nina; GPL-3.0-or-later.
"""
import http.server
import re
import secrets
import sys


class Handler(http.server.BaseHTTPRequestHandler):
    def do_POST(self):
        length = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(length)
        ok = b'name="reqtype"' in body and b"fileupload" in body and b'name="fileToUpload"' in body
        hours = re.search(rb'name="time"\r\n\r\n(\d+)h', body)
        if not ok or not hours:
            self.send_response(400)
            self.end_headers()
            self.wfile.write(b"bad request")
            return
        reply = f"https://files.test/{secrets.token_hex(3)}.bin".encode()
        self.send_response(200)
        self.send_header("Content-Type", "text/plain")
        self.send_header("Content-Length", str(len(reply)))
        self.end_headers()
        self.wfile.write(reply)
        print(f"upload of {length} bytes, {hours.group(1).decode()} h -> {reply.decode()}", flush=True)

    def log_message(self, *args):
        pass


if __name__ == "__main__":
    http.server.ThreadingHTTPServer(("127.0.0.1", int(sys.argv[1])), Handler).serve_forever()
