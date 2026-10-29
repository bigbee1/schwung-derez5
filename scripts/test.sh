#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
mkdir -p build
cc -O2 -Wall -Wextra tests/derez5_smoke_test.c -Isrc/dsp -lm -o build/derez5_smoke_test
./build/derez5_smoke_test
