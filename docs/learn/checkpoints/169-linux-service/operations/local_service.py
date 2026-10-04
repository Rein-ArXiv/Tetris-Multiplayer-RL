"""Educational lifecycle helper for a trusted local C++ service.

Expected service contract:
  * argv: [port, absolute_database_path]
  * binds only to numeric 127.0.0.1, using port 0 for an ephemeral port
  * initialises the SQLite database/schema before binding
  * flushes exactly "PORT <actual port>\n" after bind, then listens

The readiness probe uses only GET /healthz; it does not exercise mutation
endpoints or downstream services. The launched process itself may initialise
its database. Operational requests belong inside the caller's context.  Every process started here
is owned by this context and is always torn down on exit; unrelated
listeners are never signalled.
"""

from __future__ import annotations

import argparse
import http.client
import json
import math
import re
import subprocess
import tempfile
import time
from contextlib import contextmanager
from pathlib import Path

_LOOPBACK = "127.0.0.1"
_PORT_LINE_LIMIT = 256   # hard cap on the child's port report
_BODY_LIMIT = 1025       # tiny JSON body; never read an unbounded stream
_HTTP_TIMEOUT_MAX = 0.2
_TERM_GRACE = 2.0


def _read_port_line(log_path: Path, proc: subprocess.Popen, deadline: float) -> bytes:
    """Return one newline-terminated line, bounded, checking liveness/deadline."""
    data = b""
    while True:
        with open(log_path, "rb") as reader:
            reader.seek(len(data))
            data += reader.read(_PORT_LINE_LIMIT - len(data))
        if b"\n" in data:
            return data.split(b"\n", 1)[0] + b"\n"
        if len(data) >= _PORT_LINE_LIMIT:
            raise RuntimeError("service port report was oversized")
        if proc.poll() is not None:
            raise RuntimeError("service exited before reporting a port")
        if time.monotonic() >= deadline:
            raise RuntimeError("service deadline exceeded while awaiting port")
        time.sleep(0.02)  # regular files do not block, so poll politely


def _wait_healthy(proc: subprocess.Popen, port: int, deadline: float) -> None:
    """Probe the trusted local service; check an admission deadline between I/O.

    Socket timeouts bound individual blocking operations, not an entire slow
    peer response. This helper is not a general-purpose remote health monitor.
    """
    while True:
        if proc.poll() is not None:
            raise RuntimeError("service process exited before becoming healthy")
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise RuntimeError("service deadline exceeded")

        # Explicit numeric host/port: http.client never consults environment
        # proxies, so loopback traffic stays local.
        conn = http.client.HTTPConnection(
            _LOOPBACK, port, timeout=min(_HTTP_TIMEOUT_MAX, remaining)
        )
        try:
            conn.request("GET", "/healthz")
            resp = conn.getresponse()
            status = resp.status
            body = resp.read(_BODY_LIMIT)
        except (OSError, http.client.HTTPException):
            # Listen socket may not be accepting yet; retry within deadline.
            time.sleep(min(0.02, max(0.0, deadline - time.monotonic())))
            continue
        finally:
            conn.close()

        if status == 503:
            time.sleep(min(0.02, max(0.0, deadline - time.monotonic())))
            continue
        if status != 200:
            raise RuntimeError("health check returned a non-200 status")
        if len(body) >= _BODY_LIMIT:
            raise RuntimeError("health check body exceeded the size limit")
        try:
            payload = json.loads(body)
        except (json.JSONDecodeError, UnicodeDecodeError):
            raise RuntimeError("health check body was not valid JSON")
        if not isinstance(payload, dict) or payload.get("ok") is not True:
            raise RuntimeError("health check body was not an object with ok=true")

        # Recheck liveness and deadline immediately before yielding.
        if proc.poll() is not None:
            raise RuntimeError("service process exited during health check")
        if time.monotonic() >= deadline:
            raise RuntimeError("service deadline exceeded")
        return


@contextmanager
def local_service(executable: Path, database: Path, timeout: float = 5.0):
    executable = Path(executable).resolve()
    database = Path(database).resolve()

    if not (isinstance(timeout, (int, float)) and math.isfinite(timeout) and timeout > 0):
        raise ValueError("timeout must be a positive, finite number")
    if not executable.is_file():
        raise FileNotFoundError(f"service executable not found: {executable}")
    if not database.parent.is_dir():          # DB path itself stays preserved
        raise FileNotFoundError(f"database parent does not exist: {database.parent}")

    deadline = time.monotonic() + timeout
    proc = None

    with tempfile.TemporaryDirectory(prefix="local-service-") as tmp:
        log_path = Path(tmp) / "child.log"    # stdout+stderr; never a PIPE
        try:
            with open(log_path, "wb") as sink:
                proc = subprocess.Popen(
                    [str(executable), "0", str(database)],
                    cwd=tmp,                  # private, owned working directory
                    stdout=sink,
                    stderr=subprocess.STDOUT,
                )

            line = _read_port_line(log_path, proc, deadline)
            match = re.fullmatch(rb"PORT ([1-9][0-9]{0,4})\n", line)
            if match is None:
                raise RuntimeError("service reported an invalid port line")
            port = int(match[1])
            if not 1 <= port <= 65535:
                raise RuntimeError("service port out of range")

            _wait_healthy(proc, port, deadline)
            yield f"http://{_LOOPBACK}:{port}"
        finally:
            # Signal only our own Popen; other listeners are untouched.
            if proc is not None and proc.poll() is None:
                proc.terminate()
                try:
                    proc.wait(timeout=_TERM_GRACE)
                except subprocess.TimeoutExpired:
                    proc.kill()
                    proc.wait(timeout=_TERM_GRACE)


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description="Run and probe a local service.")
    parser.add_argument("binary", type=Path)
    parser.add_argument("database", type=Path)
    args = parser.parse_args(argv)

    with local_service(args.binary, args.database) as base_url:
        print(base_url)   # local_service already performed one /healthz probe
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
