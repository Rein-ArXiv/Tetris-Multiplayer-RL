"""Loopback-only lesson server with an allowlisted, read-only live source API."""
from __future__ import annotations

import argparse
import hashlib
import json
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlsplit

ROOT = Path(__file__).resolve().parents[1]
DOCS = ROOT / "docs"
MANIFEST = DOCS / "learn/library-manifest.json"


class LessonHandler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(DOCS), **kwargs)

    def end_headers(self):
        # Lessons are edited locally: cached JS must not hide a revised lesson/UI.
        self.send_header("Cache-Control", "no-cache")
        self.send_header("X-Content-Type-Options", "nosniff")
        super().end_headers()

    def do_GET(self):
        parsed = urlsplit(self.path)
        if parsed.path != "/__learn/source":
            return super().do_GET()
        # No CORS: the source viewer is for this local lesson page only.
        port = self.server.server_port
        allowed_hosts = {f"127.0.0.1:{port}", f"localhost:{port}"}
        host = self.headers.get("Host", "")
        origin = self.headers.get("Origin")
        if host not in allowed_hosts or (origin and origin != f"http://{host}"):
            return self.send_error(403)
        query = parse_qs(parsed.query)
        paths = query.get("path", [])
        if len(paths) != 1:
            return self.send_error(400)
        requested = paths[0]
        try:
            allowed = json.loads(MANIFEST.read_text(encoding="utf-8"))["sources"]
            path = ROOT / requested
            if requested not in allowed or not path.resolve().is_relative_to(ROOT) or path.is_symlink() or not path.is_file():
                return self.send_error(404)
            raw = path.read_bytes()
            payload = json.dumps({
                "path": requested, "text": raw.decode("utf-8"),
                "sha256": hashlib.sha256(raw).hexdigest(),
                "modified": path.stat().st_mtime, "kind": "working-tree",
            }, ensure_ascii=False).encode("utf-8")
        except (OSError, ValueError, KeyError, UnicodeError):
            return self.send_error(404)
        self.send_response(200)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(payload)))
        self.end_headers()
        self.wfile.write(payload)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=18765)
    args = parser.parse_args()
    with ThreadingHTTPServer(("127.0.0.1", args.port), LessonHandler) as server:
        print(f"Learning: http://127.0.0.1:{args.port}/learn/", flush=True)
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            pass


if __name__ == "__main__":
    main()
