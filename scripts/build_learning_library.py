"""Build the offline reference/code panel used beside the authored HTML lessons.

This is a lossless reference import, not a generator of finished lessons. Teaching
order, explanations, exercises and answers are authored separately. No AI/API is
required for indexing Markdown and snapshotting explicitly referenced sources.
Run: uv run --with markdown-it-py==3.0.0 python scripts/build_learning_library.py
"""
from __future__ import annotations

import argparse
import hashlib
import html
import json
import re
from pathlib import Path
from urllib.parse import unquote, urlsplit

from learning_markdown import make_markdown

ROOT = Path(__file__).resolve().parents[1]
BLOG = ROOT / "docs/blog"
DEST = ROOT / "docs/learn/library.js"
REPORT = ROOT / "docs/learn/library-manifest.json"
SOURCE_ROOTS = {"bindings", "src", "core", "renderer", "platform", "audio", "net", "server", "meta", "bot", "python", "tests", "scripts", "web"}
SOURCE_EXTENSIONS = {".cpp", ".h", ".hpp", ".c", ".py", ".sh", ".ps1", ".html"}


def source_allowed(path: Path) -> bool:
    """Never enumerate user credentials, databases, .git or local settings."""
    try:
        rel = path.relative_to(ROOT)
    except ValueError:
        return False
    if not rel.parts or any(part.startswith(".") or part == "__pycache__" for part in rel.parts):
        return False
    # Reject symlinked parents as well as the final component; snapshots stay
    # inside the named repository path, without following external targets.
    if path.resolve() != path or not path.is_file():
        return False
    return rel.as_posix() in {"CMakeLists.txt", "CMakePresets.json", "cmake/TetrisOnnxRuntime.cmake",
        "deploy/systemd/tetris-meta.service", "deploy/systemd/tetris-relay.service",
        "deploy/systemd/tetris-wss.service"} or (
        rel.parts[0] in SOURCE_ROOTS and path.suffix in SOURCE_EXTENSIONS
    )


def lesson_source_paths(directory: Path) -> set[Path]:
    """A reviewed lesson's explicit code reference survives prose reorganization."""
    sources = set()
    for manuscript in sorted(directory.glob("*.json")):
        lesson = json.loads(manuscript.read_text(encoding="utf-8"))
        if lesson.get("state") != "reviewed":
            continue
        for section in lesson.get("sections", []):
            reference = section.get("reference")
            if not reference:
                continue
            name = reference.get("path")
            if not isinstance(name, str) or not source_allowed(ROOT / name):
                raise ValueError(f"Invalid lesson source reference in {manuscript.name}: {name!r}")
            sources.add(ROOT / name)
    return sources


