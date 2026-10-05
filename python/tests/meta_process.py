from __future__ import annotations

import contextlib
import http.client
import json
import math
import os
import re
import subprocess
import tempfile
import time
from pathlib import Path
from typing import Iterator, Mapping, Optional, Sequence, Tuple

_PORT_LINE = re.compile(rb"PORT ([1-9][0-9]{0,4})\r?\n")
_PORT_READ_LIMIT = 256
_DIAG_LIMIT = 4096
_HEALTH_BODY_LIMIT = 1025
_IO_TIMEOUT = 0.2


class MetaServerStartupError(RuntimeError):
    """The local test meta server did not become ready in time."""


@contextlib.contextmanager
def local_meta_server(
    binary: Path,
    database: Path,
    secret: Optional[str] = None,
    timeout: float = 30,
    *,
    cwd: Optional[Path] = None,
    extra_args: Sequence[str] = (),
    env: Optional[Mapping[str, str]] = None,
) -> Iterator[str]:
    """
    Run the repository's LOCAL test meta server for the duration of the block.

    Yields the base URL (http://127.0.0.1:<port>). Intended for tests only, not
    production. The parent never reserves a free port; the child binds port 0
    and announces the chosen port, avoiding the reserve/close race.
    The single startup deadline covers process creation, schema initialization
    and HTTP readiness. SQLite alone may wait five seconds for a lock; loaded
    CI storage also needs time for schema writes. Fast starts return immediately.
    ``cwd``, ``env`` and ``extra_args`` are keyword-only; extra args must not
    override the helper-owned port, DB, or secret options.
    """
    binary = Path(binary).resolve()
    database = Path(database).resolve()
    if not math.isfinite(timeout) or timeout <= 0:
        raise ValueError("timeout must be positive and finite")
    deadline = time.monotonic() + timeout

    with tempfile.TemporaryDirectory(prefix="meta-server-") as tmp:
        tmp_path = Path(tmp)
        out_path = tmp_path / "stdout.bin"
        err_path = tmp_path / "stderr.bin"

        argv = [
            os.fspath(binary),
            *extra_args,
            "--http", "127.0.0.1:0",
            "--db", os.fspath(database),
        ]
        if secret is not None:
            argv.extend(("--relay-secret", secret))

        with out_path.open("wb") as out_f, err_path.open("wb") as err_f:
            proc = subprocess.Popen(
                argv,
                cwd=os.fspath(cwd) if cwd is not None else None,
                env=dict(env) if env is not None else None,
                stdout=out_f,
                stderr=err_f,
            )

        try:
            port, reason = _await_ready(proc, out_path, deadline)
            if port is None:
                _shutdown(proc)
                effective_secret = secret
                if effective_secret is None:
                    effective_env = env if env is not None else os.environ
                    effective_secret = effective_env.get("TETRIS_RELAY_SECRET", "")
                diagnostic = _diagnostic(proc, err_path, effective_secret)
                raise MetaServerStartupError(
                    f"local meta server not ready: {reason}\n{diagnostic}"
                )
            yield f"http://127.0.0.1:{port}"
        finally:
            _shutdown(proc)


def _await_ready(
    proc: subprocess.Popen,
    out_path: Path,
    deadline: float,
) -> Tuple[Optional[int], Optional[str]]:
    with open(out_path, "rb") as f:
        buf = bytearray()
        while True:
            chunk = f.read(_PORT_READ_LIMIT - len(buf))
            if chunk:
                buf += chunk
                if b"\n" in buf:
                    break
                if len(buf) >= _PORT_READ_LIMIT:
                    return None, "port announcement exceeded 256 bytes without newline"
                continue
            code = proc.poll()
            if code is not None:
                return None, f"process exited with code {code} before announcing a port"
            if time.monotonic() >= deadline:
                return None, "timed out waiting for port announcement"
            time.sleep(0.01)

    newline = buf.index(b"\n")
    line = bytes(buf[: newline + 1])
    match = _PORT_LINE.fullmatch(line)
    if match is None:
        return None, "malformed port announcement"
    port = int(match.group(1))
    if not 1 <= port <= 65535:
        return None, f"port out of range: {port}"

    while True:
        code = proc.poll()
        if code is not None:
            return None, f"process exited with code {code} during readiness probe"
        if time.monotonic() >= deadline:
            return None, "timed out waiting for /healthz readiness"
        try:
            status, body = _probe(port)
        except (OSError, http.client.HTTPException):
            pass
        else:
            if status == 200 and _is_ok(body):
                if proc.poll() is not None or time.monotonic() >= deadline:
                    return None, "process ended or deadline expired during probe"
                return port, None
        time.sleep(0.02)


def _probe(port: int) -> Tuple[int, bytes]:
    # Fixed numeric loopback address; http.client does not consult proxy env.
    conn = http.client.HTTPConnection("127.0.0.1", port, timeout=_IO_TIMEOUT)
    try:
        conn.request("GET", "/healthz")
        response = conn.getresponse()
        return response.status, response.read(_HEALTH_BODY_LIMIT)
    finally:
        conn.close()


def _is_ok(body: bytes) -> bool:
    if len(body) >= _HEALTH_BODY_LIMIT:
        return False
    try:
        data = json.loads(body.decode("utf-8"))
    except (UnicodeDecodeError, ValueError):
        return False
    return isinstance(data, dict) and data.get("ok") is True


def _diagnostic(proc: subprocess.Popen, err_path: Path, secret: str) -> str:
    code = proc.poll()
    try:
        with err_path.open("rb") as stream:
            # Read enough lookahead to redact a secret crossing the output cap.
            raw = stream.read(_DIAG_LIMIT + len(secret.encode("utf-8")))
    except OSError:
        raw = b""
    if secret:
        raw = raw.replace(secret.encode("utf-8"), b"<redacted>")
    text = raw[:_DIAG_LIMIT].decode("utf-8", "replace")
    return f"exit code: {code}\nfirst stderr bytes:\n{text}"


def _shutdown(proc: subprocess.Popen) -> None:
    # An unsuccessful cleanup is a test failure, never silently accepted.
    if proc.poll() is not None:
        return
    try:
        proc.terminate()
    except ProcessLookupError:
        pass
    try:
        proc.wait(timeout=3)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait(timeout=3)
