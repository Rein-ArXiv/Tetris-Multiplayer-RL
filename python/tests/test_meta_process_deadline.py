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