def build() -> tuple[str, str]:
    md = make_markdown()
    paths = [BLOG / "README.md"] + sorted(BLOG.glob("part*.md"), key=lambda p: int(re.search(r"part(\d+)", p.name)[1]))
    paths += sorted(p for p in (ROOT / "docs").glob("*.md") if p.name not in {"learning-companion.md", "README.md"})
    docs = {}
    path_to_doc = {p.resolve(): ("part" + re.search(r"part(\d+)", p.name)[1] if re.match(r"part\d+-", p.name) else p.stem) for p in paths}
    source_paths = {ROOT / "CMakeLists.txt", ROOT / "CMakePresets.json"} | lesson_source_paths(ROOT / "docs/learn/lessons")
    # Only source files actually named in the teaching corpus become browser data.
    for path in paths:
        text = path.read_text(encoding="utf-8")
        for candidate in re.findall(r"(?:bindings|src|core|renderer|platform|audio|net|server|meta|bot|python|tests|scripts|web)/[\w./-]+\.(?:cpp|hpp|h|c|py|sh|ps1|html)\b", text):
            resolved = ROOT / candidate
            if source_allowed(resolved):
                source_paths.add(resolved)

    for path in paths:
        raw = path.read_text(encoding="utf-8")
        docid = path_to_doc[path.resolve()]
        tokens = md.parse(raw)
        headings = []
        heading_counts = {}
        for index, token in enumerate(tokens):
            if token.type == "heading_open":
                title = tokens[index + 1].content
                slug = re.sub(r"[^\w\-\s]", "", title.lower()).replace(" ", "-")
                number = heading_counts.get(slug, 0)
                heading_counts[slug] = number + 1
                slug += f"-{number}" if number else ""
                ident = f"ref-{docid}-{len(headings)}"
                token.attrSet("id", ident)
                headings.append({"title": title, "level": int(token.tag[1:]), "line": token.map[0] + 1, "id": ident, "slug": slug})
            for child in token.children or []:
                if child.type != "link_open":
                    continue
                href = child.attrGet("href") or ""
                parsed = urlsplit(href)
                if parsed.scheme or parsed.netloc:
                    if parsed.scheme in {"http", "https"}:
                        child.attrSet("target", "_blank")
                        child.attrSet("rel", "noopener noreferrer")
                    continue
                resolved = (path.parent / unquote(parsed.path)).resolve() if parsed.path else path.resolve()
                if resolved in path_to_doc:
                    target = path_to_doc[resolved]
                    child.attrSet("href", "#reference")
                    child.attrSet("data-doc", target)
                    child.attrSet("data-doc-anchor", unquote(parsed.fragment))
                elif resolved in source_paths:
                    child.attrSet("href", "#source")
                    child.attrSet("data-source", resolved.relative_to(ROOT).as_posix())
                else:
                    # An unbundled link must not quietly open a missing localhost path.
                    child.attrSet("href", "#reference")
                    child.attrSet("data-unbundled", href)
        fence_counter = 0
        default_fence = md.renderer.rules["fence"]

        def fence_renderer(items, idx, options, env):
            nonlocal fence_counter
            token = items[idx]
            fence_counter += 1
            lang = token.info.strip().split(" ")[0]
            if lang == "mermaid":
                code = html.escape(token.content)
                return f'<div class="diagram" data-diagram><div class="diagram-drawing"></div><details><summary>다이어그램 원문</summary><pre class="mermaid-source"><code>{code}</code></pre></details></div>'
            rendered = default_fence(items, idx, options, env)
            return f'<div class="reference-code"><div class="codebar"><span>{html.escape(lang or "text")}</span><button type="button" data-copy-block>코드 복사</button></div>{rendered}</div>'

        md.renderer.rules["fence"] = fence_renderer
        rendered = md.renderer.render(tokens, md.options, {})
        md.renderer.rules["fence"] = default_fence
        docs[docid] = {
            "title": headings[0]["title"] if headings else path.stem,
            "path": path.relative_to(ROOT).as_posix(), "headings": headings,
            "html": rendered, "sha256": hashlib.sha256(raw.encode()).hexdigest(),
            "lines": len(raw.splitlines()), "codeBlocks": fence_counter,
        }
    sources = {}
    for path in sorted(source_paths):
        text = path.read_text(encoding="utf-8")
        sources[path.relative_to(ROOT).as_posix()] = {
            "text": text, "sha256": hashlib.sha256(text.encode()).hexdigest(),
            "lines": len(text.splitlines()),
        }
    plan_path = ROOT / "docs/learn/course-plan.json"
    payload = {"documents": docs, "sources": sources, "coursePlan": json.loads(plan_path.read_text()) if plan_path.exists() else None, "scope": "Reference corpus and named source snapshots; not authored lesson status"}
    output = "// Generated by scripts/build_learning_library.py; do not edit.\nwindow.LEARNING_LIBRARY = " + json.dumps(payload, ensure_ascii=False, separators=(",", ":")) + ";\n"
    manifest = json.dumps({
        "documents": {k: {field: v[field] for field in ("path", "sha256", "lines", "codeBlocks")} for k, v in docs.items()},
        "sources": {k: {field: v[field] for field in ("sha256", "lines")} for k, v in sources.items()},
        "note": "All Part 0–18 text is preserved as reference. Authored lessons are tracked separately.",
    }, ensure_ascii=False, indent=2) + "\n"
    return output, manifest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="Fail when bundled documents/code no longer match the repository")
    args = parser.parse_args()
    output, manifest = build()
    if args.check:
        if not DEST.exists() or DEST.read_text() != output or not REPORT.exists() or REPORT.read_text() != manifest:
            print("Learning library is stale. Run scripts/build_learning_library.py")
            return 1
    else:
        DEST.write_text(output, encoding="utf-8")
        REPORT.write_text(manifest, encoding="utf-8")
    data = json.loads(manifest)
    print(f"Learning library: {len(data['documents'])} documents, {len(data['sources'])} source files; hashes and full code blocks preserved")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
