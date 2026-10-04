"""Package only the configured opponents, models and images (no checkpoints).
Run this checkpoint helper with --root CHECKPOINT --out NEW_ARCHIVE.zip
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
import tempfile
from pathlib import Path, PurePosixPath
import sys

from opponent_profile import split_profile_line
import zipfile


def publish_archive(out: Path, config_bytes: bytes, members: dict[str, Path]) -> None:
    """Publish complete bytes without overwrite, from a trusted frozen source tree.

    Config was captured/validated by the caller. Hashes describe archived bytes;
    they do not authenticate a publisher or validate model behavior. The parent
    is application-owned; directory durability after power loss is separate.
    """
    out = Path(out)  # Preserve a final symlink as an existing name, never resolve it.
    if os.path.lexists(out):
        raise FileExistsError(out)
    config_name, manifest_name = 'assets/opponents.cfg', 'opponents-manifest.json'
    if {config_name, manifest_name} & members.keys():
        raise ValueError('reserved archive member')
    fd, name = tempfile.mkstemp(prefix='.opponents-', suffix='.tmp', dir=out.parent)
    temporary = Path(name)
    try:
        # mkstemp supplies owner-only mode on POSIX; Windows uses the parent ACL.
        with os.fdopen(fd, 'w+b') as raw:
            hashes = {}
            with zipfile.ZipFile(raw, 'w', compression=zipfile.ZIP_DEFLATED) as archive:
                def write(member, data):
                    info = zipfile.ZipInfo(member, date_time=(1980, 1, 1, 0, 0, 0))
                    info.create_system = 3
                    info.external_attr = 0o100600 << 16
                    info.compress_type = zipfile.ZIP_DEFLATED
                    archive.writestr(info, data)
                for member in sorted([config_name, *members]):
                    data = config_bytes if member == config_name else members[member].read_bytes()
                    write(member, data)
                    hashes[member] = hashlib.sha256(data).hexdigest()
                write(manifest_name, (json.dumps({'format': 1, 'sha256': hashes},
                                                 sort_keys=True, indent=2) + '\n').encode('utf-8'))
            raw.flush()
            os.fsync(raw.fileno())
        # Same-parent hard link is atomic/no-overwrite. Unsupported filesystems fail.
        os.link(temporary, out)
    finally:
        temporary.unlink(missing_ok=True)


def package(root: Path, out: Path) -> None:
    root = root.resolve()
    config = (root / "assets/opponents.cfg").resolve()
    if not config.is_relative_to(root) or not config.is_file():
        raise ValueError("missing or external opponent config")
    config_bytes = config.read_bytes()
    members = {}
    ids = set()
    for number, line in enumerate(config_bytes.decode("utf-8").split('\n'), 1):
        try:
            fields = split_profile_line(line)
        except ValueError as error:
            raise ValueError(f"line {number}: {error}") from error
        if fields is None:
            continue
        identity, name, model, icon, portrait, difficulty, interval, think, minimum = fields
        if identity in ids:
            raise ValueError(f"line {number}: duplicate identity")
        ids.add(identity)
        for asset, folder, extensions in ((model, "model", {".onnx"}), (icon, "assets", {".png", ".jpg", ".jpeg"}), (portrait, "assets", {".png", ".jpg", ".jpeg"})):
            if not asset or asset == "@heuristic":
                if asset == "@heuristic" and folder != "model":
                    raise ValueError("image must be a path")
                if not asset and folder == "model":
                    raise ValueError("model cannot be empty")
                continue
            path = PurePosixPath(asset)
            if not path.parts or path.is_absolute() or ".." in path.parts or "\\" in asset or path.parts[0] != folder or path.suffix.lower() not in extensions:
                raise ValueError(f"unsafe or unsupported asset path: {asset}")
            local = (root / asset).resolve()
            if not local.is_relative_to(root) or not local.is_file():
                raise ValueError(f"missing or external asset: {asset}")
            members[asset] = local
    if not ids:
        raise ValueError("no opponents configured")
    publish_archive(out, config_bytes, members)
    print(f"Packaged {len(ids)} opponents, {len(members) + 1} files: {out}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    package(args.root, args.out)
