#!/usr/bin/env bash
set -euo pipefail

./build_emscripten_deploy_dir.sh

ssh whirred.io 'mkdir -p dist/hyperverse'
rsync -av --delete dist/hyperverse/ whirred.io:dist/hyperverse/

ssh whirred.io '
  set -euo pipefail
  cd dist/hyperverse
  docker build -t hyperverse-staging .
  cd /www/docker-stuff
  docker compose up -d
'
