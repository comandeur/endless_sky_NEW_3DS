#!/usr/bin/env bash
# Build the "release" version of Endless Sky for the 3DS: a single CIA that
# contains the game data, so that installing it with FBI is all a player has to
# do. Also builds a .3dsx with the same embedded data.
#
#   tools/make-release.sh
#
# The game data is converted first if assets-out/endless-sky/ does not exist
# (15 to 30 minutes). The results are in dist/.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
IMAGE="endless-sky-3ds-tools"
DATA="assets-out/endless-sky"

if [ ! -f "$ROOT/upstream/source/main.cpp" ]; then
	git -C "$ROOT" submodule update --init --depth 1 upstream
fi

if [ ! -f "$ROOT/$DATA/images.pak" ]; then
	echo "Converting the game data..."
	"$ROOT/tools/convert-assets.sh"
fi

docker build -q -t "$IMAGE" "$ROOT/tools/docker" > /dev/null
docker run --rm -u "$(id -u):$(id -g)" -v "$ROOT:/project" -w /project "$IMAGE" bash -c "
	set -e
	cmake -S . -B build-release -DCMAKE_TOOLCHAIN_FILE=/opt/devkitpro/cmake/3DS.cmake \
		-DCMAKE_BUILD_TYPE=Release -DES_ROMFS=/project/$DATA
	cmake --build build-release -j\$(nproc)
"

mkdir -p "$ROOT/dist"
cp "$ROOT/build-release/endless-sky.cia" "$ROOT/dist/endless-sky.cia"
cp "$ROOT/build-release/endless-sky.3dsx" "$ROOT/dist/endless-sky.3dsx"
echo "Release files:"
ls -lh "$ROOT/dist"
