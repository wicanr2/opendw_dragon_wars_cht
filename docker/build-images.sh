#!/usr/bin/env bash
# 重建本專案 docker 映像(dev-setup 在新機第一步)。
set -euo pipefail
HERE="$(cd "$(dirname "$0")/.." && pwd)"; cd "$HERE"
docker build -t dwsdl -f docker/Dockerfile.dwsdl .
docker build -t dwpil -f docker/Dockerfile.dwpil .
echo "✅ dwsdl / dwpil 已建。其餘(mingw / appimagetool / wine)由各打包腳本按需自裝。"
