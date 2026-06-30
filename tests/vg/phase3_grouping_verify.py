#!/usr/bin/env python3

from pathlib import Path
import struct
import sys


RAD_FORWARD_MASK = 0x80000000
EXPECTED_BARCODE = "AAACCCAAGTTTGGGA"
EXPECTED_UMI = "AAAAAAAAAAAA"


def fail(message: str) -> None:
    raise SystemExit(message)


def read_summary(path: Path) -> dict[str, str]:
    rows: dict[str, str] = {}
    for line in path.read_text().splitlines():
        fields = line.split("\t")
        if len(fields) != 2:
            fail(f"malformed summary line: {line!r}")
        rows[fields[0]] = fields[1]
    return rows


def matrix_data_lines(path: Path) -> list[str]:
    return [line.strip() for line in path.read_text().splitlines() if line.strip() and not line.startswith("%")]


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


def read_rad_records(
    pc_out: Path,
    expected_barcode: str = EXPECTED_BARCODE,
    expected_umi: str = EXPECTED_UMI,
) -> tuple[list[str], list[list[int]]]:
    reader = RadReader(pc_out / "map.rad")
    is_paired = reader.u8()
    refs = [reader.string() for _ in range(reader.u64())]
    num_chunks = reader.u64()
    file_tags = reader.tag_section()
    read_tags = dict(reader.tag_section())
    aln_tags = dict(reader.tag_section())
    file_values = {name: reader.typed_value(type_id) for name, type_id in file_tags}
    if is_paired != 0 or num_chunks != 1:
        fail(f"unexpected RAD header: paired={is_paired}, chunks={num_chunks}")
    if file_values != {"cblen": 16, "ulen": 12}:
        fail(f"unexpected file tag values: {file_values!r}")
    if set(read_tags) != {"b", "u"} or aln_tags != {"compressed_ori_refid": 3}:
        fail(f"unexpected tag schema: read={read_tags!r}, aln={aln_tags!r}")

    chunk_start = reader.offset
    chunk_size = reader.u32()
    nrec = reader.u32()
    records: list[list[int]] = []
    for _ in range(nrec):
        naln = reader.u32()
        bc = reader.typed_value(read_tags["b"])
        umi = reader.typed_value(read_tags["u"])
        if decode_packed_sequence(bc, 16) != expected_barcode:
            fail("unexpected barcode")
        if decode_packed_sequence(umi, 12) != expected_umi:
            fail("unexpected UMI")
        records.append([reader.u32() for _ in range(naln)])
    if chunk_size != reader.offset - chunk_start or reader.offset != len(reader.data):
        fail("RAD chunk size or trailing-byte check failed")
    return refs, records


def verify_two_groups(pc_out: Path) -> None:
    expected_summary = {
        "input_records": "2",
        "input_read_groups": "2",
        "emitted_groups": "2",
        "mixed_orientation_dropped_groups": "0",
        "no_compatible_transcript_groups": "0",
        "raw_molecule_missing_groups": "0",
        "raw_molecule_malformed_groups": "0",
        "raw_molecule_unsupported_groups": "0",
        "raw_molecule_skipped_groups": "0",
        "grouping_recurrence_failures": "0",
    }
    summary = read_summary(pc_out / "summary.tsv")
    for key, expected in expected_summary.items():
        if summary.get(key) != expected:
            fail(f"summary {key} expected {expected!r}, saw {summary.get(key)!r}")
    if (pc_out / "tx2gene.tsv").read_text() != "TX_A\tGENE_A\n":
        fail("unexpected tx2gene.tsv contents")
    refs, records = read_rad_records(pc_out)
    if refs != ["TX_A"] or records != [[RAD_FORWARD_MASK], [RAD_FORWARD_MASK]]:
        fail(f"unexpected two-group RAD records: refs={refs!r}, records={records!r}")


