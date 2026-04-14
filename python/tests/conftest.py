"""Pytest config — make ``python/`` importable so tests can use
``from common.checkpoint import ...`` etc. without an editable install.
"""

from __future__ import annotations

import sys
from pathlib import Path

PY_DIR = Path(__file__).resolve().parents[1]
if str(PY_DIR) not in sys.path:
    sys.path.insert(0, str(PY_DIR))
