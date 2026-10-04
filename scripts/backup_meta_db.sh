#!/usr/bin/env bash
# Linux archive/retention wrapper for the portable SQLite snapshot helper.
# Requires Python sqlite3, GNU tar/sort, hard links and flock. Output is private.
set -euo pipefail
umask 077

DB="${1:-/srv/tetris/db/tetris.db}"
OUT_DIR="${2:-/srv/tetris/backups}"
KEEP="${KEEP:-14}"
if [[ ! "$KEEP" =~ ^[0-9]+$ ]]; then
    echo '[backup_meta_db] KEEP must be a positive decimal integer.' >&2
    exit 2
fi
while [[ ${#KEEP} -gt 1 && "$KEEP" == 0* ]]; do KEEP="${KEEP#0}"; done
if [[ "$KEEP" == 0 || ${#KEEP} -gt 9 ]]; then
    echo '[backup_meta_db] KEEP must be in 1..999999999.' >&2
    exit 2
fi
if [[ ! -f "$DB" ]]; then
    echo "[backup_meta_db] DB not found: $DB" >&2
    exit 1
fi
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$OUT_DIR"
# Serialize publication and retention in this output directory. SQLite itself
# coordinates an online source; this lock does not pause application writers.
exec {LOCK_FD}>"$OUT_DIR/.tetris-backup.lock"
if ! flock -n "$LOCK_FD"; then
    echo '[backup_meta_db] Another archive operation owns this directory.' >&2
    exit 1
fi
BASE="tetris-$(date -u +%Y%m%dT%H%M%SZ)-$$"
STAGE="$(mktemp -d "$OUT_DIR/.tetris-stage.XXXXXX")"
cleanup() {
    rm -f -- "$STAGE/$BASE.db" "$STAGE/archive.tar.gz" "$STAGE/retention.list" || \
        echo '[backup_meta_db] warning: staging cleanup failed.' >&2
    rmdir -- "$STAGE" 2>/dev/null || \
        echo '[backup_meta_db] warning: private staging directory remains.' >&2
}
trap cleanup EXIT
python3 "$SCRIPT_DIR/backup_meta_db.py" "$DB" "$STAGE/$BASE.db"
tar -czf "$STAGE/archive.tar.gz" -C "$STAGE" "$BASE.db"
# Publish only a complete archive. A late sync failure may leave this file;
# no retention runs unless publication and its immediate-parent sync succeed.
python3 - "$STAGE/archive.tar.gz" "$OUT_DIR/$BASE.tar.gz" <<'PY'
import os
from pathlib import Path
import sys
source, target = map(Path, sys.argv[1:])
with source.open('r+b') as stream:
    os.fsync(stream.fileno())
os.link(source, target)
fd = os.open(target.parent, os.O_RDONLY | os.O_DIRECTORY)
try:
    os.fsync(fd)
finally:
    os.close(fd)
PY

prune_old_backups() {
    local out_dir="$1" base="$2" keep="$3"
    local -a candidates=() sorted=()
    local path name
    [[ -f "$out_dir/$base.tar.gz" && ! -L "$out_dir/$base.tar.gz" ]] || {
        echo '[backup_meta_db] warning: current archive missing; retention skipped.' >&2
        return 0
    }
    for path in "$out_dir"/tetris-*.tar.gz; do
        [[ -f "$path" && ! -L "$path" ]] || continue
        name="${path##*/}"
        [[ "$name" =~ ^tetris-[0-9]{8}T[0-9]{6}Z-[0-9]+[.]tar[.]gz$ ]] || continue
        [[ "$name" == "$base.tar.gz" ]] || candidates+=("$name")
    done
    ((${#candidates[@]})) || return 0
    # Check the whole sort before deleting anything; preserve path boundaries.
    if ! printf '%s\0' "${candidates[@]}" | LC_ALL=C sort -zr > "$STAGE/retention.list"; then
        echo '[backup_meta_db] warning: retention sort failed; nothing pruned.' >&2
        return 0
    fi
    while IFS= read -r -d '' name; do sorted+=("$name"); done < "$STAGE/retention.list"
    local i
    # Keep the new completed archive plus newest named UTC snapshots.
    # Same-second snapshots use the filename as a deterministic tie breaker.
    for ((i=keep-1; i<${#sorted[@]}; ++i)); do
        rm -f -- "$out_dir/${sorted[i]}" || \
            echo '[backup_meta_db] warning: old archive could not be pruned.' >&2
    done
    return 0
}
prune_old_backups "$OUT_DIR" "$BASE" "$KEEP"
printf '[backup_meta_db] Done: %s/%s.tar.gz (checked snapshot)\n' "$OUT_DIR" "$BASE"