def verify_multi_target(pc_out: Path) -> None:
    expected_summary = {
        "input_records": "2",
        "input_read_groups": "1",
        "emitted_groups": "1",
        "mixed_orientation_dropped_groups": "0",
        "no_compatible_transcript_groups": "0",
        "raw_molecule_missing_groups": "0",
        "raw_molecule_malformed_groups": "0",
        "raw_molecule_unsupported_groups": "0",
        "raw_molecule_skipped_groups": "0",
        "grouping_recurrence_failures": "0",
    }
    summary = read_summary(pc_out / "summary.tsv")
    for key, expected in expected_summary.items():
        if summary.get(key) != expected:
            fail(f"summary {key} expected {expected!r}, saw {summary.get(key)!r}")
    if (pc_out / "tx2gene.tsv").read_text() != "TX_A\tGENE_A\nTX_B\tGENE_B\n":
        fail("unexpected tx2gene.tsv contents")
    refs, records = read_rad_records(pc_out)
    if refs != ["TX_A", "TX_B"] or records != [[RAD_FORWARD_MASK, 1]]:
        fail(f"unexpected multi-target RAD records: refs={refs!r}, records={records!r}")


def verify_multi_target_quant(alevin_dir: Path) -> None:
    rows = (alevin_dir / "quants_mat_rows.txt").read_text().splitlines()
    cols = (alevin_dir / "quants_mat_cols.txt").read_text().splitlines()
    matrix = matrix_data_lines(alevin_dir / "quants_mat.mtx")
    if rows != [EXPECTED_BARCODE]:
        fail(f"unexpected rows: {rows!r}")
    if cols != ["GENE_A", "GENE_B"]:
        fail(f"unexpected columns: {cols!r}")
    if matrix != ["1 2 0"]:
        fail(f"unexpected ambiguous multi-target matrix: {matrix!r}")


def verify_no_compatible(pc_out: Path) -> None:
    expected_summary = {
        "input_records": "1",
        "input_read_groups": "1",
        "emitted_groups": "0",
        "mixed_orientation_dropped_groups": "0",
        "no_compatible_transcript_groups": "1",
        "raw_molecule_missing_groups": "0",
        "raw_molecule_malformed_groups": "0",
        "raw_molecule_unsupported_groups": "0",
        "raw_molecule_skipped_groups": "0",
        "grouping_recurrence_failures": "0",
    }
    summary = read_summary(pc_out / "summary.tsv")
    for key, expected in expected_summary.items():
        if summary.get(key) != expected:
            fail(f"summary {key} expected {expected!r}, saw {summary.get(key)!r}")
    if (pc_out / "tx2gene.tsv").read_text() != "TX_A\tGENE_A\n":
        fail("unexpected tx2gene.tsv contents")
    reader = RadReader(pc_out / "map.rad")
    is_paired = reader.u8()
    refs = [reader.string() for _ in range(reader.u64())]
    num_chunks = reader.u64()
    reader.tag_section()
    reader.tag_section()
    reader.tag_section()
    file_cblen = reader.u16()
    file_ulen = reader.u16()
    if is_paired != 0 or refs != ["TX_A"] or num_chunks != 0:
        fail(f"unexpected no-compatible RAD header: paired={is_paired}, refs={refs!r}, chunks={num_chunks}")
    if file_cblen != 16 or file_ulen != 12 or reader.offset != len(reader.data):
        fail("unexpected no-compatible RAD file tag values or trailing bytes")


def verify_bad_molecule_skip(pc_out: Path) -> None:
    expected_summary = {
        "input_records": "3",
        "input_read_groups": "3",
        "emitted_groups": "0",
        "mixed_orientation_dropped_groups": "0",
        "no_compatible_transcript_groups": "0",
        "raw_molecule_missing_groups": "1",
        "raw_molecule_malformed_groups": "1",
        "raw_molecule_unsupported_groups": "1",
        "raw_molecule_skipped_groups": "3",
        "grouping_recurrence_failures": "0",
    }
    summary = read_summary(pc_out / "summary.tsv")
    for key, expected in expected_summary.items():
        if summary.get(key) != expected:
            fail(f"summary {key} expected {expected!r}, saw {summary.get(key)!r}")
    if (pc_out / "tx2gene.tsv").read_text() != "TX_A\tGENE_A\n":
        fail("unexpected tx2gene.tsv contents")
    reader = RadReader(pc_out / "map.rad")
    is_paired = reader.u8()
    refs = [reader.string() for _ in range(reader.u64())]
    num_chunks = reader.u64()
    reader.tag_section()
    reader.tag_section()
    reader.tag_section()
    file_cblen = reader.u16()
    file_ulen = reader.u16()
    if is_paired != 0 or refs != ["TX_A"] or num_chunks != 0:
        fail(f"unexpected bad-molecule RAD header: paired={is_paired}, refs={refs!r}, chunks={num_chunks}")
    if file_cblen != 16 or file_ulen != 12 or reader.offset != len(reader.data):
        fail("unexpected bad-molecule RAD file tag values or trailing bytes")


