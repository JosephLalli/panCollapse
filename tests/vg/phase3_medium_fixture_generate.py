#!/usr/bin/env python3

from pathlib import Path
import shutil
import sys


BARCODE = "AAACCCAAGTTTGGGA"
READ_SEQ = "A" * 20
READ_QUAL = "I" * 20
TOTAL_GROUPS = 50_000
BUCKETS = 20


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


def read_name(index: int, umi: str | None = None) -> str:
    return f"medium{index:05d}_{BARCODE}_{umi if umi is not None else umi_for(index)}"


def gamp_record(name: str, node_id: int, offset: int, is_reverse: bool, score: int) -> str:
    reverse = "true" if is_reverse else "false"
    return (
        f'{{"name":"{name}","start":[0],"subpath":[{{"path":{{"mapping":[{{"position":'
        f'{{"node_id":{node_id},"offset":{offset},"is_reverse":{reverse}}},"edit":'
        f'[{{"from_length":20,"to_length":20}}],"rank":1}}]}},"score":{score}}}]}}\n'
    )


def gamp_unaligned(name: str) -> str:
    return f'{{"name":"{name}"}}\n'


def expected_row(name: str, targets: list[str], dirs: list[str], case: str) -> str:
    _, barcode, umi = name.rsplit("_", 2)
    return f"{name}\t{barcode}\t{umi}\t{','.join(targets)}\t{','.join(dirs)}\t{case}\n"


def non_emitted_row(name: str, reason: str, case: str) -> str:
    return f"{name}\t{reason}\t{case}\n"


def main(argv: list[str]) -> int:
    if len(argv) != 2:
        raise SystemExit("usage: phase3_medium_fixture_generate.py WORK_DIR")

    if TOTAL_GROUPS % BUCKETS != 0:
        raise SystemExit("TOTAL_GROUPS must be divisible by BUCKETS")

    work_dir = Path(argv[1])
    if work_dir.exists():
        shutil.rmtree(work_dir)
    work_dir.mkdir(parents=True)

    sequence = "A" * 400
    # chrA/chrB carry the multi-isoform and multi-gene cases; hapA/hapB are two
    # copy/haplotype source paths whose transcripts collapse to one canonical target
    # TX_H, so a single read compatible with both must yield exactly one RAD target.
    write_text(
        work_dir / "graph.gfa",
        "\n".join(
            [
                "H\tVN:Z:1.0",
                f"S\t1\t{sequence}",
                f"S\t2\t{sequence}",
                f"S\t3\t{sequence}",
                f"S\t4\t{sequence}",
                "P\tchrA\t1+\t400M",
                "P\tchrB\t2+\t400M",
                "P\thapA\t3+\t400M",
                "P\thapB\t4+\t400M",
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
                'hapA\tpanCollapse\texon\t301\t350\t.\t+\t.\tgene_id "GENE_H"; transcript_id "SRC_H1";',
                'hapB\tpanCollapse\texon\t301\t350\t.\t+\t.\tgene_id "GENE_H"; transcript_id "SRC_H2";',
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
                "hapA\tSRC_H1\tTX_H\tGENE_H",
                "hapB\tSRC_H2\tTX_H\tGENE_H",
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
    unaligned_reads = 0
    raw_molecule_malformed_groups = 0
    raw_molecule_skipped_groups = 0
    score_removed_targets = 0

    for index in range(TOTAL_GROUPS):
        bucket = index % BUCKETS
        name = read_name(index)
        read_fastq_name = name

        if bucket <= 3:
            # multi-gene: read overlaps two annotated exons on chrA
            case = "shared-overlap-forward"
            gamp_records.append(gamp_record(name, 1, 310, False, 40))
            expected_records.append(expected_row(name, ["TX_A", "TX_A_ALT"], ["fw", "fw"], case))
            input_records += 1
            emitted_groups += 1
        elif bucket <= 5:
            case = "shared-overlap-reverse"
            gamp_records.append(gamp_record(name, 1, 70, True, 40))
            expected_records.append(expected_row(name, ["TX_A", "TX_A_ALT"], ["rc", "rc"], case))
            input_records += 1
            emitted_groups += 1
        elif bucket <= 7:
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
        elif bucket == 9:
            # unique single-target emitted read (chrB carries one transcript)
            case = "unique-forward"
            gamp_records.append(gamp_record(name, 2, 310, False, 40))
            expected_records.append(expected_row(name, ["TX_B"], ["fw"], case))
            input_records += 1
            emitted_groups += 1
        elif bucket <= 12:
            # two source paths collapse to canonical TX_H; the equal-score TX_H
            # contributions must not sum, so the score-38 TX_B companion stays inside
            # the score window and is retained. A summing bug would drop TX_B.
            case = "collapse-haplotype"
            gamp_records.append(gamp_record(name, 3, 310, False, 40))
            gamp_records.append(gamp_record(name, 4, 310, False, 40))
            gamp_records.append(gamp_record(name, 2, 310, False, 38))
            expected_records.append(expected_row(name, ["TX_B", "TX_H"], ["fw", "fw"], case))
            input_records += 3
            emitted_groups += 1
        elif bucket == 13:
            # two source paths collapse to a single canonical target
            case = "collapse-haplotype-unique"
            gamp_records.append(gamp_record(name, 3, 310, False, 40))
            gamp_records.append(gamp_record(name, 4, 310, False, 40))
            expected_records.append(expected_row(name, ["TX_H"], ["fw"], case))
            input_records += 2
            emitted_groups += 1
        elif bucket <= 16:
            case = "no-compatible-target"
            gamp_records.append(gamp_record(name, 1, 10, False, 40))
            expected_non_emitted.append(non_emitted_row(name, "no_compatible_transcript", case))
            input_records += 1
            no_compatible_transcript_groups += 1
        elif bucket == 17:
            case = "mixed-orientation-drop"
            gamp_records.append(gamp_record(name, 1, 310, False, 40))
            gamp_records.append(gamp_record(name, 1, 70, True, 40))
            expected_non_emitted.append(non_emitted_row(name, "mixed_orientation", case))
            input_records += 2
            mixed_orientation_dropped_groups += 1
        elif bucket == 18:
            # GAMP record with no subpaths: unaligned input, counted twice
            case = "unaligned"
            gamp_records.append(gamp_unaligned(name))
            expected_non_emitted.append(non_emitted_row(name, "unaligned", case))
            input_records += 1
            no_compatible_transcript_groups += 1
            unaligned_reads += 1
        else:
            # raw UMI length does not match the configured 12, so the group is skipped
            # for molecule identity before compatibility regardless of its alignment
            case = "molecule-identity-malformed"
            skip_umi = "A" * 11
            name = read_name(index, umi=skip_umi)
            read_fastq_name = name
            gamp_records.append(gamp_record(name, 2, 310, False, 40))
            expected_non_emitted.append(non_emitted_row(name, "raw_molecule_malformed", case))
            input_records += 1
            raw_molecule_malformed_groups += 1
            raw_molecule_skipped_groups += 1

        fastq_records.append(f"@{read_fastq_name}\n{READ_SEQ}\n+\n{READ_QUAL}\n")

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
                f"unaligned_reads\t{unaligned_reads}",
                "raw_molecule_missing_groups\t0",
                f"raw_molecule_malformed_groups\t{raw_molecule_malformed_groups}",
                "raw_molecule_unsupported_groups\t0",
                f"raw_molecule_skipped_groups\t{raw_molecule_skipped_groups}",
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
