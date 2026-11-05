#!/usr/bin/env bash
# Repair only files whose modification time is ahead of the current system clock.
# Generated build outputs are included: future-dated objects/stamps can otherwise
# make Make incorrectly consider stale outputs up to date.
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd -- "${SCRIPT_DIR}/.." && pwd)"

fixed=0
while IFS= read -r -d '' file; do
    touch -c -- "$file"
    printf 'Normalized timestamp: %s\n' "${file#"$ROOT_DIR"/}"
    fixed=$((fixed + 1))
done < <(find "$ROOT_DIR" -type f ! -path "$ROOT_DIR/.git/*" -newermt 'now' -print0)

if (( fixed == 0 )); then
    echo "Timestamps: no future-dated files found."
else
    printf 'Timestamps: normalized %d future-dated file(s).\n' "$fixed"
fi
