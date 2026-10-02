#!/usr/bin/env bash
# Build endless-sky.3dsx with the official devkitPro Docker image, so that
# nothing but Docker has to be installed. The result is in build/.
#
#   tools/docker-build.sh            # release build
#   tools/docker-build.sh Debug      # debug build
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_TYPE="${1:-Release}"
IMAGE="${DEVKITARM_IMAGE:-devkitpro/devkitarm:latest}"

if [ ! -f "$ROOT/upstream/source/main.cpp" ]; then
	git -C "$ROOT" submodule update --init --depth 1 upstream
fi

docker run --rm -u "$(id -u):$(id -g)" -v "$ROOT:/project" -w /project "$IMAGE" bash -c "
	set -e
	cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=/opt/devkitpro/cmake/3DS.cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE
	cmake --build build -j\$(nproc)
"
echo "Built: $ROOT/build/endless-sky.3dsx"
