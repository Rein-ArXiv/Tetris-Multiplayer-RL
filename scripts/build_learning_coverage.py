"""Index every Part section without mistaking topic assignment for authored coverage.

Run after editing a Part or the course plan. Manual evidence belongs in
docs/learn/coverage-evidence.json; generated candidates are not review evidence.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
from pathlib import Path

from markdown_it import MarkdownIt

ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT / "docs/learn/coverage.json"


def build() -> dict:
    plan = json.loads((ROOT / "docs/learn/course-plan.json").read_text())
    evidence_path = ROOT / "docs/learn/coverage-evidence.json"
    evidence = json.loads(evidence_path.read_text()) if evidence_path.exists() else {}
    routing_path = ROOT / "docs/learn/coverage-routing.json"
    routing = json.loads(routing_path.read_text())["assignments"] if routing_path.exists() else {}
    units = [u for m in plan["modules"] for u in m["units"]]
    unit_ids = {u["id"] for u in units}
    rows = []
    md = MarkdownIt("commonmark")
    for path in sorted((ROOT / "docs/blog").glob("part*.md"), key=lambda p: int(re.match(r"part(\d+)", p.name)[1])):
        part = int(re.match(r"part(\d+)", path.name)[1])
        text = path.read_text()
        lines = text.splitlines(keepends=True)
        tokens = md.parse(text)
        headings = [(t.map[0], tokens[i + 1].content) for i, t in enumerate(tokens) if t.type == "heading_open" and t.tag == "h2"]
        # Include any introduction before the first H2 instead of silently dropping it.
        starts = [(0, "표제와 도입")] + headings if headings and headings[0][0] else headings
        if not starts:
            starts = [(0, "전체 문서")]
        for index, (start, title) in enumerate(starts):
            end = starts[index + 1][0] if index + 1 < len(starts) else len(lines)
            digest = hashlib.sha256("".join(lines[start:end]).encode()).hexdigest()
            ident = f"part{part}--section-{index:02d}"
            record = evidence.get(ident, {})
            route = routing.get(ident, {})
            candidates = route.get("units", [])
            if len(set(candidates)) != len(candidates) or not set(candidates) <= unit_ids:
                raise ValueError(f"Invalid candidate unit in {ident}")
            routing_current = route.get("sha256") == digest
            rows.append({
                "id": ident, "part": part, "path": path.relative_to(ROOT).as_posix(),
                "title": title, "startLine": start + 1, "endLine": end, "sha256": digest,
                "candidateUnits": candidates if routing_current else [u["id"] for u in units if part in u.get("referenceParts", [])],
                "routingState": "draft" if routing_current else "needs-review" if route else "module-only",
                "lessons": record.get("lessons", []),
                "state": (record.get("state", "unassigned") if record.get("sha256") == digest else "needs-review") if record else "unassigned",
                "note": record.get("note", "후보 차시만 존재하며 내용 대응은 아직 검토하지 않음"),
            })
    missing = set(evidence) - {r["id"] for r in rows}
    if missing:
        raise ValueError(f"Evidence points to missing sections: {sorted(missing)}")
    if set(routing) - {r["id"] for r in rows}:
        raise ValueError("Routing points to missing sections")
    return {"version": 1, "note": "Candidates are routing hints, not completion. Covered requires explicit evidence tied to the section hash.", "sections": rows}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    payload = build()
    output = json.dumps(payload, ensure_ascii=False, indent=2) + "\n"
    if args.check:
        if not DEST.exists() or DEST.read_text() != output:
            print("Learning coverage is stale. Run scripts/build_learning_coverage.py")
            return 1
    else:
        DEST.write_text(output)
    covered = sum(row["state"] == "covered" for row in payload["sections"])
    print(f"Learning coverage: {len(payload['sections'])} sections, {covered} fully reviewed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
