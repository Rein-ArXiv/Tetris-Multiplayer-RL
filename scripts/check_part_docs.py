"""Check Part structure, current-source excerpts and local Markdown file links.

Excerpts must remain contiguous excerpts of their named source. C/C++ comments
and formatting may differ; other languages normalize whitespace only. This is a
drift check, not a compiler or a proof that the surrounding explanation is right.
"""
from __future__ import annotations

import json
import re
from functools import lru_cache
from pathlib import Path
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parents[1]
LABEL = re.compile(r"\*\*(?:현재 소스 발췌 — `[^`]+`|Part \d+ 체크포인트 — `[^`]+`|예시\(실제 저장소에는 없음\))\*\*")
FENCE = re.compile(r"^```[^\n]*\n.*?^```[^\n]*$", re.M | re.S)
EXCERPT = re.compile(
    r"\*\*현재 소스 발췌 — `([^`]+)`\*\*\s*```([^\n]+)\n(.*?)\n```", re.S
)
CXX_TOKEN = re.compile(
    r'R"(?P<delimiter>[^\s()\\]{0,16})\(.*?\)(?P=delimiter)"'
    r'|//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\''
    r'|[A-Za-z_]\w*|\d+|[^\s]', re.S
)


def normalized(text: str, language: str) -> str:
    if language in {"cpp", "c"}:
        # Delimit tokens on both ends so a partial identifier cannot match.
        tokens = [m[0] for m in CXX_TOKEN.finditer(text)
                  if not m[0].startswith(("//", "/*"))]
        return "\0" + "\0".join(tokens) + "\0" if tokens else ""
    return " ".join(text.split())


@lru_cache(maxsize=None)
def source_text(path: Path, language: str) -> str:
    text = path.read_text(encoding="utf-8")
    if path.suffix == ".ipynb":
        notebook = json.loads(text)
        text = "\n\n".join("".join(cell["source"]) for cell in notebook["cells"]
                           if cell["cell_type"] == "code")
    return normalized(text, language)


def check() -> list[str]:
    errors = []
    for path in sorted((ROOT / "docs/blog").glob("part*.md")):
        text = path.read_text(encoding="utf-8")
        number = int(re.match(r"part(\d+)", path.name)[1])
        lines = text.splitlines()
        series = f"> **시리즈:** 제로부터 멀티플레이어 테트리스 + RL | [시리즈 목차](./README.md) | **Part {number}**"
        if not lines[0].startswith(f"# Part {number}: ") or lines[1:6] != ["", series, "", "---", ""]:
            errors.append(f"{path.relative_to(ROOT)}: series header differs")
        if "## 이번 Part의 구현 계약" not in lines:
            errors.append(f"{path.relative_to(ROOT)}: missing implementation contract")
        for match in re.finditer(r"^```(?:cpp|c|python|cmake)\n", text, re.M):
            prior = text[:match.start()].rstrip().splitlines()[-1]
            if not LABEL.fullmatch(prior):
                line = text[:match.start()].count("\n") + 1
                errors.append(f"{path.relative_to(ROOT)}:{line}: missing canonical code label")
        for match in EXCERPT.finditer(text):
            source = ROOT / match[1]
            line = text[:match.start()].count("\n") + 1
            location = f"{path.relative_to(ROOT)}:{line}"
            if not source.is_file():
                errors.append(f"{location}: missing excerpt source {match[1]}")
            elif normalized(match[3], match[2]) not in source_text(source, match[2]):
                errors.append(f"{location}: excerpt differs from {match[1]}")
    for path in sorted((ROOT / "docs").rglob("*.md")):
        prose = FENCE.sub("", path.read_text(encoding="utf-8"))
        for match in re.finditer(r"\[[^\]\n]*\]\((<?[^\s)]+>?)\)", prose):
            link = match[1].strip("<>")
            parsed = urlsplit(link)
            if parsed.scheme or not parsed.path or link.startswith("//"):
                continue
            target = path.parent / unquote(parsed.path)
            if not target.exists():
                errors.append(f"{path.relative_to(ROOT)}: missing local file {link}")
    return errors


if __name__ == "__main__":
    errors = check()
    for error in errors:
        print(error)
    if not errors:
        print("Part headers, contracts, code labels, current-source excerpts and local links: OK")
    raise SystemExit(bool(errors))
