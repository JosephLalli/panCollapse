from __future__ import annotations

import gzip
import hashlib
import os
import subprocess
import sys
from pathlib import Path

import pysam
import pytest


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts/subset_bam_by_fastq_hash.py"
CORRECT_CB_SELECTED = ROOT / "scripts/correct_cb_selected.py"
CORRECT_CB = Path(
    os.environ.get(
        "PANCOLLAPSE_BENCHMARK_CORRECT_CB",
        "/mnt/ssd/lalli/"
        "hg002_chr20_chr21_chr22_strict_membership_final_method_freeze_v8_v090_"
        "20260904T055700Z/provenance/bin/correct_cb.py",
    )
)
CORRECT_CB_SHA256 = (
    "a54ac84bf757e779e9591792f6e387d241acbbdd0f8d40be1590ae3c6a83b526"
)


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_checksum_manifest(root: Path, paths: list[Path]) -> Path:
    manifest = root / "SHA256SUMS"
    manifest.write_text(
        "".join(f"{digest(path)}  {path.relative_to(root)}\n" for path in paths)
    )
    return manifest


def fastq_names(path: Path) -> list[str]:
    result = []
    with gzip.open(path, "rt") as handle:
        while header := handle.readline():
            result.append(header[1:].strip())
            assert handle.readline()
            assert handle.readline().startswith("+")
            assert handle.readline()
    return result


def test_exact_hash_subset_is_assignment_blind_and_order_preserving(tmp_path: Path) -> None:
    truth_root = tmp_path / "truth"
    bam_root = tmp_path / "bam"
    truth_root.mkdir()
    bam_root.mkdir()
    names = [f"CB{i:02d}:F{i:06d}:1:2:3:UMI{i:02d}:{i}" for i in range(30)]
    fastq_order = names[::2] + names[1::2]
    fastq = truth_root / "R2.fastq.gz"
    with gzip.GzipFile(filename="", mode="wb", fileobj=fastq.open("wb"), mtime=0) as raw:
        for name in fastq_order:
            raw.write(f"@{name}\nACGT\n+\nNNNN\n".encode())
    truth = truth_root / "template_truth.tsv"
    truth.write_text(
        "template_id\tstable_count_gene\tmapping_status\tscope_chromosome\n"
        + "".join(
            f"F{i:06d}\tENSG{i:011d}\tmapped\tchr{20 + i % 3}\n"
            for i in range(30)
        )
    )
    bam = bam_root / "genetag.bam"
    header = {"HD": {"VN": "1.6", "SO": "unsorted"}, "SQ": [{"SN": "x", "LN": 100}]}
    with pysam.AlignmentFile(bam, "wb", header=header) as output:
        for index, name in enumerate(reversed(names)):
            record = pysam.AlignedSegment(output.header)
            record.query_name = name
            record.query_sequence = "A"
            record.flag = 4
            record.query_qualities = pysam.qualitystring_to_array("N")
            record.set_tag("GX", f"assignment-{index}")
            output.write(record)
    truth_sums = write_checksum_manifest(truth_root, [fastq, truth])
    bam_sums = write_checksum_manifest(bam_root, [bam])
    seed = "unit-test-seed"

    outputs = []
    for run in ("one", "two"):
        out = tmp_path / run
        subprocess.run(
            [
                "python3", "-B", str(SCRIPT),
                "--source-bam", str(bam),
                "--source-reads-fastq", str(fastq),
                "--template-truth", str(truth),
                "--bam-checksum-root", str(bam_root),
                "--bam-checksums", str(bam_sums),
                "--truth-checksum-root", str(truth_root),
                "--truth-checksums", str(truth_sums),
                "--sample-size", "7",
                "--expected-source-records", "30",
                "--seed", seed,
                "--threads", "1",
                "--progress-every", "0",
                "--out-dir", str(out),
            ],
            check=True,
        )
        outputs.append(out)

    expected = set(
        sorted(
            names,
            key=lambda name: hashlib.sha256(
                seed.encode() + b"\0" + name.encode()
            ).digest(),
        )[:7]
    )
    assert fastq_names(outputs[0] / "R2.subset.fastq.gz") == [
        name for name in fastq_order if name in expected
    ]
    with pysam.AlignmentFile(outputs[0] / "genetag.subset.bam", "rb") as observed:
        assert [record.query_name for record in observed.fetch(until_eof=True)] == [
            name for name in reversed(names) if name in expected
        ]
    assert (outputs[0] / "STATUS").read_text().startswith("complete\tselected_reads=7\t")
    for relative in (
        "R2.subset.fastq.gz",
        "genetag.subset.bam",
        "selected_read_names.tsv.gz",
        "selected_read_names.txt",
        "SUMMARY.tsv",
    ):
        assert digest(outputs[0] / relative) == digest(outputs[1] / relative)

    selection_only = tmp_path / "selection-only"
    subprocess.run(
        [
            "python3", "-B", str(SCRIPT),
            "--source-bam", str(bam),
            "--source-reads-fastq", str(fastq),
            "--template-truth", str(truth),
            "--bam-checksum-root", str(bam_root),
            "--bam-checksums", str(bam_sums),
            "--truth-checksum-root", str(truth_root),
            "--truth-checksums", str(truth_sums),
            "--sample-size", "7",
            "--expected-source-records", "30",
            "--seed", seed,
            "--threads", "1",
            "--progress-every", "0",
            "--skip-bam-output",
            "--out-dir", str(selection_only),
        ],
        check=True,
    )
    assert not (selection_only / "genetag.subset.bam").exists()
    assert set((selection_only / "selected_read_names.txt").read_text().splitlines()) == expected


