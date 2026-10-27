#!/usr/bin/env bash
# Build the panCollapse runtime Docker image.
#
# panCollapse links against an existing vg installation's shared libraries and the
# surrounding toolchain. This script assembles a self-contained build context by
# resolving the freshly built binary's shared-library closure with ldd, then builds
# an image that launches the binary through the bundled dynamic loader (see the
# Dockerfile). Nothing about the host vg build is fetched or recompiled.
#
# Usage: scripts/tooling/build-docker-image.sh [--tag <image:tag>] [--binary <path>]
#
# Defaults: tag josephlalli/pancollapse:v0.10.0, binary build/src/panCollapse.
# The configured build tree must contain the direct-count CTest fixture; the script
# creates it through CTest when needed, then exercises every v0.10 count sink in the image.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
TAG="josephlalli/pancollapse:v0.10.0"
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
BINARY="$(realpath "$BINARY")"
if [ ! -f "${REPO_ROOT}/Dockerfile" ]; then
  echo "error: Dockerfile not found at ${REPO_ROOT}/Dockerfile" >&2
  exit 1
fi

CONTEXT="$(mktemp -d "${TMPDIR:-/tmp}/pancollapse-docker.XXXXXX")"
COUNT_SMOKE_ROOT=""
cleanup() {
  rm -rf "$CONTEXT"
  if [ -n "$COUNT_SMOKE_ROOT" ]; then
    rm -rf "$COUNT_SMOKE_ROOT"
  fi
}
trap cleanup EXIT
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

if ldd "$BINARY" | grep -Eqi 'libpython[0-9.]*\.so'; then
  echo "error: release binary closure unexpectedly depends on libpython" >&2
  exit 1
fi

INTERP="$(patchelf --print-interpreter "$BINARY" 2>/dev/null || true)"
if [ -n "$INTERP" ] && [ -e "$INTERP" ]; then
  cp -Lf "$INTERP" "$CONTEXT/lib/$(basename "$INTERP")"
fi
if ! ls "$CONTEXT/lib/" | grep -q '^ld-linux'; then
  echo "error: dynamic loader (ld-linux) was not staged" >&2
  exit 1
fi

for required in libarrow.so libparquet.so libzstd.so; do
  if ! find "$CONTEXT/lib" -maxdepth 1 -type f -name "${required}*" -print -quit | grep -q .; then
    echo "error: release closure is missing required ${required} runtime library" >&2
    exit 1
  fi
done
if find "$CONTEXT/lib" -maxdepth 1 -type f -name 'libpython*.so*' -print -quit | grep -q .; then
  echo "error: release closure must not package libpython" >&2
  exit 1
fi

LOADER="$(find "$CONTEXT/lib" -maxdepth 1 -type f -name 'ld-linux*.so*' -printf '%f\n' | head -n 1)"
if [ -z "$LOADER" ]; then
  echo "error: bundled dynamic loader is not discoverable" >&2
  exit 1
fi
STAGED_LDD="$("$CONTEXT/lib/$LOADER" --library-path "$CONTEXT/lib" --list "$CONTEXT/bin/panCollapse")"
if printf '%s\n' "$STAGED_LDD" | grep -q 'not found'; then
  echo "error: staged runtime closure has unresolved libraries" >&2
  exit 1
fi
for required in libarrow.so libparquet.so libzstd.so; do
  if ! printf '%s\n' "$STAGED_LDD" | grep -q "$required"; then
    echo "error: staged runtime closure does not resolve ${required}" >&2
    exit 1
  fi
done
if printf '%s\n' "$STAGED_LDD" | grep -Eqi 'libpython[0-9.]*\.so'; then
  echo "error: staged runtime closure resolves libpython" >&2
  exit 1
fi

echo "staged $(ls "$CONTEXT/lib" | wc -l) libraries ($(du -sh "$CONTEXT" | cut -f1))"
echo "building image: $TAG"
docker build -f "${REPO_ROOT}/Dockerfile" -t "$TAG" "$CONTEXT"

