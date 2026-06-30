#!/usr/bin/env python3

from pathlib import Path
import struct
import sys


EXPECTED_BARCODE = "AAACCCAAGTTTGGGA"
EXPECTED_UMI = "AAAAAAAAAAAA"
EXPECTED_TARGET = "TX_A"
EXPECTED_GENE = "GENE_A"
RAD_U8 = 1
RAD_U16 = 2
RAD_U32 = 3
RAD_U64 = 4
RAD_FORWARD_MASK = 0x80000000


def fail(message: str) -> None:
    raise SystemExit(message)


def read_lines(path: Path) -> list[str]:
    if not path.exists():
        fail(f"missing expected file: {path}")
    return path.read_text().splitlines()


def matrix_data_lines(path: Path) -> list[str]:
    if not path.exists():
        fail(f"missing expected file: {path}")
    return [line.strip() for line in path.read_text().splitlines() if line.strip() and not line.startswith("%")]


class RadReader:
    def __init__(self, path: Path):
        if not path.exists():
            fail(f"missing expected file: {path}")
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
        if type_id == RAD_U8:
            return self.u8()
        if type_id == RAD_U16:
            return self.u16()
        if type_id == RAD_U32:
            return self.u32()
        if type_id == RAD_U64:
            return self.u64()
        fail(f"unsupported RAD integer type id: {type_id}")


def decode_packed_sequence(value: int, length: int) -> str:
    bases = "ACGT"
    return "".join(bases[(value >> shift) & 0x3] for shift in range(2 * (length - 1), -1, -2))


def verify_rad(pc_out: Path, expected_orientation: str) -> None:
    tx2gene = pc_out / "tx2gene.tsv"
    if tx2gene.read_text() != f"{EXPECTED_TARGET}\t{EXPECTED_GENE}\n":
        fail(f"unexpected tx2gene.tsv contents: {tx2gene.read_text()!r}")

    reader = RadReader(pc_out / "map.rad")
    is_paired = reader.u8()
    ref_count = reader.u64()
    refs = [reader.string() for _ in range(ref_count)]
    num_chunks = reader.u64()
    file_tags = reader.tag_section()
    read_tags = reader.tag_section()
    aln_tags = reader.tag_section()

    file_values = {name: reader.typed_value(type_id) for name, type_id in file_tags}
    read_tag_map = dict(read_tags)
    aln_tag_map = dict(aln_tags)

    if is_paired != 0 or refs != [EXPECTED_TARGET] or num_chunks != 1:
        fail(f"unexpected RAD header: is_paired={is_paired}, refs={refs!r}, num_chunks={num_chunks}")
    if file_values != {"cblen": 16, "ulen": 12}:
        fail(f"unexpected RAD file tags: {file_values!r}")
    if set(read_tag_map) != {"b", "u"} or aln_tag_map != {"compressed_ori_refid": RAD_U32}:
        fail(f"unexpected RAD tag schema: read={read_tags!r}, aln={aln_tags!r}")

    chunk_start = reader.offset
    chunk_size = reader.u32()
    nrec = reader.u32()
    naln = reader.u32()
    bc = reader.typed_value(read_tag_map["b"])
    umi = reader.typed_value(read_tag_map["u"])
    compressed_ref = reader.u32()
    chunk_end = reader.offset

    if chunk_size != chunk_end - chunk_start or reader.offset != len(reader.data):
        fail("RAD chunk size or trailing-byte check failed")
    if nrec != 1 or naln != 1:
        fail(f"unexpected RAD record counts: nrec={nrec}, naln={naln}")
    if decode_packed_sequence(bc, 16) != EXPECTED_BARCODE or decode_packed_sequence(umi, 12) != EXPECTED_UMI:
        fail("RAD barcode or UMI does not match expected raw values")
    is_forward = (compressed_ref & RAD_FORWARD_MASK) != 0
    expected_forward = expected_orientation == "forward"
    if is_forward != expected_forward or (compressed_ref & ~RAD_FORWARD_MASK) != 0:
        fail(f"unexpected RAD compatible target/orientation: {compressed_ref:#x}")


def main(argv: list[str]) -> int:
    if len(argv) not in {3, 4}:
        fail("usage: phase2_quant_verify.py PC_OUT AF_QUANT_ALEVIN_DIR [forward|reverse]")

    expected_orientation = argv[3] if len(argv) == 4 else "forward"
    if expected_orientation not in {"forward", "reverse"}:
        fail(f"unexpected orientation argument: {expected_orientation}")

    verify_rad(Path(argv[1]), expected_orientation)
    alevin_dir = Path(argv[2])
    rows = read_lines(alevin_dir / "quants_mat_rows.txt")
    cols = read_lines(alevin_dir / "quants_mat_cols.txt")
    matrix = matrix_data_lines(alevin_dir / "quants_mat.mtx")

    if rows != [EXPECTED_BARCODE]:
        fail(f"unexpected rows: {rows!r}")
    if cols != [EXPECTED_GENE]:
        fail(f"unexpected columns: {cols!r}")
    if len(matrix) != 2:
        fail(f"expected matrix dimensions plus one entry, saw: {matrix!r}")

    dims = matrix[0].split()
    if dims != ["1", "1", "1"]:
        fail(f"unexpected matrix dimensions: {dims!r}")

    entry = matrix[1].split()
    if len(entry) != 3 or entry[0] != "1" or entry[1] != "1" or float(entry[2]) != 1.0:
        fail(f"unexpected matrix entry: {entry!r}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
