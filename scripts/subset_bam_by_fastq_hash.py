#!/usr/bin/env python3
"""Create an exact, assignment-blind BAM/FASTQ read subset.

The canonical read FASTQ defines the sampling universe.  The selected reads are
the ``N`` smallest SHA-256 values of ``seed + NUL + read_name``.  Only after the
selection is frozen is the BAM scanned, so BAM assignment fields cannot affect
sample inclusion.
"""

from __future__ import annotations

import argparse
import csv
import gzip
import hashlib
import heapq
import json
import shutil
import sys
import time
from collections import Counter, defaultdict
from datetime import datetime, timezone
from pathlib import Path
from typing import BinaryIO, Iterator, TextIO

import pysam


SCRIPT_VERSION = "1.0.0"
SCHEMA = "pansc-assignment-blind-read-subset-v1"


def now() -> str:
    return datetime.now(timezone.utc).isoformat()


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(8 * 1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def open_text(path: Path) -> TextIO:
    return gzip.open(path, "rt") if path.suffix == ".gz" else path.open()


def deterministic_gzip(path: Path) -> TextIO:
    raw: BinaryIO = path.open("wb")
    compressed = gzip.GzipFile(filename="", mode="wb", fileobj=raw, mtime=0)
    return __import__("io").TextIOWrapper(compressed, newline="")


def fastq_records(path: Path) -> Iterator[tuple[str, tuple[str, str, str, str]]]:
    with open_text(path) as handle:
        record_number = 0
        while True:
            header = handle.readline()
            if not header:
                return
            sequence = handle.readline()
            plus = handle.readline()
            quality = handle.readline()
            record_number += 1
            if not sequence or not plus or not quality:
                raise RuntimeError(f"{path}: truncated FASTQ record {record_number}")
            if not header.startswith("@") or not plus.startswith("+"):
                raise RuntimeError(f"{path}: malformed FASTQ record {record_number}")
            read_name = header[1:].split()[0]
            if not read_name:
                raise RuntimeError(f"{path}: empty read name at record {record_number}")
            yield read_name, (header, sequence, plus, quality)


def hash_read_name(seed: str, read_name: str) -> int:
    digest = hashlib.sha256()
    digest.update(seed.encode("utf-8"))
    digest.update(b"\0")
    digest.update(read_name.encode("utf-8"))
    return int.from_bytes(digest.digest(), "big")


def checksum_rows(path: Path, root: Path) -> dict[Path, str]:
    rows: dict[Path, str] = {}
    with path.open() as handle:
        for line_number, raw in enumerate(handle, start=1):
            line = raw.rstrip("\n")
            if "  " not in line:
                raise RuntimeError(f"{path}:{line_number}: malformed checksum row")
            digest, relative = line.split("  ", 1)
            if len(digest) != 64 or any(char not in "0123456789abcdef" for char in digest):
                raise RuntimeError(f"{path}:{line_number}: malformed SHA-256")
            relative_path = Path(relative)
            if relative_path.is_absolute() or ".." in relative_path.parts:
                raise RuntimeError(f"{path}:{line_number}: unsafe checksum path")
            resolved = (root / relative_path).resolve()
            if resolved in rows:
                raise RuntimeError(f"{path}:{line_number}: duplicate checksum path")
            rows[resolved] = digest
    if not rows:
        raise RuntimeError(f"{path}: empty checksum manifest")
    return rows


def authoritative_input_record(path: Path, root: Path, checksums: Path) -> dict[str, object]:
    rows = checksum_rows(checksums, root)
    resolved = path.resolve()
    if resolved not in rows:
        raise RuntimeError(f"{checksums}: no checksum row for {resolved}")
    if not resolved.is_file():
        raise RuntimeError(f"missing input: {resolved}")
    return {
        "path": str(resolved),
        "bytes": resolved.stat().st_size,
        "sha256": rows[resolved],
        "checksum_authority": {
            "path": str(checksums.resolve()),
            "sha256": sha256(checksums),
        },
    }


def load_truth(path: Path) -> dict[str, tuple[str, str, str]]:
    truth: dict[str, tuple[str, str, str]] = {}
    with path.open() as handle:
        reader = csv.DictReader(handle, delimiter="\t")
        required = {"template_id", "stable_count_gene", "mapping_status", "scope_chromosome"}
        if reader.fieldnames is None or not required <= set(reader.fieldnames):
            raise RuntimeError(f"{path}: missing truth columns {sorted(required)}")
        for row in reader:
            template_id = row["template_id"]
            value = (
                row["stable_count_gene"],
                row["mapping_status"],
                row["scope_chromosome"],
            )
            previous = truth.setdefault(template_id, value)
            if previous != value:
                raise RuntimeError(f"{path}: conflicting truth for {template_id!r}")
    if not truth:
        raise RuntimeError(f"{path}: empty truth table")
    return truth


def select_read_names(
    fastq: Path,
    sample_size: int,
    seed: str,
    expected_records: int,
    progress_every: int,
) -> tuple[list[tuple[int, str]], int]:
    # Negated integer hashes make heap[0] the largest (worst) selected hash.
    heap: list[tuple[int, str]] = []
    selected_hashes: set[int] = set()
    records = 0
    started = time.monotonic()
    for read_name, _ in fastq_records(fastq):
        records += 1
        value = hash_read_name(seed, read_name)
        if len(heap) < sample_size:
            if value in selected_hashes:
                raise RuntimeError("SHA-256 collision or duplicate selected read name")
            heapq.heappush(heap, (-value, read_name))
            selected_hashes.add(value)
        elif value < -heap[0][0]:
            if value in selected_hashes:
                raise RuntimeError("SHA-256 collision or duplicate selected read name")
            removed, _ = heapq.heapreplace(heap, (-value, read_name))
            selected_hashes.remove(-removed)
            selected_hashes.add(value)
        elif value == -heap[0][0]:
            # A 256-bit collision at the selection boundary must not silently
            # make the result input-order dependent.
            raise RuntimeError("SHA-256 collision at deterministic subset boundary")
        if progress_every and records % progress_every == 0:
            elapsed = time.monotonic() - started
            print(
                f"subset: ranked {records:,} FASTQ reads in {elapsed:.1f}s",
                file=sys.stderr,
                flush=True,
            )
    if records != expected_records:
        raise RuntimeError(
            f"FASTQ record count {records:,} differs from expected {expected_records:,}"
        )
    if len(heap) != sample_size:
        raise RuntimeError(f"requested {sample_size:,} reads from only {records:,}")
    selected = sorted((-negative, read_name) for negative, read_name in heap)
    if len({name for _, name in selected}) != sample_size:
        raise RuntimeError("selected read names are not unique")
    return selected, records


def write_selected_names(
    path: Path, plain_path: Path, selected: list[tuple[int, str]]
) -> None:
    with deterministic_gzip(path) as handle:
        writer = csv.writer(handle, delimiter="\t", lineterminator="\n")
        writer.writerow(("selection_rank", "sha256", "read_name"))
        for rank, (value, read_name) in enumerate(selected, start=1):
            writer.writerow((rank, f"{value:064x}", read_name))
    with plain_path.open("w") as handle:
        for _, read_name in selected:
            handle.write(read_name + "\n")


def write_subset_fastq(
    source: Path,
    destination: Path,
    selected_names: set[str],
    expected_records: int,
) -> int:
    seen: set[str] = set()
    source_records = 0
    with deterministic_gzip(destination) as output:
        for read_name, lines in fastq_records(source):
            source_records += 1
            if read_name not in selected_names:
                continue
            if read_name in seen:
                raise RuntimeError(f"duplicate selected FASTQ read {read_name!r}")
            seen.add(read_name)
            output.writelines(lines)
    if source_records != expected_records:
        raise RuntimeError("FASTQ changed between selection and subset writing")
    missing = selected_names - seen
    if missing:
        raise RuntimeError(f"FASTQ subset is missing {len(missing):,} selected reads")
    return len(seen)


def write_subset_bam(
    source: Path,
    destination: Path,
    selected_names: set[str],
    expected_records: int,
    threads: int,
    progress_every: int,
) -> tuple[int, int]:
    seen: set[str] = set()
    source_records = 0
    started = time.monotonic()
    with pysam.AlignmentFile(str(source), "rb", threads=threads) as input_bam:
        with pysam.AlignmentFile(
            str(destination), "wb", template=input_bam, threads=threads
        ) as output_bam:
            for record in input_bam.fetch(until_eof=True):
                source_records += 1
                read_name = record.query_name
                if read_name in selected_names:
                    if read_name in seen:
                        raise RuntimeError(f"duplicate selected BAM read {read_name!r}")
                    seen.add(read_name)
                    output_bam.write(record)
                if progress_every and source_records % progress_every == 0:
                    elapsed = time.monotonic() - started
                    print(
                        f"subset: scanned {source_records:,} BAM records; "
                        f"wrote {len(seen):,} in {elapsed:.1f}s",
                        file=sys.stderr,
                        flush=True,
                    )
    if source_records != expected_records:
        raise RuntimeError(
            f"BAM record count {source_records:,} differs from expected {expected_records:,}"
        )
    missing = selected_names - seen
    if missing:
        raise RuntimeError(f"BAM subset is missing {len(missing):,} selected reads")
    return source_records, len(seen)


def sample_truth_summary(
    selected: list[tuple[int, str]], truth: dict[str, tuple[str, str, str]]
) -> dict[str, object]:
    scopes = Counter()
    statuses = Counter()
    genes_by_scope: dict[str, set[str]] = defaultdict(set)
    for _, read_name in selected:
        fields = read_name.split(":")
        if len(fields) < 2 or fields[1] not in truth:
            raise RuntimeError(f"selected read lacks known template truth: {read_name!r}")
        gene, status, scope = truth[fields[1]]
        scopes[scope] += 1
        statuses[status] += 1
        if status == "mapped" and gene:
            genes_by_scope[scope].add(gene)
    return {
        "reads_by_scope": dict(sorted(scopes.items())),
        "reads_by_mapping_status": dict(sorted(statuses.items())),
        "mapped_truth_genes_by_scope": {
            scope: len(genes) for scope, genes in sorted(genes_by_scope.items())
        },
        "mapped_truth_genes_union": len(set().union(*genes_by_scope.values())),
    }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-bam", type=Path, required=True)
    parser.add_argument("--source-reads-fastq", type=Path, required=True)
    parser.add_argument("--template-truth", type=Path, required=True)
    parser.add_argument("--bam-checksum-root", type=Path, required=True)
    parser.add_argument("--bam-checksums", type=Path, required=True)
    parser.add_argument("--truth-checksum-root", type=Path, required=True)
    parser.add_argument("--truth-checksums", type=Path, required=True)
    parser.add_argument("--sample-size", type=int, required=True)
    parser.add_argument("--expected-source-records", type=int, required=True)
    parser.add_argument("--seed", required=True)
    parser.add_argument("--threads", type=int, default=4)
    parser.add_argument("--progress-every", type=int, default=1_000_000)
    parser.add_argument(
        "--skip-bam-output",
        action="store_true",
        help=(
            "Freeze names and subset FASTQ without scanning the BAM. Use this when a "
            "full-data barcode-correction pass will filter its output to the frozen names."
        ),
    )
    parser.add_argument("--out-dir", type=Path, required=True)
    args = parser.parse_args()
    for name, value in vars(args).items():
        if isinstance(value, Path):
            if not value.is_absolute():
                parser.error(f"--{name.replace('_', '-')} must be absolute")
            setattr(args, name, value.resolve())
    if args.sample_size <= 0 or args.expected_source_records < args.sample_size:
        parser.error("sample size must be positive and no larger than expected source records")
    if not args.seed:
        parser.error("--seed must be nonempty")
    if args.threads <= 0 or args.progress_every < 0:
        parser.error("threads must be positive and progress interval nonnegative")
    if args.out_dir.exists():
        parser.error(f"refusing existing output directory: {args.out_dir}")
    return args


def main() -> int:
    args = parse_args()
    inputs = {
        "source_bam": authoritative_input_record(
            args.source_bam, args.bam_checksum_root, args.bam_checksums
        ),
        "source_reads_fastq": authoritative_input_record(
            args.source_reads_fastq, args.truth_checksum_root, args.truth_checksums
        ),
        "template_truth": authoritative_input_record(
            args.template_truth, args.truth_checksum_root, args.truth_checksums
        ),
    }
    args.out_dir.mkdir(parents=True)
    (args.out_dir / "STATUS").write_text(
        f"running\tstarted_utc={now()}\n", encoding="utf-8"
    )
    try:
        provenance = args.out_dir / "provenance"
        provenance.mkdir()
        frozen_script = provenance / Path(__file__).name
        shutil.copy2(Path(__file__).resolve(), frozen_script)

        command = {
            "schema": SCHEMA,
            "script_version": SCRIPT_VERSION,
            "argv": [str(Path(__file__).resolve()), *sys.argv[1:]],
            "started_utc": now(),
        }
        (args.out_dir / "COMMAND.json").write_text(
            json.dumps(command, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )

        truth = load_truth(args.template_truth)
        selected, fastq_records_seen = select_read_names(
            args.source_reads_fastq,
            args.sample_size,
            args.seed,
            args.expected_source_records,
            args.progress_every,
        )
        selected_names = {name for _, name in selected}
        selected_path = args.out_dir / "selected_read_names.tsv.gz"
        plain_names = args.out_dir / "selected_read_names.txt"
        selected_partial = selected_path.with_name(selected_path.name + ".partial")
        plain_partial = plain_names.with_name(plain_names.name + ".partial")
        write_selected_names(selected_partial, plain_partial, selected)
        selected_partial.replace(selected_path)
        plain_partial.replace(plain_names)

        subset_fastq = args.out_dir / "R2.subset.fastq.gz"
        subset_fastq_partial = subset_fastq.with_name(subset_fastq.name + ".partial")
        subset_fastq_records = write_subset_fastq(
            args.source_reads_fastq,
            subset_fastq_partial,
            selected_names,
            args.expected_source_records,
        )
        subset_fastq_partial.replace(subset_fastq)

        subset_bam = args.out_dir / "genetag.subset.bam"
        if args.skip_bam_output:
            bam_records_seen: int | str = "not_scanned"
            subset_bam_records: int | str = "deferred_to_full_prior_correction"
        else:
            subset_bam_partial = subset_bam.with_name(subset_bam.name + ".partial")
            bam_records_seen, subset_bam_records = write_subset_bam(
                args.source_bam,
                subset_bam_partial,
                selected_names,
                args.expected_source_records,
                args.threads,
                args.progress_every,
            )
            subset_bam_partial.replace(subset_bam)

        truth_summary = sample_truth_summary(selected, truth)
        summary_rows = {
            "selection": "lowest_sha256(seed_NUL_read_name)",
            "seed": args.seed,
            "sample_size": args.sample_size,
            "source_fastq_records": fastq_records_seen,
            "source_bam_records": bam_records_seen,
            "subset_fastq_records": subset_fastq_records,
            "subset_bam_records": subset_bam_records,
            "assignment_fields_read_for_selection": "false",
            "raw_bam_subset_written": str(not args.skip_bam_output).lower(),
        }
        with (args.out_dir / "SUMMARY.tsv").open("w", newline="") as handle:
            writer = csv.writer(handle, delimiter="\t", lineterminator="\n")
            writer.writerows(summary_rows.items())

        manifest = {
            "schema": SCHEMA,
            "script_version": SCRIPT_VERSION,
            "status": "complete",
            "completed_utc": now(),
            "selection": {
                "universe": "canonical R2 FASTQ read names",
                "method": "lowest_sha256(seed_NUL_read_name)",
                "seed": args.seed,
                "sample_size": args.sample_size,
                "assignment_blind": True,
                "input_order_independent_membership": True,
                "output_order": "source FASTQ or BAM order",
            },
            "inputs": inputs,
            "truth_composition": truth_summary,
            "outputs": {
                path.name: {"bytes": path.stat().st_size}
                for path in (
                    selected_path,
                    plain_names,
                    subset_fastq,
                    args.out_dir / "SUMMARY.tsv",
                    *(() if args.skip_bam_output else (subset_bam,)),
                )
            },
            "runtime": {
                "python": sys.version,
                "pysam": pysam.__version__,
                "threads": args.threads,
                "script": {
                    "path": str(frozen_script),
                    "sha256": sha256(frozen_script),
                },
            },
        }
        manifest_path = args.out_dir / "MANIFEST.json"
        manifest_path.write_text(
            json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )
        checksum_targets = (
            args.out_dir / "COMMAND.json",
            manifest_path,
            args.out_dir / "SUMMARY.tsv",
            selected_path,
            plain_names,
            subset_fastq,
            *(() if args.skip_bam_output else (subset_bam,)),
            frozen_script,
        )
        with (args.out_dir / "SHA256SUMS").open("w") as handle:
            for path in checksum_targets:
                handle.write(f"{sha256(path)}  {path.relative_to(args.out_dir)}\n")
        (args.out_dir / "STATUS").write_text(
            f"complete\tselected_reads={args.sample_size}\tcompleted_utc={now()}\n",
            encoding="utf-8",
        )
    except BaseException as error:
        (args.out_dir / "STATUS").write_text(
            f"failed\terror={str(error).replace(chr(9), ' ')}\n", encoding="utf-8"
        )
        raise
    print(f"complete\t{args.sample_size}\t{args.out_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
