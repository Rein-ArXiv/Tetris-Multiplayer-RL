"""Pytest config — make ``python/`` importable so tests can use
``from common.checkpoint import ...`` etc. without an editable install.
"""

from __future__ import annotations

import sys
from pathlib import Path

PY_DIR = Path(__file__).resolve().parents[1]
if str(PY_DIR) not in sys.path:
    sys.path.insert(0, str(PY_DIR))


import pytest

@pytest.fixture(autouse=True)
def legacy_relay_fixture(request, monkeypatch):
    """Older wire fixtures intentionally send reusable credentials. Production
    defaults and secure transport tests must never inherit that compatibility mode.
    """
    if request.node.path.name not in {"test_secure_admission.py", "test_account_security.py"}:
        monkeypatch.setenv("TETRIS_RELAY_LEGACY_AUTH", "1")
    else:
        monkeypatch.delenv("TETRIS_RELAY_LEGACY_AUTH", raising=False)
