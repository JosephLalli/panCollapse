#!/usr/bin/env python3

from pathlib import Path
import struct
import sys


EXPECTED_SUMMARY = {
    "input_records": "2",
    "input_read_groups": "1",
    "emitted_groups": "0",
    "mixed_orientation_dropped_groups": "1",
    "no_compatible_transcript_groups": "0",
    "raw_molecule_missing_groups": "0",
    "raw_molecule_malformed_groups": "0",
    "raw_molecule_unsupported_groups": "0",
    "raw_molecule_skipped_groups": "0",
    "grouping_recurrence_failures": "0",
}


def fail(message: str) -> None:
    raise SystemExit(message)


def read_summary(path: Path) -> dict[str, str]:
    if not path.exists():
        fail(f"missing expected file: {path}")
    rows: dict[str, str] = {}
    for line in path.read_text().splitlines():
        fields = line.split("\t")
        if len(fields) != 2:
            fail(f"malformed summary line: {line!r}")
        rows[fields[0]] = fields[1]
    return rows


class RadHeaderReader:
    def __init__(self, path: Path):
        self.data = path.read_bytes()
        self.offset = 0

    def read(self, size: int) -> bytes:
        end = self.offset + size
        if end > len(self.data):
            fail("unexpected EOF while reading RAD header")
        value = self.data[self.offset:end]
        self.offset = end
        return value

    def u8(self) -> int:
        return self.read(1)[0]

    def u16(self) -> int:
        return struct.unpack_from("<H", self.read(2))[0]

    def u64(self) -> int:
        return struct.unpack_from("<Q", self.read(8))[0]

    def string(self) -> str:
        return self.read(self.u16()).decode()

    def tag_section(self) -> None:
        for _ in range(self.u16()):
            self.string()
            self.u8()


def verify_no_chunks(path: Path) -> None:
    reader = RadHeaderReader(path)
    is_paired = reader.u8()
    ref_count = reader.u64()
    refs = [reader.string() for _ in range(ref_count)]
    num_chunks = reader.u64()
    reader.tag_section()
    reader.tag_section()
    reader.tag_section()
    file_cblen = reader.u16()
    file_ulen = reader.u16()
    if is_paired != 0 or refs != ["TX_A"] or num_chunks != 0:
        fail(f"unexpected empty RAD header: paired={is_paired}, refs={refs!r}, chunks={num_chunks}")
    if file_cblen != 16 or file_ulen != 12:
        fail(f"unexpected RAD file tag values: cblen={file_cblen}, ulen={file_ulen}")
    if reader.offset != len(reader.data):
        fail("expected dropped-output RAD to contain no chunk bytes")


def main(argv: list[str]) -> int:
    if len(argv) != 2:
        fail("usage: phase3_mixed_orientation_verify.py PC_OUT")

    pc_out = Path(argv[1])
    for filename in ("map.rad", "tx2gene.tsv", "summary.tsv"):
        if not (pc_out / filename).exists():
            fail(f"missing expected file: {pc_out / filename}")

    summary = read_summary(pc_out / "summary.tsv")
    for key, expected in EXPECTED_SUMMARY.items():
        actual = summary.get(key)
        if actual != expected:
            fail(f"summary {key} expected {expected!r}, saw {actual!r}")

    if (pc_out / "tx2gene.tsv").read_text() != "TX_A\tGENE_A\n":
        fail("unexpected tx2gene.tsv contents")
    verify_no_chunks(pc_out / "map.rad")

    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
