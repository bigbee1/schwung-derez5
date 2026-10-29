#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
IMAGE_NAME="derez5-schwung-builder"
if [ -z "${CROSS_PREFIX:-}" ] && [ ! -f /.dockerenv ] && command -v docker >/dev/null 2>&1; then
  docker image inspect "$IMAGE_NAME" >/dev/null 2>&1 || docker build -t "$IMAGE_NAME" -f scripts/Dockerfile .
  docker run --rm -v "$ROOT:/build" -u "$(id -u):$(id -g)" -w /build "$IMAGE_NAME" ./scripts/build.sh
  exit 0
fi
CROSS_PREFIX="${CROSS_PREFIX:-aarch64-linux-gnu-}"
mkdir -p build dist/derez5
"${CROSS_PREFIX}gcc" -Ofast -shared -fPIC -fvisibility=hidden -march=armv8-a -mtune=cortex-a72 -fomit-frame-pointer -fno-stack-protector -DNDEBUG src/dsp/derez5.c -o build/derez5.so -Isrc/dsp -lm
cp src/module.json dist/derez5/module.json
cp src/help.json dist/derez5/help.json
cp build/derez5.so dist/derez5/derez5.so
chmod 755 dist/derez5/derez5.so
(cd dist && tar -czf derez5-module.tar.gz derez5/)
echo "Built dist/derez5/ and dist/derez5-module.tar.gz"
