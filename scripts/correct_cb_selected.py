#!/usr/bin/env python3
"""Apply the frozen all-read barcode prior while emitting selected QNAMEs only.

This is an execution-equivalent specialization of ``bin/correct_cb.py`` for an
assignment-blind read subset. Pass 1 is unchanged and scans the complete BAM to
derive exact-whitelist abundance. Pass 2 checks QNAME membership before barcode
work, emitting selected non-transport records in source order.
"""

from __future__ import annotations

import argparse
import subprocess
import sys
import time
from pathlib import Path

import pysam


SCRIPT_VERSION = "1.0.0"
PSEUDOCOUNT = 1
QUALITY_BASE = 33
QUALITY_MAX = 33
BASES = "ACGT"
QUALITY_ERROR = [10.0 ** (-quality / 10.0) for quality in range(QUALITY_MAX + 1)]


def load_first_fields(path: Path, *, label: str) -> set[str]:
    values: set[str] = set()
    with path.open(encoding="utf-8") as handle:
        for line_number, raw in enumerate(handle, start=1):
            fields = raw.split()
            if not fields:
                continue
            value = fields[0]
            if value in values:
                raise RuntimeError(
                    f"{path}:{line_number}: duplicate {label} {value!r}"
                )
            values.add(value)
    if not values:
        raise RuntimeError(f"{path}: empty {label} file")
    return values


def neighbours(barcode: str, whitelist: set[str]) -> list[tuple[str, int]]:
    ambiguous = [index for index, base in enumerate(barcode) if base not in BASES]
    if len(ambiguous) > 1:
        return []
    if len(ambiguous) == 1:
        index = ambiguous[0]
        return [
            (barcode[:index] + base + barcode[index + 1 :], index)
            for base in BASES
            if barcode[:index] + base + barcode[index + 1 :] in whitelist
        ]
    result = []
    for index, observed_base in enumerate(barcode):
        for base in BASES:
            if base == observed_base:
                continue
            candidate = barcode[:index] + base + barcode[index + 1 :]
            if candidate in whitelist:
                result.append((candidate, index))
    return result


class Corrector:
    def __init__(
        self,
        whitelist: set[str],
        abundance: dict[str, int],
        threshold: float,
    ) -> None:
        self.whitelist = whitelist
        self.abundance = abundance
        self.threshold = threshold
        self.neighbour_cache: dict[str, list[tuple[str, int]]] = {}

    def correct(self, barcode: str, quality: str | None) -> str | None:
        if barcode in self.whitelist:
            return barcode
        candidates = self.neighbour_cache.get(barcode)
        if candidates is None:
            candidates = neighbours(barcode, self.whitelist)
            self.neighbour_cache[barcode] = candidates
        if not candidates:
            return None

        has_quality = quality is not None and len(quality) == len(barcode)
        total_probability = 0.0
        maximum_probability = -1.0
        best = None
        for candidate, mismatch_index in candidates:
            weight = self.abundance.get(candidate, 0) + PSEUDOCOUNT
            if has_quality:
                assert quality is not None
                capped_quality = ord(quality[mismatch_index]) - QUALITY_BASE
                capped_quality = max(0, min(QUALITY_MAX, capped_quality))
                weight *= QUALITY_ERROR[capped_quality]
            total_probability += weight
            if weight > maximum_probability:
                maximum_probability = weight
                best = candidate
        if (
            total_probability > 0.0
            and maximum_probability >= self.threshold * total_probability
        ):
            return best
        return None


