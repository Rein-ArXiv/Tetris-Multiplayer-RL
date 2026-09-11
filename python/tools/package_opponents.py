"""Package only the configured opponents, models and images (no checkpoints).
Run on Colab or locally: python python/tools/package_opponents.py --out opponents.zip
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import zipfile


def package(root: Path, out: Path) -> None:
    root = root.resolve()
    config = root / "assets/opponents.cfg"
    members = {"assets/opponents.cfg": config}
    ids = set()
    for number, line in enumerate(config.read_text(encoding="utf-8").splitlines(), 1):
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        fields = [s.strip() for s in line.split("|")]
        if len(fields) != 9:
            raise ValueError(f"line {number}: expected 9 fields")
        identity, name, model, icon, portrait, difficulty, interval, think, minimum = fields
        if not re.fullmatch(r"[a-z0-9_-]{1,32}", identity) or identity in ids:
            raise ValueError(f"line {number}: invalid or duplicate identity")
        ids.add(identity)
        if not name or len(name.encode("utf-8")) > 96:
            raise ValueError(f"line {number}: invalid name")
        for value, low, high in ((interval, 1, 30), (think, 0, 180), (minimum, 1, 600)):
            if not re.fullmatch(r"[0-9]+", value) or not low <= int(value) <= high:
                raise ValueError(f"line {number}: invalid pacing")
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
