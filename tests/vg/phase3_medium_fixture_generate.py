#!/usr/bin/env python3

from pathlib import Path
import shutil
import sys


BARCODE = "AAACCCAAGTTTGGGA"
READ_SEQ = "A" * 20
READ_QUAL = "I" * 20
TOTAL_GROUPS = 50_000


def write_text(path: Path, contents: str) -> None:
    path.write_text(contents)


def umi_for(index: int) -> str:
    alphabet = "ACGT"
    value = index
    bases = []
    for _ in range(12):
        bases.append(alphabet[value & 0x3])
        value >>= 2
    return "".join(reversed(bases))


def read_name(index: int) -> str:
    return f"medium{index:05d}_{BARCODE}_{umi_for(index)}"


def gamp_record(name: str, node_id: int, offset: int, is_reverse: bool, score: int) -> str:
    reverse = "true" if is_reverse else "false"
    return (
        f'{{"name":"{name}","start":[0],"subpath":[{{"path":{{"mapping":[{{"position":'
        f'{{"node_id":{node_id},"offset":{offset},"is_reverse":{reverse}}},"edit":'
        f'[{{"from_length":20,"to_length":20}}],"rank":1}}]}},"score":{score}}}]}}\n'
    )


def expected_row(name: str, targets: list[str], dirs: list[str], case: str) -> str:
    _, barcode, umi = name.rsplit("_", 2)
    return f"{name}\t{barcode}\t{umi}\t{','.join(targets)}\t{','.join(dirs)}\t{case}\n"


def non_emitted_row(name: str, reason: str, case: str) -> str:
    return f"{name}\t{reason}\t{case}\n"


def main(argv: list[str]) -> int:
    if len(argv) != 2:
        raise SystemExit("usage: phase3_medium_fixture_generate.py WORK_DIR")

    work_dir = Path(argv[1])
    if work_dir.exists():
        shutil.rmtree(work_dir)
    work_dir.mkdir(parents=True)

    sequence = "A" * 400
    write_text(
        work_dir / "graph.gfa",
        "\n".join(
            [
                "H\tVN:Z:1.0",
                f"S\t1\t{sequence}",
                f"S\t2\t{sequence}",
                "P\tchrA\t1+\t400M",
                "P\tchrB\t2+\t400M",
                "",
            ]
        ),
    )

    write_text(
        work_dir / "annotation.gtf",
        "\n".join(
            [
                'chrA\tpanCollapse\texon\t301\t350\t.\t+\t.\tgene_id "GENE_A"; transcript_id "SRC_A";',
                'chrA\tpanCollapse\texon\t321\t370\t.\t+\t.\tgene_id "GENE_A_ALT"; transcript_id "SRC_A_ALT";',
                'chrB\tpanCollapse\texon\t301\t350\t.\t+\t.\tgene_id "GENE_B"; transcript_id "SRC_B";',
                "",
            ]
        ),
    )

    write_text(
        work_dir / "collapse.tsv",
        "\n".join(
            [
                "source_path_name\tsource_transcript_id\tcanonical_transcript_id\tcanonical_gene_id",
                "chrA\tSRC_A\tTX_A\tGENE_A",
                "chrA\tSRC_A_ALT\tTX_A_ALT\tGENE_A_ALT",
                "chrB\tSRC_B\tTX_B\tGENE_B",
                "",
            ]
        ),
    )

    gamp_records: list[str] = []
    fastq_records: list[str] = []
    expected_records: list[str] = [
        "read_name\tbarcode\tumi\ttargets\tdirs\tcase\n",
    ]
    expected_non_emitted: list[str] = [
        "read_name\treason\tcase\n",
    ]

    input_records = 0
    emitted_groups = 0
    mixed_orientation_dropped_groups = 0
    no_compatible_transcript_groups = 0
    score_removed_targets = 0

    for index in range(TOTAL_GROUPS):
        name = read_name(index)
        fastq_records.append(f"@{name}\n{READ_SEQ}\n+\n{READ_QUAL}\n")
        bucket = index % 10

        if bucket < 4:
            case = "shared-overlap-forward"
            gamp_records.append(gamp_record(name, 1, 310, False, 40))
            expected_records.append(expected_row(name, ["TX_A", "TX_A_ALT"], ["fw", "fw"], case))
            input_records += 1
            emitted_groups += 1
        elif bucket < 6:
            case = "shared-overlap-reverse"
            gamp_records.append(gamp_record(name, 1, 70, True, 40))
            expected_records.append(expected_row(name, ["TX_A", "TX_A_ALT"], ["rc", "rc"], case))
            input_records += 1
            emitted_groups += 1
        elif bucket < 8:
            case = "score-window-multitarget"
            gamp_records.append(gamp_record(name, 1, 310, False, 40))
            gamp_records.append(gamp_record(name, 2, 70, True, 37))
            expected_records.append(expected_row(name, ["TX_A", "TX_A_ALT", "TX_B"], ["fw", "fw", "rc"], case))
            input_records += 2
            emitted_groups += 1
        elif bucket == 8:
            case = "top-score-retained-lower-removed"
            gamp_records.append(gamp_record(name, 1, 310, False, 40))
            gamp_records.append(gamp_record(name, 2, 310, False, 30))
            expected_records.append(expected_row(name, ["TX_A", "TX_A_ALT"], ["fw", "fw"], case))
            input_records += 2
            emitted_groups += 1
            score_removed_targets += 1
        elif index % 20 == 9:
            case = "mixed-orientation-drop"
            gamp_records.append(gamp_record(name, 1, 310, False, 40))
            gamp_records.append(gamp_record(name, 1, 70, True, 40))
            expected_non_emitted.append(non_emitted_row(name, "mixed_orientation", case))
            input_records += 2
            mixed_orientation_dropped_groups += 1
        else:
            case = "no-compatible-target"
            gamp_records.append(gamp_record(name, 1, 10, False, 40))
            expected_non_emitted.append(non_emitted_row(name, "no_compatible_transcript", case))
            input_records += 1
            no_compatible_transcript_groups += 1

    write_text(work_dir / "reads.fastq", "".join(fastq_records))
    write_text(work_dir / "reads.gamp.json", "".join(gamp_records))
    write_text(work_dir / "expected_rad_records.tsv", "".join(expected_records))
    write_text(work_dir / "expected_non_emitted.tsv", "".join(expected_non_emitted))
    write_text(
        work_dir / "expected_summary.tsv",
        "\n".join(
            [
                f"input_records\t{input_records}",
                f"input_read_groups\t{TOTAL_GROUPS}",
                f"emitted_groups\t{emitted_groups}",
                f"mixed_orientation_dropped_groups\t{mixed_orientation_dropped_groups}",
                f"no_compatible_transcript_groups\t{no_compatible_transcript_groups}",
                "unaligned_reads\t0",
                "raw_molecule_missing_groups\t0",
                "raw_molecule_malformed_groups\t0",
                "raw_molecule_unsupported_groups\t0",
                "raw_molecule_skipped_groups\t0",
                f"score_removed_targets\t{score_removed_targets}",
                "traversal_cap_exceeded_groups\t0",
                "grouping_recurrence_failures\t0",
                "",
            ]
        ),
    )

    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
