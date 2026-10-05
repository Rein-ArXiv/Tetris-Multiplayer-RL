"""Exercise the real readiness loop with a virtual clock, without long sleeps."""
import inspect
from types import SimpleNamespace

import pytest

from . import meta_process


class Clock:
    def __init__(self):
        self.now = 0.0

    def monotonic(self):
        return self.now

    def sleep(self, duration):
        self.now += duration


class Child:
    def __init__(self, clock, output, publish_at=None):
        self.clock, self.output, self.publish_at = clock, output, publish_at
        self.published = False

    def poll(self):
        if not self.published and self.publish_at is not None and self.clock.now >= self.publish_at:
            self.output.write_bytes(b"PORT 12345\n")
            self.published = True
        return None


def setup_loop(monkeypatch, tmp_path, publish_at, healthy_at):
    clock = Clock()
    output = tmp_path / "stdout.bin"
    output.touch()
    child = Child(clock, output, publish_at)
    monkeypatch.setattr(meta_process, "time", clock)

    def probe(port):
        assert port == 12345
        return (200, b'{"ok":true}') if clock.now >= healthy_at else (503, b'{}')

    monkeypatch.setattr(meta_process, "_probe", probe)
    return clock, child, output


def test_default_budget_allows_schema_startup_beyond_five_seconds(monkeypatch, tmp_path):
    clock, child, output = setup_loop(monkeypatch, tmp_path, 6.0, 6.0)
    budget = inspect.signature(meta_process.local_meta_server).parameters["timeout"].default
    assert meta_process._await_ready(child, output, budget) == (12345, None)
    assert 6 <= clock.now < budget


def test_missing_port_still_reaches_deadline(monkeypatch, tmp_path):
    clock, child, output = setup_loop(monkeypatch, tmp_path, None, 0)
    port, reason = meta_process._await_ready(child, output, 2.0)
    assert port is None and "timed out waiting for port" in reason
    assert 2 <= clock.now < 2.02


def test_http_readiness_uses_remaining_budget(monkeypatch, tmp_path):
    clock, child, output = setup_loop(monkeypatch, tmp_path, 1.5, 3.0)
    port, reason = meta_process._await_ready(child, output, 2.0)
    assert port is None and "timed out waiting for /healthz" in reason
    assert 2 <= clock.now < 2.03


def test_child_failure_does_not_wait_out_budget(monkeypatch, tmp_path):
    clock, _, output = setup_loop(monkeypatch, tmp_path, None, 0)
    port, reason = meta_process._await_ready(SimpleNamespace(poll=lambda: 7), output, 30)
    assert port is None and "exited with code 7" in reason
    assert clock.now == 0


def test_startup_failure_has_bounded_redacted_diagnostic_and_cleanup(monkeypatch, tmp_path):
    clock = Clock()
    monkeypatch.setattr(meta_process, "time", clock)
    state = {}

    class Process:
        def __init__(self, argv, **kwargs):
            state["stderr"] = kwargs["stderr"]
            state["argv"] = argv
            kwargs["stderr"].write(b"header " + b"secret-value " * 1000)
            kwargs["stderr"].flush()
            self.returncode = None
            state["process"] = self

        def poll(self):
            return self.returncode

        def terminate(self):
            self.returncode = 0

        def wait(self, timeout):
            return self.returncode

    monkeypatch.setattr(meta_process.subprocess, "Popen", Process)
    env = {"TETRIS_RELAY_SECRET": "secret-value"}
    with pytest.raises(meta_process.MetaServerStartupError) as error:
        with meta_process.local_meta_server(
            tmp_path / "tetris_meta", tmp_path / "data.db", timeout=1, env=env
        ):
            pytest.fail("server without a port announcement became ready")
    message = str(error.value)
    assert "timed out waiting for port announcement" in message
    assert "secret-value" not in message
    diagnostic = message.split("first stderr bytes:\n", 1)[1]
    assert len(diagnostic.encode("utf-8")) <= meta_process._DIAG_LIMIT
    assert state["argv"][-2:] != ["--relay-secret", "secret-value"]
    assert state["process"].returncode == 0
    assert 1 <= clock.now < 1.02