def verify_reverse_path(pc_out: Path) -> None:
    expected_summary = {
        "input_records": "2",
        "input_read_groups": "2",
        "emitted_groups": "2",
        "mixed_orientation_dropped_groups": "0",
        "no_compatible_transcript_groups": "0",
        "raw_molecule_missing_groups": "0",
        "raw_molecule_malformed_groups": "0",
        "raw_molecule_unsupported_groups": "0",
        "raw_molecule_skipped_groups": "0",
        "grouping_recurrence_failures": "0",
    }
    summary = read_summary(pc_out / "summary.tsv")
    for key, expected in expected_summary.items():
        if summary.get(key) != expected:
            fail(f"summary {key} expected {expected!r}, saw {summary.get(key)!r}")
    if (pc_out / "tx2gene.tsv").read_text() != "TX_REV\tGENE_REV\n":
        fail("unexpected tx2gene.tsv contents")
    refs, records = read_rad_records(pc_out)
    if refs != ["TX_REV"] or records != [[RAD_FORWARD_MASK], [0]]:
        fail(f"unexpected reverse-path RAD records: refs={refs!r}, records={records!r}")


def verify_n_base(pc_out: Path) -> None:
    expected_summary = {
        "input_records": "1",
        "input_read_groups": "1",
        "emitted_groups": "1",
        "mixed_orientation_dropped_groups": "0",
        "no_compatible_transcript_groups": "0",
        "raw_molecule_missing_groups": "0",
        "raw_molecule_malformed_groups": "0",
        "raw_molecule_unsupported_groups": "0",
        "raw_molecule_skipped_groups": "0",
        "grouping_recurrence_failures": "0",
    }
    summary = read_summary(pc_out / "summary.tsv")
    for key, expected in expected_summary.items():
        if summary.get(key) != expected:
            fail(f"summary {key} expected {expected!r}, saw {summary.get(key)!r}")
    if (pc_out / "tx2gene.tsv").read_text() != "TX_A\tGENE_A\n":
        fail("unexpected tx2gene.tsv contents")
    refs, records = read_rad_records(pc_out)
    if refs != ["TX_A"] or records != [[RAD_FORWARD_MASK]]:
        fail(f"unexpected N-base RAD records: refs={refs!r}, records={records!r}")


def main(argv: list[str]) -> int:
    if len(argv) != 3 or argv[1] not in {
        "two-groups",
        "multi-target",
        "multi-target-quant",
        "no-compatible",
        "bad-molecule-skip",
        "reverse-path",
        "n-base",
    }:
        fail(
            "usage: phase3_grouping_verify.py "
            "two-groups|multi-target|multi-target-quant|no-compatible|bad-molecule-skip|reverse-path|n-base PATH"
        )
    if argv[1] == "two-groups":
        verify_two_groups(Path(argv[2]))
    elif argv[1] == "multi-target":
        verify_multi_target(Path(argv[2]))
    elif argv[1] == "multi-target-quant":
        verify_multi_target_quant(Path(argv[2]))
    elif argv[1] == "no-compatible":
        verify_no_compatible(Path(argv[2]))
    elif argv[1] == "bad-molecule-skip":
        verify_bad_molecule_skip(Path(argv[2]))
    elif argv[1] == "reverse-path":
        verify_reverse_path(Path(argv[2]))
    else:
        verify_n_base(Path(argv[2]))
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
