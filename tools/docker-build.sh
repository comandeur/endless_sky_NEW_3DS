#!/usr/bin/env bash
# Build Endless Sky for the 3DS in Docker, so that nothing but Docker has to be
# installed. The results are in build/:
#   build/endless-sky.3dsx   for the Homebrew Launcher
#   build/endless-sky.cia    to install with FBI (recommended: more memory)
#
#   tools/docker-build.sh            # release build
#   tools/docker-build.sh Debug      # debug build
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_TYPE="${1:-Release}"
IMAGE="endless-sky-3ds-tools"

if [ ! -f "$ROOT/upstream/source/main.cpp" ]; then
	git -C "$ROOT" submodule update --init --depth 1 upstream
fi

docker build -q -t "$IMAGE" "$ROOT/tools/docker" > /dev/null
docker run --rm -u "$(id -u):$(id -g)" -v "$ROOT:/project" -w /project "$IMAGE" bash -c "
	set -e
	cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=/opt/devkitpro/cmake/3DS.cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE
	cmake --build build -j\$(nproc)
"
echo "Built: $ROOT/build/endless-sky.3dsx"
[ -f "$ROOT/build/endless-sky.cia" ] && echo "Built: $ROOT/build/endless-sky.cia"
