#!/usr/bin/env bash
# SQLite meta DB 백업.
#
# 사용법:
#   ./scripts/backup_meta_db.sh /srv/tetris/db/tetris.db /srv/tetris/backups
#
# 실행 중인 WAL DB도 일관되게 복사하도록 sqlite3 온라인 백업 API(.backup)를
# 사용하고, 완성된 스냅샷의 무결성을 확인한다.
set -euo pipefail

DB="${1:-/srv/tetris/db/tetris.db}"
OUT_DIR="${2:-/srv/tetris/backups}"
TS="$(date -u +%Y%m%dT%H%M%SZ)"
# 같은 초에 수동 실행과 timer가 겹쳐도 snapshot 이름이 충돌하지 않게 PID를 붙인다.
BASE="tetris-$TS-$$"

if [ ! -f "$DB" ]; then
    echo "[backup_meta_db] DB not found: $DB" >&2
    exit 1
fi

case "$DB" in
    /*) DB_ABS="$DB" ;;
    *)  DB_ABS="$PWD/$DB" ;;
esac

mkdir -p "$OUT_DIR"

if ! command -v sqlite3 >/dev/null 2>&1; then
    echo "[backup_meta_db] sqlite3 CLI is required for a consistent online backup." >&2
    echo "[backup_meta_db] Install sqlite3, or stop tetris_meta before making an offline copy." >&2
    exit 1
fi

OUT_DB="$OUT_DIR/$BASE.db"
(
    cd "$OUT_DIR"
    # BASE는 스크립트가 만든 안전한 이름이다. 호출자가 준 경로는 sqlite
    # dot-command 문자열에 넣지 않고 DB_ABS argv와 cd 대상으로만 사용한다.
    sqlite3 "$DB_ABS" ".backup '$BASE.db'"
)

INTEGRITY="$(sqlite3 "$OUT_DB" 'PRAGMA integrity_check;')"
if [ "$INTEGRITY" != "ok" ]; then
    echo "[backup_meta_db] integrity_check failed for $OUT_DB: $INTEGRITY" >&2
    exit 1
fi

tar -czf "$OUT_DIR/$BASE.tar.gz" -C "$OUT_DIR" "$BASE.db"

# tar.gz 에 스냅샷이 들어갔으니 중간 산출물 .db 는 지운다. 남겨두면 백업마다
# 압축본 + 비압축 원본이 이중으로 쌓여 디스크가 두 배로 소모된다. 삭제 실패는
# 경고만 남긴다 — 백업 본체는 이미 성공했으므로 여기서 스크립트를 죽이지 않는다.
rm -f -- "$OUT_DB" || echo "[backup_meta_db] warning: could not remove $OUT_DB" >&2

# ── 보존 정책 ─────────────────────────────────────────────────────────────────
# 최근 백업 KEEP개(기본 14, 환경변수 KEEP 로 조정)만 남기고 오래된 tar.gz 를
# 삭제한다. 정리는 부가 작업이므로 어떤 실패도 백업 성공을 뒤집어선 안 된다
# (set -e 아래에서 if ! ... 로 감싸 실패를 흡수한다).
KEEP="${KEEP:-14}"
case "$KEEP" in
    ''|*[!0-9]*)
        # 숫자가 아니면 산술 확장에서 스크립트가 죽으므로 기본값으로 대체.
        echo "[backup_meta_db] warning: invalid KEEP='$KEEP'; using 14" >&2
        KEEP=14
        ;;
esac
prune_old_backups() {
    # 파일명은 이 스크립트가 만든 UTC 타임스탬프 형식뿐이라 공백/개행이 없고,
    # ls -1t(수정시각 내림차순) 기준 KEEP+1 번째부터가 "오래된" 백업이다.
    ls -1t "$OUT_DIR"/tetris-*.tar.gz 2>/dev/null | tail -n +"$((KEEP + 1))" |
    while IFS= read -r old; do
        rm -f -- "$old" || echo "[backup_meta_db] warning: failed to prune $old" >&2
    done
}
if ! prune_old_backups; then
    echo "[backup_meta_db] warning: retention cleanup failed (backup itself is OK)" >&2
fi

echo "[backup_meta_db] Done: $OUT_DIR/$BASE.tar.gz (integrity_check=ok)"
