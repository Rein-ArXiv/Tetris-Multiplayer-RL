"""Atomic single-file checkpoint saving with torch.

``atomic_torch_save`` serializes ``payload`` to a temporary file in the
destination's parent directory and commits it with ``os.replace``.

The replace is the commit point: the previous destination stays intact until
then. There is no parent-directory fsync, so this is atomic against concurrent
readers and in-process failures but does not guarantee durability across a power loss or OS crash.
The destination's parent directory must already exist and is never created.
Serialization is synchronous and reads the live objects, so callers must
freeze the model/state they pass while saving. Only one writer may target a
given path at a time.
"""

import os
import tempfile
from pathlib import Path

import torch


def atomic_torch_save(payload, path):
    """Serialize ``payload`` to ``path`` atomically using ``torch.save``.

    Args:
        payload: Any object pickleable by ``torch.save``.
        path: Destination file path.
    """
    dest = Path(path)
    fd, tmp_name = tempfile.mkstemp(
        dir=dest.parent, prefix=f".{dest.name}.", suffix=".tmp"
    )
    try:
        try:
            stream = os.fdopen(fd, "wb")
        except BaseException:
            os.close(fd)
            raise
        with stream as fh:
            torch.save(payload, fh)
            fh.flush()
            os.fsync(fh.fileno())
        os.replace(tmp_name, dest)
    except BaseException:
        try:
            os.unlink(tmp_name)
        except OSError:
            pass
        raise