def test_frozen_barcode_correction_can_be_stream_filtered(tmp_path: Path) -> None:
    if not CORRECT_CB.is_file():
        pytest.skip(
            "set PANCOLLAPSE_BENCHMARK_CORRECT_CB to the checksum-pinned "
            "panSC correct_cb.py integration oracle"
        )
    assert digest(CORRECT_CB) == CORRECT_CB_SHA256

    bam = tmp_path / "raw.bam"
    whitelist = tmp_path / "whitelist.txt"
    whitelist.write_text("AAAAAAAAAAAAAAAA\nCCCCCCCCCCCCCCCC\n")
    header = {"HD": {"VN": "1.6", "SO": "unsorted"}, "SQ": [{"SN": "x", "LN": 100}]}
    names = ["keep-exact", "omit-exact", "keep-corrected"]
    barcodes = ["AAAAAAAAAAAAAAAA", "CCCCCCCCCCCCCCCC", "AAAAAAAAAAAAAAAT"]
    with pysam.AlignmentFile(bam, "wb", header=header) as output:
        for name, barcode in zip(names, barcodes, strict=True):
            record = pysam.AlignedSegment(output.header)
            record.query_name = name
            record.query_sequence = "A"
            record.flag = 4
            record.query_qualities = pysam.qualitystring_to_array("N")
            record.set_tag("CB", barcode)
            record.set_tag("CY", "F" * 16)
            output.write(record)
    selected = tmp_path / "selected.txt"
    selected.write_text("keep-exact\nkeep-corrected\n")
    corrected = tmp_path / "corrected-subset.bam"
    corrected_selected = tmp_path / "corrected-selected-direct.bam"

    correct = subprocess.Popen(
        [sys.executable, "-B", str(CORRECT_CB), str(bam), "-", str(whitelist)],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    assert correct.stdout is not None
    filtered = subprocess.run(
        [
            "samtools", "view", "--no-PG", "-N", str(selected), "-b", "-o",
            str(corrected), "-",
        ],
        stdin=correct.stdout,
        capture_output=True,
        check=True,
    )
    correct.stdout.close()
    stderr = correct.stderr.read().decode() if correct.stderr is not None else ""
    assert correct.wait() == 0, stderr
    assert filtered.stderr == b""
    selected_run = subprocess.run(
        [
            sys.executable,
            "-B",
            str(CORRECT_CB_SELECTED),
            str(bam),
            str(corrected_selected),
            str(whitelist),
            str(selected),
            "--expected-source-records",
            "3",
            "--expected-selected-records",
            "2",
            "--progress-every",
            "0",
        ],
        capture_output=True,
        text=True,
        check=True,
    )
    assert (
        "source_records=3 selected_records=2 selected_filter_records=2 emitted=2"
        in selected_run.stderr
    )
    with pysam.AlignmentFile(corrected, "rb") as observed:
        records = list(observed.fetch(until_eof=True))
    with pysam.AlignmentFile(corrected_selected, "rb") as observed:
        direct_records = list(observed.fetch(until_eof=True))
    assert [record.query_name for record in records] == ["keep-exact", "keep-corrected"]
    assert [record.get_tag("CB") for record in records] == [
        "AAAAAAAAAAAAAAAA", "AAAAAAAAAAAAAAAA"
    ]
    assert [record.to_string() for record in direct_records] == [
        record.to_string() for record in records
    ]


def test_selected_correction_uses_unselected_reads_in_global_prior(
    tmp_path: Path,
) -> None:
    bam = tmp_path / "raw.bam"
    whitelist = tmp_path / "whitelist.txt"
    barcode_a = "AAAAAAAAAAAAAAAA"
    barcode_b = "CAAAAAAAAAAAAAAC"
    ambiguous = "CAAAAAAAAAAAAAAA"
    whitelist.write_text(f"{barcode_a}\n{barcode_b}\n")
    header = {"HD": {"VN": "1.6", "SO": "unsorted"}, "SQ": [{"SN": "x", "LN": 100}]}
    with pysam.AlignmentFile(bam, "wb", header=header) as output:
        for index in range(100):
            record = pysam.AlignedSegment(output.header)
            record.query_name = f"unselected-a-{index}"
            record.query_sequence = "A"
            record.flag = 4
            record.query_qualities = pysam.qualitystring_to_array("N")
            record.set_tag("CB", barcode_a)
            record.set_tag("CY", "F" * 16)
            output.write(record)
        for name, barcode in (("unselected-b", barcode_b), ("selected", ambiguous)):
            record = pysam.AlignedSegment(output.header)
            record.query_name = name
            record.query_sequence = "A"
            record.flag = 4
            record.query_qualities = pysam.qualitystring_to_array("N")
            record.set_tag("CB", barcode)
            record.set_tag("CY", "F" * 16)
            output.write(record)
    selected = tmp_path / "selected.txt"
    selected.write_text("selected\n")
    corrected = tmp_path / "corrected-selected.bam"

    subprocess.run(
        [
            sys.executable,
            "-B",
            str(CORRECT_CB_SELECTED),
            str(bam),
            str(corrected),
            str(whitelist),
            str(selected),
            "--expected-source-records",
            "102",
            "--expected-selected-records",
            "1",
            "--progress-every",
            "0",
        ],
        check=True,
    )
    with pysam.AlignmentFile(corrected, "rb") as observed:
        records = list(observed.fetch(until_eof=True))
    assert len(records) == 1
    assert records[0].query_name == "selected"
    assert records[0].get_tag("CB") == barcode_a
