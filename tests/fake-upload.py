#!/usr/bin/env python3
"""Stands in for Litterbox in tests: accepts a multipart upload, answers with
a download link on files.test (keeping the file's extension, as Litterbox
does), and serves what was uploaded at /<name> over plain HTTP (the app's
test build maps files.test links to this server).

usage: fake-upload.py <port>
Part of harbour_icq_nina; GPL-3.0-or-later.
"""
import http.server
import re
import secrets
import sys
import threading

FILES = {}
LOCK = threading.Lock()


class Handler(http.server.BaseHTTPRequestHandler):
    def do_POST(self):
        length = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(length)
        ok = b'name="reqtype"' in body and b"fileupload" in body and b'name="fileToUpload"' in body
        hours = re.search(rb'name="time"\r\n\r\n(\d+)h', body)
        boundary = re.search(r'boundary="?([^";]+)"?', self.headers.get("Content-Type", ""))
        part = re.search(rb'name="fileToUpload"; filename="([^"]*)"\r\n(?:[^\r\n]+\r\n)*\r\n', body)
        if not ok or not hours or not boundary or not part:
            self.send_response(400)
            self.end_headers()
            self.wfile.write(b"bad request")
            return
        end = body.find(b"\r\n--" + boundary.group(1).encode(), part.end())
        data = body[part.end():end]
        ext = re.search(rb"\.([A-Za-z0-9]{1,5})$", part.group(1))
        name = secrets.token_hex(3) + ("." + ext.group(1).decode().lower() if ext else "")
        with LOCK:
            FILES[name] = data
        reply = f"https://files.test/{name}".encode()
        self.send_response(200)
        self.send_header("Content-Type", "text/plain")
        self.send_header("Content-Length", str(len(reply)))
        self.end_headers()
        self.wfile.write(reply)
        print(f"upload of {len(data)} bytes, {hours.group(1).decode()} h -> {reply.decode()}", flush=True)

    def do_GET(self):
        with LOCK:
            data = FILES.get(self.path.lstrip("/"))
        if data is None:
            self.send_response(404)
            self.end_headers()
            print(f"download {self.path}: not found", flush=True)
            return
        self.send_response(200)
        self.send_header("Content-Type", "application/octet-stream")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)
        print(f"download {self.path}: {len(data)} bytes", flush=True)

    def log_message(self, *args):
        pass


if __name__ == "__main__":
    http.server.ThreadingHTTPServer(("127.0.0.1", int(sys.argv[1])), Handler).serve_forever()
