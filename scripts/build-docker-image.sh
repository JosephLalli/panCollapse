#!/usr/bin/env bash
# Build the panCollapse runtime Docker image.
#
# panCollapse links against an existing vg installation's shared libraries and the
# surrounding toolchain. This script assembles a self-contained build context by
# resolving the freshly built binary's shared-library closure with ldd, then builds
# an image that launches the binary through the bundled dynamic loader (see the
# Dockerfile). Nothing about the host vg build is fetched or recompiled.
#
# Usage: scripts/build-docker-image.sh [--tag <image:tag>] [--binary <path>]
#
# Defaults: tag josephlalli/pancollapse:v0.4, binary build/src/panCollapse.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TAG="josephlalli/pancollapse:v0.4"
BINARY="${REPO_ROOT}/build/src/panCollapse"

while [ "$#" -gt 0 ]; do
  case "$1" in
    --tag) TAG="$2"; shift 2 ;;
    --binary) BINARY="$2"; shift 2 ;;
    -h|--help)
      grep '^#' "$0" | sed 's/^# \{0,1\}//'
      exit 0 ;;
    *) echo "unknown argument: $1" >&2; exit 2 ;;
  esac
done

if [ ! -x "$BINARY" ]; then
  echo "error: binary not found or not executable: $BINARY" >&2
  echo "build it first, e.g.:" >&2
  echo "  cmake --build build -j 24" >&2
  exit 1
fi
if [ ! -f "${REPO_ROOT}/Dockerfile" ]; then
  echo "error: Dockerfile not found at ${REPO_ROOT}/Dockerfile" >&2
  exit 1
fi

CONTEXT="$(mktemp -d "${TMPDIR:-/tmp}/pancollapse-docker.XXXXXX")"
trap 'rm -rf "$CONTEXT"' EXIT
mkdir -p "$CONTEXT/lib" "$CONTEXT/bin"

echo "staging binary -> $CONTEXT/bin/panCollapse"
cp "$BINARY" "$CONTEXT/bin/panCollapse"
strip "$CONTEXT/bin/panCollapse" 2>/dev/null || true

echo "resolving shared-library closure with ldd"
# Copy every resolved dependency (dereferencing symlinks) plus the program
# interpreter into the bundled lib directory.
ldd "$BINARY" | awk '/=> \//{print $3} /ld-linux/ && $1 ~ /^\//{print $1}' \
  | sort -u | while read -r lib; do
  [ -e "$lib" ] && cp -Lf "$lib" "$CONTEXT/lib/"
done

INTERP="$(patchelf --print-interpreter "$BINARY" 2>/dev/null || true)"
if [ -n "$INTERP" ] && [ -e "$INTERP" ]; then
  cp -Lf "$INTERP" "$CONTEXT/lib/$(basename "$INTERP")"
fi
if ! ls "$CONTEXT/lib/" | grep -q '^ld-linux'; then
  echo "error: dynamic loader (ld-linux) was not staged" >&2
  exit 1
fi

echo "staged $(ls "$CONTEXT/lib" | wc -l) libraries ($(du -sh "$CONTEXT" | cut -f1))"
echo "building image: $TAG"
docker build -f "${REPO_ROOT}/Dockerfile" -t "$TAG" "$CONTEXT"

echo "done: $TAG"
