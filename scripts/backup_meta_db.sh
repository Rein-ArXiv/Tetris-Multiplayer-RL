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
echo "[backup_meta_db] Done: $OUT_DIR/$BASE.tar.gz (integrity_check=ok)"
