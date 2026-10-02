#!/usr/bin/env bash
# Convert the Endless Sky game data (images, sounds, data files) for the 3DS,
# in Docker. The result goes to assets-out/endless-sky/, which must be copied
# to sdmc:/3ds/endless-sky/ on the SD card.
#
#   tools/convert-assets.sh [extra convert.py options]
#
# A full conversion takes 15 to 30 minutes, depending on the computer.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
IMAGE="endless-sky-3ds-tools"

if [ ! -f "$ROOT/upstream/source/main.cpp" ]; then
	git -C "$ROOT" submodule update --init --depth 1 upstream
fi

docker build -q -t "$IMAGE" "$ROOT/tools/docker" > /dev/null
docker run --rm -u "$(id -u):$(id -g)" -v "$ROOT:/project" -w /project "$IMAGE" \
	python3 tools/assets/convert.py --tex3ds /opt/devkitpro/tools/bin/tex3ds "$@"
