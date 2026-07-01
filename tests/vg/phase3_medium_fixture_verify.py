#!/usr/bin/env python3

from collections import Counter
from pathlib import Path
import struct
import sys


RAD_FORWARD_MASK = 0x80000000


def fail(message: str) -> None:
    raise SystemExit(message)


class RadReader:
    def __init__(self, path: Path):
        self.path = path
        self.data = path.read_bytes()
        self.offset = 0

    def read(self, size: int) -> bytes:
        end = self.offset + size
        if end > len(self.data):
            fail(f"unexpected EOF while reading {self.path}")
        value = self.data[self.offset:end]
        self.offset = end
        return value

    def u8(self) -> int:
        return self.read(1)[0]

    def u16(self) -> int:
        return struct.unpack_from("<H", self.read(2))[0]

    def u32(self) -> int:
        return struct.unpack_from("<I", self.read(4))[0]

    def u64(self) -> int:
        return struct.unpack_from("<Q", self.read(8))[0]

    def string(self) -> str:
        return self.read(self.u16()).decode()

    def tag_section(self) -> list[tuple[str, int]]:
        return [(self.string(), self.u8()) for _ in range(self.u16())]

    def typed_value(self, type_id: int) -> int:
        if type_id == 1:
            return self.u8()
        if type_id == 2:
            return self.u16()
        if type_id == 3:
            return self.u32()
        if type_id == 4:
            return self.u64()
        fail(f"unsupported RAD integer type id: {type_id}")


def decode_packed_sequence(value: int, length: int) -> str:
    bases = "ACGT"
    return "".join(bases[(value >> shift) & 0x3] for shift in range(2 * (length - 1), -1, -2))


def read_key_value_tsv(path: Path) -> dict[str, str]:
    rows: dict[str, str] = {}
    for line in path.read_text().splitlines():
        if not line:
            continue
        fields = line.split("\t")
        if len(fields) != 2:
            fail(f"malformed key/value row in {path}: {line!r}")
        rows[fields[0]] = fields[1]
    return rows


def expected_records(path: Path) -> Counter[tuple[str, str, tuple[str, ...], tuple[str, ...]]]:
    rows = Counter()
    lines = path.read_text().splitlines()
    if not lines or lines[0] != "read_name\tbarcode\tumi\ttargets\tdirs\tcase":
        fail("unexpected expected_rad_records.tsv header")
    for line in lines[1:]:
        read_name, barcode, umi, targets, dirs, _case = line.split("\t")
        if read_name.rsplit("_", 2)[1:] != [barcode, umi]:
            fail(f"raw molecule fields do not match read-name suffix: {read_name}")
        target_values = tuple(targets.split(","))
        dir_values = tuple(dirs.split(","))
        if len(target_values) != len(dir_values):
            fail(f"target/dir length mismatch for {read_name}")
        rows[(barcode, umi, target_values, dir_values)] += 1
    return rows


def expected_read_names(records_path: Path, non_emitted_path: Path) -> set[str]:
    names: set[str] = set()
    for path in [records_path, non_emitted_path]:
        lines = path.read_text().splitlines()
        for line in lines[1:]:
            names.add(line.split("\t", 1)[0])
    return names


def fastq_read_names(path: Path) -> set[str]:
    lines = path.read_text().splitlines()
    if len(lines) % 4 != 0:
        fail("FASTQ line count is not divisible by four")
    names = set()
    for i in range(0, len(lines), 4):
        if not lines[i].startswith("@"):
            fail(f"malformed FASTQ header at line {i + 1}: {lines[i]!r}")
        names.add(lines[i][1:])
    return names


