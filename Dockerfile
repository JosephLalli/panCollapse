# panCollapse runtime image.
#
# panCollapse is a standalone consumer of an existing vg installation: it links
# against vg's shared libraries (libvgio, libhandlegraph, libhts, ...) plus the
# surrounding toolchain (libstdc++, protobuf, abseil, glibc). Rather than rebuild
# that toolchain inside the image, this Dockerfile packages the prebuilt binary
# together with the exact shared-library closure it was built and tested against,
# and launches it through the bundled dynamic loader. The binary therefore does
# not depend on the base image's C/C++ runtime.
#
# The build context (./bin and ./lib) is assembled by scripts/build-docker-image.sh,
# which resolves the closure with ldd from the freshly built binary. Do not run
# `docker build` against the repository root directly; use that script.

FROM debian:bookworm-slim

# procps provides /bin/ps, which the Nextflow docker executor invokes inside the
# container to collect per-task resource metrics. It is unrelated to the bundled
# binary (which runs through its own loader below); without it Nextflow aborts the
# task with "Command 'ps' ... cannot be found".
RUN apt-get update \
 && apt-get install -y --no-install-recommends procps \
 && rm -rf /var/lib/apt/lists/*

LABEL org.opencontainers.image.title="panCollapse" \
      org.opencontainers.image.version="0.4.5" \
      org.opencontainers.image.description="Convert vg mpmap GAMP multipath alignments into alevin-fry RAD records (PathTally graph-native transcript-compatibility scoring)." \
      org.opencontainers.image.source="https://github.com/JosephLalli/panCollapse" \
      org.opencontainers.image.licenses="Apache-2.0"

# Bundled shared-library closure and the program interpreter.
COPY lib/ /opt/pancollapse/lib/
# Prebuilt, stripped panCollapse binary.
COPY bin/panCollapse /opt/pancollapse/bin/panCollapse

# Launch wrapper: resolve every dependency from the bundled closure via the
# bundled loader, independent of the base image's libc. Verify at build time.
RUN printf '%s\n' \
      '#!/bin/sh' \
      'exec /opt/pancollapse/lib/ld-linux-x86-64.so.2 \' \
      '  --library-path /opt/pancollapse/lib \' \
      '  /opt/pancollapse/bin/panCollapse "$@"' \
      > /usr/local/bin/panCollapse \
 && chmod +x /usr/local/bin/panCollapse \
 && /usr/local/bin/panCollapse --version

# Run as an unprivileged user; convert reads from stdin or a mounted volume.
RUN useradd --create-home --uid 1000 pancollapse
USER pancollapse
WORKDIR /work

ENTRYPOINT ["/usr/local/bin/panCollapse"]
CMD ["--help"]
