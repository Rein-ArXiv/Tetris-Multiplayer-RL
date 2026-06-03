#!/usr/bin/env bash
# SQLite meta DB 백업.
#
# 사용법:
#   ./scripts/backup_meta_db.sh /srv/tetris/db/tetris.db /srv/tetris/backups
#
# sqlite3 CLI 가 있으면 온라인 백업 API(.backup)를 사용한다. 없으면 DB/WAL/SHM
# 파일을 같이 보관한다.
set -euo pipefail

DB="${1:-/srv/tetris/db/tetris.db}"
OUT_DIR="${2:-/srv/tetris/backups}"
TS="$(date -u +%Y%m%dT%H%M%SZ)"
BASE="tetris-$TS"

if [ ! -f "$DB" ]; then
    echo "[backup_meta_db] DB not found: $DB" >&2
    exit 1
fi

mkdir -p "$OUT_DIR"

if command -v sqlite3 >/dev/null 2>&1; then
    OUT_DB="$OUT_DIR/$BASE.db"
    sqlite3 "$DB" ".backup '$OUT_DB'"
    tar -czf "$OUT_DIR/$BASE.tar.gz" -C "$OUT_DIR" "$BASE.db"
    echo "[backup_meta_db] Done: $OUT_DIR/$BASE.tar.gz"
else
    SNAP="$OUT_DIR/$BASE.files"
    mkdir -p "$SNAP"
    cp -p "$DB" "$SNAP/"
    if [ -f "$DB-wal" ]; then cp -p "$DB-wal" "$SNAP/"; fi
    if [ -f "$DB-shm" ]; then cp -p "$DB-shm" "$SNAP/"; fi
    tar -czf "$OUT_DIR/$BASE.tar.gz" -C "$OUT_DIR" "$BASE.files"
    echo "[backup_meta_db] Done: $OUT_DIR/$BASE.tar.gz"
    echo "[backup_meta_db] sqlite3 CLI not found; DB/WAL/SHM snapshot was archived."
fi
