"""Package only the configured opponents, models and images (no checkpoints).
Run on Colab or locally: python python/tools/package_opponents.py --out opponents.zip
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import sys

# Allow the documented direct-script invocation and normal module imports.
if __package__ in (None, ""):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.opponent_profile import split_profile_line
import zipfile


def package(root: Path, out: Path) -> None:
    root = root.resolve()
    config = root / "assets/opponents.cfg"
    members = {"assets/opponents.cfg": config}
    ids = set()
    for number, line in enumerate(config.read_text(encoding="utf-8").split('\n'), 1):
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
            if path.is_absolute() or ".." in path.parts or "\\" in asset or path.parts[0] != folder or path.suffix.lower() not in extensions:
                raise ValueError(f"unsafe or unsupported asset path: {asset}")
            local = (root / asset).resolve()
            if not local.is_relative_to(root) or not local.is_file():
                raise ValueError(f"missing or external asset: {asset}")
            members[asset] = local
    if not ids:
        raise ValueError("no opponents configured")
    # Exclusive creation prevents accidental replacement of a previous release.
    with zipfile.ZipFile(out, "x", compression=zipfile.ZIP_DEFLATED) as archive:
        hashes = {}
        for name, local in sorted(members.items()):
            data = local.read_bytes()
            archive.writestr(name, data)
            hashes[name] = hashlib.sha256(data).hexdigest()
        archive.writestr("opponents-manifest.json", json.dumps({"format": 1, "sha256": hashes}, indent=2) + "\n")
    print(f"Packaged {len(ids)} opponents, {len(members)} files: {out}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    package(args.root, args.out)