def progress(phase: str, records: int, started: float) -> None:
    print(
        f"correct_cb_selected: {phase} records={records:,} "
        f"elapsed_seconds={time.monotonic() - started:.1f}",
        file=sys.stderr,
        flush=True,
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("in_bam", type=Path)
    parser.add_argument("out_bam", type=Path)
    parser.add_argument("whitelist", type=Path)
    parser.add_argument("qname_file", type=Path)
    parser.add_argument("--threshold", type=float, default=0.975)
    parser.add_argument("--expected-source-records", type=int)
    parser.add_argument("--expected-selected-records", type=int)
    parser.add_argument("--progress-every", type=int, default=1_000_000)
    parser.add_argument("--filter-threads", type=int, default=4)
    args = parser.parse_args()
    if not 0.0 <= args.threshold <= 1.0:
        parser.error("--threshold must be between zero and one")
    if args.progress_every < 0:
        parser.error("--progress-every must be nonnegative")
    if args.filter_threads < 1:
        parser.error("--filter-threads must be positive")
    for path in (args.in_bam, args.whitelist, args.qname_file):
        if not path.is_file():
            parser.error(f"missing input file: {path}")
    if args.out_bam.exists():
        parser.error(f"refusing existing output: {args.out_bam}")
    args.out_bam.parent.mkdir(parents=True, exist_ok=True)
    return args


def selected_bam_process(args: argparse.Namespace) -> subprocess.Popen[bytes]:
    # pysam ships the samtools dispatcher even when no standalone samtools
    # executable is installed in the runtime image. catch_stdout=False makes
    # the child write its uncompressed BAM stream directly to this pipe.
    program = """
import pysam
import sys

pysam.view(
    "--no-PG",
    "--qname-file",
    sys.argv[1],
    "--uncompressed",
    "--threads",
    sys.argv[2],
    sys.argv[3],
    catch_stdout=False,
)
"""
    process = subprocess.Popen(
        [
            sys.executable,
            "-c",
            program,
            str(args.qname_file),
            str(args.filter_threads),
            str(args.in_bam),
        ],
        stdout=subprocess.PIPE,
    )
    if process.stdout is None:
        raise RuntimeError("failed to open selected-BAM filter pipe")
    return process


def main() -> int:
    args = parse_args()
    whitelist = load_first_fields(args.whitelist, label="whitelist barcode")
    selected_names = load_first_fields(args.qname_file, label="selected QNAME")
    if (
        args.expected_selected_records is not None
        and len(selected_names) != args.expected_selected_records
    ):
        raise RuntimeError(
            f"selected QNAME count {len(selected_names):,}, expected "
            f"{args.expected_selected_records:,}"
        )

    abundance: dict[str, int] = {}
    pass_one_records = 0
    started = time.monotonic()
    with pysam.AlignmentFile(args.in_bam, "rb", check_sq=False) as source:
        for record in source:
            pass_one_records += 1
            if not record.is_secondary and not record.is_supplementary:
                if record.has_tag("CB"):
                    barcode = record.get_tag("CB")
                    if barcode in whitelist:
                        abundance[barcode] = abundance.get(barcode, 0) + 1
            if args.progress_every and pass_one_records % args.progress_every == 0:
                progress("prior", pass_one_records, started)
    if (
        args.expected_source_records is not None
        and pass_one_records != args.expected_source_records
    ):
        raise RuntimeError(
            f"pass-1 source count {pass_one_records:,}, expected "
            f"{args.expected_source_records:,}"
        )

    corrector = Corrector(whitelist, abundance, args.threshold)
    selected_input_records = 0
    selected_seen: set[str] = set()
    emitted = corrected = dropped = with_quality = barcode_only = 0
    started = time.monotonic()
    filter_process = selected_bam_process(args)
    filter_completed = False
    try:
        with pysam.AlignmentFile(
            filter_process.stdout, "rb", check_sq=False
        ) as source:
            with pysam.AlignmentFile(args.out_bam, "wb", template=source) as output:
                for record in source:
                    selected_input_records += 1
                    read_name = record.query_name
                    if read_name not in selected_names:
                        raise RuntimeError(
                            f"QNAME filter emitted an unselected record: {read_name!r}"
                        )
                    if read_name in selected_seen:
                        raise RuntimeError(
                            f"selected QNAME has multiple BAM records: {read_name!r}"
                        )
                    selected_seen.add(read_name)
                    if record.has_tag("XB") and record.get_tag("XB") == "barcode_only":
                        barcode_only += 1
                        if record.has_tag("CY"):
                            with_quality += 1
                        continue
                    barcode = record.get_tag("CB") if record.has_tag("CB") else None
                    quality = record.get_tag("CY") if record.has_tag("CY") else None
                    if quality is not None:
                        with_quality += 1
                    corrected_barcode = (
                        corrector.correct(barcode, quality)
                        if barcode is not None
                        else None
                    )
                    if corrected_barcode is None:
                        dropped += 1
                        continue
                    if corrected_barcode != barcode:
                        record.set_tag("CB", corrected_barcode)
                        corrected += 1
                    output.write(record)
                    emitted += 1
                    if (
                        args.progress_every
                        and selected_input_records % args.progress_every == 0
                    ):
                        progress("selected-output", selected_input_records, started)
        filter_process.stdout.close()
        filter_returncode = filter_process.wait()
        filter_completed = True
        if filter_returncode != 0:
            raise RuntimeError(
                f"pysam QNAME filter failed with exit code {filter_returncode}"
            )
    finally:
        if not filter_completed:
            filter_process.stdout.close()
            filter_process.terminate()
            filter_process.wait()
    missing = selected_names - selected_seen
    if missing:
        examples = ", ".join(repr(name) for name in sorted(missing)[:5])
        raise RuntimeError(
            f"source BAM lacks {len(missing):,} selected QNAMEs; examples: {examples}"
        )
    if (
        args.expected_selected_records is not None
        and len(selected_seen) != args.expected_selected_records
    ):
        raise RuntimeError(
            f"selected BAM records {len(selected_seen):,}, expected "
            f"{args.expected_selected_records:,}"
        )

    print(
        f"source_records={pass_one_records} selected_records={len(selected_seen)} "
        f"selected_filter_records={selected_input_records} "
        f"emitted={emitted} corrected={corrected} dropped_uncorrectable={dropped} "
        f"barcode_only={barcode_only} "
        f"whitelist_barcodes_with_exact_reads={len(abundance)} "
        f"selected_reads_with_CY={with_quality}",
        file=sys.stderr,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
