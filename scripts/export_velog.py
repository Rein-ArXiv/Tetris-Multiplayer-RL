"""블로그 시리즈를 Velog 게시용으로 내보낸다.

Velog 는 저장소를 모른다 — 상대경로 md 링크는 깨지고 mermaid 는 렌더링되지 않는다.
이 스크립트는 docs/blog 의 각 파트를 다음과 같이 변환해 out/velog/ 에 모은다.

1. mermaid 블록 → PNG 렌더링 (mermaid-cli, 2x 스케일 / 흰 배경) 후
   본문을 ``![diagram](./images/<파일>-N.png)`` 참조로 치환.
   이미지 전부를 out/velog/images/ 한 폴더에 모은다.
2. 로컬 전용 링크(상대경로 md 등 http(s) 가 아닌 모든 링크)는 라벨 텍스트만 남기고
   제거. 외부 http(s) 링크는 유지.

게시 절차: Velog 에디터에 out/velog/<파트>.md 본문을 붙여넣고, ./images/ 참조가
나오는 자리마다 대응 PNG 를 업로드하면 된다 (파일명이 순서를 보장한다).

요구 사항: Node.js (npx 로 @mermaid-js/mermaid-cli 를 내려받아 실행).
출력 디렉터리는 gitignore 대상(out/)이라 언제든 재생성한다.
"""
from __future__ import annotations

import re
import shutil
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
SRC_DIR = REPO / "docs" / "blog"
OUT_DIR = REPO / "out" / "velog"
IMG_DIR = OUT_DIR / "images"
TMP_DIR = OUT_DIR / "_tmp"

# _style_guide.md 는 집필 에이전트용 내부 메모 — 게시 대상이 아니다.
EXCLUDE = {"_style_guide.md"}

# ![이미지](...) 는 건드리지 않고, http(s) 가 아닌 링크만 라벨 텍스트로 축약.
LOCAL_LINK = re.compile(r"(?<!\!)\[([^\]]+)\]\((?!https?://)[^)]+\)")


def render_mermaid(src: Path, tmp_md: Path) -> bool:
    """mermaid-cli 의 markdown 모드: 블록을 PNG 로 뽑고 참조로 치환한 md 를 쓴다."""
    cmd = [
        "npx", "-y", "@mermaid-js/mermaid-cli",
        "-i", str(src), "-o", str(tmp_md),
        "--outputFormat", "png", "-b", "white", "-s", "2",
    ]
    proc = subprocess.run(cmd, capture_output=True, text=True,
                          encoding="utf-8", errors="replace",
                          shell=(sys.platform == "win32"))
    if proc.returncode != 0:
        print(f"  [FAIL] mermaid 렌더링 실패: {src.name}\n{proc.stderr[-800:]}")
        return False
    return True


def main() -> int:
    if TMP_DIR.exists():
        shutil.rmtree(TMP_DIR)
    TMP_DIR.mkdir(parents=True)
    IMG_DIR.mkdir(parents=True, exist_ok=True)

    sources = sorted(p for p in SRC_DIR.glob("*.md") if p.name not in EXCLUDE)
    total_imgs = 0
    total_links = 0
    failures: list[str] = []

    for src in sources:
        stem = src.stem
        has_mermaid = "```mermaid" in src.read_text(encoding="utf-8")
        tmp_md = TMP_DIR / src.name

        if has_mermaid:
            if not render_mermaid(src, tmp_md):
                failures.append(src.name)
                continue
        else:
            shutil.copyfile(src, tmp_md)

        # mmdc 가 tmp 에 남긴 <stem>-N.png 를 images/ 로 모으고 참조 경로를 맞춘다.
        n_imgs = 0
        for png in sorted(TMP_DIR.glob(f"{stem}-*.png")):
            shutil.move(str(png), IMG_DIR / png.name)
            n_imgs += 1

        text = tmp_md.read_text(encoding="utf-8")
        text = text.replace(f"](./{stem}-", f"](./images/{stem}-")

        n_links = len(LOCAL_LINK.findall(text))
        text = LOCAL_LINK.sub(r"\1", text)

        (OUT_DIR / src.name).write_text(text, encoding="utf-8")
        total_imgs += n_imgs
        total_links += n_links
        print(f"  {src.name:45s} 이미지 {n_imgs:2d}개, 로컬 링크 제거 {n_links:2d}개")

    shutil.rmtree(TMP_DIR, ignore_errors=True)

    print()
    print(f"완료: 문서 {len(sources) - len(failures)}개 → {OUT_DIR}")
    print(f"  이미지 총 {total_imgs}개 → {IMG_DIR}")
    print(f"  로컬 링크 제거 총 {total_links}개 (외부 http 링크는 유지)")
    if failures:
        print(f"  실패: {failures}")
        return 1

    # 검증: 산출물에 로컬 링크·mermaid 펜스가 남아 있으면 안 된다.
    leftover = []
    for md in OUT_DIR.glob("*.md"):
        t = md.read_text(encoding="utf-8")
        if LOCAL_LINK.search(t) or "```mermaid" in t:
            leftover.append(md.name)
    if leftover:
        print(f"  [경고] 변환 잔여물 존재: {leftover}")
        return 1
    print("  검증: 산출물에 로컬 링크 0건, mermaid 펜스 0건")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
