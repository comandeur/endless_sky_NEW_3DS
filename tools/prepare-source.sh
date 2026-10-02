#!/usr/bin/env bash
# Assemble the 3DS build tree from the upstream Endless Sky sources:
#   1. copy upstream/source
#   2. apply port/patches/*.patch (small modifications of upstream files)
#   3. copy port/overlay/source over it (full-file replacements)
#   4. add the 3DS platform layer from port/src3ds
# The result is synchronised into build/src by content, so that files whose
# content did not change keep their timestamps and incremental builds stay fast.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="${1:-$ROOT/build/src}"
STAGE="$(mktemp -d)"
trap 'rm -rf "$STAGE"' EXIT

if [ ! -f "$ROOT/upstream/source/main.cpp" ]; then
	echo "error: upstream sources missing; run 'git submodule update --init'" >&2
	exit 1
fi

cp -r "$ROOT/upstream/source" "$STAGE/source"

# Upstream files that the 3DS port does not build at all (desktop-only code).
rm -rf "$STAGE/source/windows"
rm -f "$STAGE/source/opengl.cpp" "$STAGE/source/shader/Shader.cpp"

shopt -s nullglob
for patch in "$ROOT"/port/patches/*.patch; do
	if ! patch -s -p1 -d "$STAGE" --no-backup-if-mismatch < "$patch"; then
		echo "error: failed to apply $(basename "$patch")" >&2
		exit 1
	fi
done

cp -r "$ROOT/port/overlay/source/." "$STAGE/source/"
mkdir -p "$STAGE/source/ctr"
cp -r "$ROOT/port/src3ds/." "$STAGE/source/ctr/"

# Synchronise without touching unchanged files.
mkdir -p "$OUT"
(cd "$STAGE/source" && find . -type f) | while read -r f; do
	if ! cmp -s "$STAGE/source/$f" "$OUT/$f"; then
		mkdir -p "$(dirname "$OUT/$f")"
		cp "$STAGE/source/$f" "$OUT/$f"
	fi
done
(cd "$OUT" && find . -type f) | while read -r f; do
	[ -e "$STAGE/source/$f" ] || rm -f "$OUT/$f"
done
echo "Prepared sources in $OUT"
