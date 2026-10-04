"""Pairing event order must not depend on the resolution of the OS clock."""
import io
import threading
from types import SimpleNamespace

import pytest

from . import test_relay_meta_smoke as smoke


def reader_with_old_pairing():
    reader = smoke._StatsReader.__new__(smoke._StatsReader)
    reader._lock = threading.Lock()
    reader._pairings = [(1.0, 2, 3)]
    reader._closes = []
    reader._samples = []
    reader.lines = []
    return reader


def test_pairing_cursor_excludes_existing_events():
    reader = reader_with_old_pairing()
    cursor = reader.pairing_cursor()
    with pytest.raises(AssertionError, match="페어링 줄"):
        reader.wait_for_pairing_after(cursor, timeout=0)


@pytest.mark.parametrize("timestamp", [1.0, 1.001])
def test_parser_retains_new_pairing_even_when_clock_does_not_advance(monkeypatch, timestamp):
    reader = reader_with_old_pairing()
    cursor = reader.pairing_cursor()
    monkeypatch.setattr(smoke, "time", SimpleNamespace(monotonic=lambda: timestamp))
    reader._proc = SimpleNamespace(stdout=io.BytesIO(b"I [relay] paired conn 4 x 5\n"))
    reader._run()
    assert reader.wait_for_pairing_after(cursor, timeout=0) == (4, 5)
    # The consumed event cannot satisfy the next wait either.
    with pytest.raises(AssertionError, match="페어링 줄"):
        reader.wait_for_pairing_after(reader.pairing_cursor(), timeout=0)


def test_cursor_observes_pairing_from_reader_thread():
    reader = reader_with_old_pairing()
    cursor = reader.pairing_cursor()
    reader._proc = SimpleNamespace(stdout=io.BytesIO(b"I [relay] paired conn 6 x 7\n"))
    worker = threading.Thread(target=reader._run)
    worker.start()
    try:
        assert reader.wait_for_pairing_after(cursor, timeout=1) == (6, 7)
    finally:
        worker.join(timeout=1)
        assert not worker.is_alive()
