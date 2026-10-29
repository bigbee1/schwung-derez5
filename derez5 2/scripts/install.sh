#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
[ -f "$ROOT/dist/derez5/derez5.so" ] || { echo 'Run ./scripts/build.sh first.'; exit 1; }
ssh ableton@move.local 'mkdir -p /data/UserData/move-anything/modules/audio_fx/derez5'
scp "$ROOT"/dist/derez5/* ableton@move.local:/data/UserData/move-anything/modules/audio_fx/derez5/
ssh ableton@move.local 'chmod -R a+rw /data/UserData/move-anything/modules/audio_fx/derez5'
echo 'Installed DeRez5. Restart Schwung/Move to load the module.'