BUILD_ROOT="$(dirname "$(dirname "$BINARY")")"
COUNT_WORK="${BUILD_ROOT}/tests/vg/ex50_tier_work"
COUNT_SOURCE="${REPO_ROOT}/tests/vg/fixtures/ex50_tier_smoke"
if [ ! -s "${COUNT_WORK}/reads.gamp" ] ||
   [ ! -s "${COUNT_WORK}/graph.xg" ] ||
   [ ! -s "${COUNT_WORK}/count_bundle/MANIFEST.json" ]; then
  if [ ! -f "${BUILD_ROOT}/CTestTestfile.cmake" ]; then
    echo "error: no configured CTest tree found at ${BUILD_ROOT}" >&2
    exit 1
  fi
  echo "creating the native-count release fixture through CTest"
  ctest --test-dir "$BUILD_ROOT" --output-on-failure -R '^direct_count_bundle_fixture$'
fi

for required in \
  "${COUNT_WORK}/reads.gamp" \
  "${COUNT_WORK}/graph.xg" \
  "${COUNT_WORK}/count_bundle/MANIFEST.json" \
  "${COUNT_SOURCE}/count_whitelist.txt" \
  "${COUNT_SOURCE}/count_t2g.tsv" \
  "${COUNT_SOURCE}/count_body_t2g.tsv"; do
  if [ ! -s "$required" ]; then
    echo "error: native-count release fixture is missing: ${required}" >&2
    exit 1
  fi
done

COUNT_SMOKE_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/pancollapse-count-smoke.XXXXXX")"
chmod 0777 "$COUNT_SMOKE_ROOT"
echo "running native-count all-format smoke in image: $TAG"
docker run --rm \
  --user "$(id -u):$(id -g)" \
  -v "${COUNT_WORK}:/fixture:ro" \
  -v "${COUNT_SOURCE}:/source-fixture:ro" \
  -v "${COUNT_SMOKE_ROOT}:/smoke" \
  "$TAG" count \
  --gamp /fixture/reads.gamp \
  --xg /fixture/graph.xg \
  --count-bundle /fixture/count_bundle \
  --barcode-whitelist /source-fixture/count_whitelist.txt \
  --t2g /source-fixture/count_t2g.tsv \
  --body-t2g /source-fixture/count_body_t2g.tsv \
  --cr7 --pansc-strict-v1 \
  --threads 2 --count-memory-budget 1MiB \
  --10x-mex --rad-out \
  --read-assignments-out diagnostics/read_assignments.parquet \
  --out-dir /smoke/counts

while IFS= read -r required; do
  if [ ! -s "${COUNT_SMOKE_ROOT}/counts/${required}" ]; then
    echo "error: container count smoke did not create ${required}" >&2
    exit 1
  fi
done <<'EOF'
parquet/barcodes.parquet
parquet/features.parquet
parquet/counts.parquet
parquet/molecules.parquet
manifest.json
summary.tsv
map.rad
diagnostics/read_assignments.parquet
mex/cr7-v1/raw_feature_bc_matrix/barcodes.tsv.gz
mex/cr7-v1/raw_feature_bc_matrix/features.tsv.gz
mex/cr7-v1/raw_feature_bc_matrix/matrix.mtx.gz
mex/pansc-strict-v1/raw_feature_bc_matrix/barcodes.tsv.gz
mex/pansc-strict-v1/raw_feature_bc_matrix/features.tsv.gz
mex/pansc-strict-v1/raw_feature_bc_matrix/matrix.mtx.gz
EOF

if ! grep -q '"cr7-v1"' "${COUNT_SMOKE_ROOT}/counts/manifest.json" ||
   ! grep -q '"pansc-strict-v1"' "${COUNT_SMOKE_ROOT}/counts/manifest.json"; then
  echo "error: container count smoke manifest does not contain both frozen profiles" >&2
  exit 1
fi
if docker run --rm --entrypoint /bin/sh "$TAG" -c \
   'command -v python >/dev/null 2>&1 || command -v python3 >/dev/null 2>&1'; then
  echo "error: release image unexpectedly contains a Python executable" >&2
  exit 1
fi
echo "done: $TAG"
