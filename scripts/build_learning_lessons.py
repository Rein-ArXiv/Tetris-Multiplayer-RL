"""Compile reviewed lecture manuscripts and checked quiz data for the HTML reader.

Manuscripts are JSON; short body fields support Markdown for authoring only.
Learners see rendered HTML. Checkpoint files are stable teaching code, independent
of mutable repository source snapshots.
"""
from __future__ import annotations

import argparse
import html
import json
import re
from pathlib import Path

from learning_markdown import make_markdown

ROOT = Path(__file__).resolve().parents[1]
LEARN = ROOT / "docs/learn"
DEST = LEARN / "lessons.js"


def validate_questions(questions: list, lesson_id: str) -> None:
    if not 3 <= len(questions) <= 5:
        raise ValueError(f"Expected 3–5 questions: {lesson_id}")
    ids = set()
    for q in questions:
        if (q["id"] in ids
                or not re.fullmatch(rf"{lesson_id.split('-')[1]}-[1-5]", q["id"])
                or not all(q.get(k) for k in ("question", "answer", "explanation"))):
            raise ValueError(f"Incomplete/duplicate question: {lesson_id}")
        ids.add(q["id"])
        options = q.get("options", [])
        if (len(options) != 4 or [o.get("id") for o in options] != list("abcd")
                or not all(isinstance(o.get("text"), str) and o["text"].strip() for o in options)
                or len({o["text"].strip() for o in options}) != 4
                or q.get("correctOptionId", "") not in list("abcd")
                or len(q.get("correctOptionId", "")) != 1):
            raise ValueError(f"Invalid multiple-choice options: {q['id']}")


def build() -> str:
    md = make_markdown()
    default_fence = md.renderer.rules["fence"]
    def fence_renderer(items, index, options, env):
        token = items[index]
        if token.info.strip() == "mermaid":
            code = html.escape(token.content)
            return f'<div class="diagram" data-diagram><div class="diagram-drawing"></div><details><summary>다이어그램 원문</summary><pre class="mermaid-source"><code>{code}</code></pre></details></div>'
        return default_fence(items, index, options, env)
    md.renderer.rules["fence"] = fence_renderer
    def render_markdown(text: str) -> str:
        # All author text, including optional notes, uses the same safe renderer.
        tokens = md.parse(text)
        for token in tokens:
            for child in token.children or []:
                if child.type == "link_open" and (child.attrGet("href") or "").startswith(("https://", "http://")):
                    child.attrSet("target", "_blank")
                    child.attrSet("rel", "noopener noreferrer")
                    child.attrSet("title", "새 탭에서 참고 자료 열기")
        return md.renderer.render(tokens, md.options, {})

    manifest = json.loads((LEARN / "library-manifest.json").read_text())
    plan = json.loads((LEARN / "course-plan.json").read_text())
    unit_ids = {u["lessonId"] for m in plan["modules"] for u in m["units"]}
    manuscripts = sorted((LEARN / "lessons").glob("*.json"))
    lessons = []
    for path in manuscripts:
        item = json.loads(path.read_text())
        if item["state"] != "reviewed":
            continue
        if item["id"] not in unit_ids or not re.fullmatch(r"lesson-\d+", item["id"]):
            raise ValueError(f"Unknown lesson: {path}")
        if not item.get("review") or not item.get("version"):
            raise ValueError(f"Missing review/version: {path}")
        validate_questions(item["questions"], item["id"])
        practice = item.get("practice")
        if (not isinstance(practice, dict)
                or not isinstance(practice.get("expected"), str)
                or not practice["expected"].strip()
                or any(not isinstance(practice.get(key), list)
                       or not practice[key]
                       or not all(isinstance(step, str) and step.strip() for step in practice[key])
                       for key in ("steps", "failureChecks"))):
            raise ValueError(f"Missing or invalid practice data: {path}")

        recap = item.get("recap", {})
        if (not 2 <= len(recap.get("points", [])) <= 3
                or not all(isinstance(p, str) and p.strip() for p in recap["points"])
                or not recap.get("bridge")):
            raise ValueError(f"Missing short recap: {path}")
        for section in item["sections"]:
            section["html"] = render_markdown(section.pop("body"))
            for note in section.get("notes", []):
                if not all(isinstance(note.get(key), str) and note[key].strip()
                           for key in ("kind", "title", "body")):
                    raise ValueError(f"Incomplete side note: {path}")
                note["html"] = render_markdown(note.pop("body"))
            codes = section.get("codes", [])
            if not isinstance(codes, list):
                raise ValueError(f"Code blocks must be a list: {path} / {section.get('title')}")
            for code in codes:
                if (not isinstance(code, dict)
                        or not all(isinstance(code.get(key), str) and code[key].strip()
                                   for key in ("label", "language"))
                        or ("file" in code) == ("text" in code)):
                    raise ValueError(f"Code block needs label/language and exactly one file or text: {path}")
                source_key = "file" if "file" in code else "text"
                if not isinstance(code[source_key], str) or not code[source_key].strip():
                    raise ValueError(f"Empty or invalid code source: {path}")
                if "file" in code:
                    checkpoint = (LEARN / code["file"]).resolve()
                    if not checkpoint.is_relative_to((LEARN / "checkpoints").resolve()):
                        raise ValueError("Checkpoint must live in docs/learn/checkpoints")
                    code["text"] = checkpoint.read_text()
            ref = section.get("reference")
            if ref:
                if ref["path"] not in manifest["sources"]:
                    raise ValueError(f"Reference not in viewer: {ref['path']}")
                if not ref["symbol"] or "\n" in ref["symbol"] or ref["symbol"] not in (ROOT / ref["path"]).read_text():
                    raise ValueError(f"Reference symbol missing: {ref}")
        lessons.append(item)
    lessons.sort(key=lambda lesson: int(lesson["id"].split("-")[1]))
    if len({x["id"] for x in lessons}) != len(lessons):
        raise ValueError("Duplicate lesson id")
    # Lesson 1 prose remains hand-authored; all quiz options share versioned data.
    first = json.loads((LEARN / "lesson-1-questions.json").read_text())
    validate_questions(first, "lesson-1")
    plan = json.loads((LEARN / "course-plan.json").read_text())
    catalog = {"modules": [{"title": m["title"], "units": [{k: u[k] for k in ("id", "title", "lessonId")} for u in m["units"]]} for m in plan["modules"]]}
    payload = {"coursePlan": catalog, "firstQuestions": first, "lessons": lessons, "coverage": json.loads((LEARN / "coverage.json").read_text())}
    return "// Generated from reviewed manuscripts by scripts/build_learning_lessons.py.\nwindow.LEARNING_LESSONS = " + json.dumps(payload, ensure_ascii=False, separators=(",", ":")) + ";\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    output = build()
    if args.check:
        if not DEST.exists() or DEST.read_text() != output:
            print("Learning lessons are stale. Run scripts/build_learning_lessons.py")
            return 1
    else:
        DEST.write_text(output)
    print("Reviewed lecture bundle, multiple-choice questions and short recaps verified")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