def read_rad_records(path: Path) -> tuple[list[str], Counter[tuple[str, str, tuple[str, ...], tuple[str, ...]]]]:
    reader = RadReader(path)
    is_paired = reader.u8()
    refs = [reader.string() for _ in range(reader.u64())]
    num_chunks = reader.u64()
    file_tags = reader.tag_section()
    read_tags = dict(reader.tag_section())
    aln_tags = dict(reader.tag_section())
    file_values = {name: reader.typed_value(type_id) for name, type_id in file_tags}

    if is_paired != 0:
        fail("medium fixture expects single-end RAD")
    if refs != ["TX_A", "TX_A_ALT", "TX_B", "TX_H"]:
        fail(f"unexpected RAD target dictionary: {refs!r}")
    if file_values != {"cblen": 16, "ulen": 12}:
        fail(f"unexpected RAD file tag values: {file_values!r}")
    if set(read_tags) != {"b", "u"} or aln_tags != {"compressed_ori_refid": 3}:
        fail(f"unexpected RAD tag schema: read={read_tags!r}, aln={aln_tags!r}")

    records = Counter()
    chunks_to_read = None if num_chunks == 0 else num_chunks
    chunks_read = 0
    while reader.offset < len(reader.data) and (chunks_to_read is None or chunks_read < chunks_to_read):
        chunk_start = reader.offset
        chunk_size = reader.u32()
        nrec = reader.u32()
        chunk_end = chunk_start + chunk_size
        if chunk_size < 8 or chunk_end > len(reader.data):
            fail("invalid RAD chunk size")
        for _ in range(nrec):
            naln = reader.u32()
            barcode = decode_packed_sequence(reader.typed_value(read_tags["b"]), 16)
            umi = decode_packed_sequence(reader.typed_value(read_tags["u"]), 12)
            target_names: list[str] = []
            dirs: list[str] = []
            for _ in range(naln):
                compressed = reader.u32()
                target_id = compressed & ~RAD_FORWARD_MASK
                if target_id >= len(refs):
                    fail(f"RAD target ID out of range: {target_id}")
                target_names.append(refs[target_id])
                dirs.append("fw" if compressed & RAD_FORWARD_MASK else "rc")
            records[(barcode, umi, tuple(target_names), tuple(dirs))] += 1
        if reader.offset != chunk_end:
            fail("RAD chunk did not end on declared boundary")
        chunks_read += 1
    if chunks_to_read is not None and chunks_read != chunks_to_read:
        fail(f"RAD ended after {chunks_read} chunks, expected {chunks_to_read}")
    if reader.offset != len(reader.data):
        fail("RAD contains trailing bytes after declared chunks")
    return refs, records


def main(argv: list[str]) -> int:
    if len(argv) != 2:
        fail("usage: phase3_medium_fixture_verify.py WORK_DIR")
    work_dir = Path(argv[1])
    pc_out = work_dir / "pc_medium"

    for required in [
        "reads.fastq",
        "reads.gamp.json",
        "reads.gamp",
        "graph.xg",
        "annotation.gtf",
        "collapse.tsv",
        "expected_rad_records.tsv",
        "expected_non_emitted.tsv",
    ]:
        if not (work_dir / required).exists():
            fail(f"missing generated fixture artifact: {work_dir / required}")

    expected_summary = read_key_value_tsv(work_dir / "expected_summary.tsv")
    actual_summary = read_key_value_tsv(pc_out / "summary.tsv")
    for key, expected in expected_summary.items():
        actual = actual_summary.get(key)
        if actual != expected:
            fail(f"summary {key} expected {expected!r}, saw {actual!r}")

    tx2gene = (pc_out / "tx2gene.tsv").read_text()
    if tx2gene != "TX_A\tGENE_A\nTX_A_ALT\tGENE_A_ALT\nTX_B\tGENE_B\nTX_H\tGENE_H\n":
        fail(f"unexpected tx2gene.tsv contents: {tx2gene!r}")

    _refs, observed = read_rad_records(pc_out / "map.rad")
    expected = expected_records(work_dir / "expected_rad_records.tsv")
    if observed != expected:
        missing = expected - observed
        extra = observed - expected
        fail(
            "RAD semantic records did not match expected truth; "
            f"missing_examples={list(missing.items())[:5]!r}; extra_examples={list(extra.items())[:5]!r}"
        )

    expected_non_emitted_lines = (work_dir / "expected_non_emitted.tsv").read_text().splitlines()
    if len(expected_non_emitted_lines) != 15_001:
        fail(f"expected 15,000 non-emitted read groups, saw {len(expected_non_emitted_lines) - 1}")
    names_from_truth = expected_read_names(work_dir / "expected_rad_records.tsv", work_dir / "expected_non_emitted.tsv")
    names_from_fastq = fastq_read_names(work_dir / "reads.fastq")
    if names_from_truth != names_from_fastq or len(names_from_fastq) != 50_000:
        fail("FASTQ read names do not match the expected emitted/non-emitted truth tables")

    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
