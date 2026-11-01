#!/usr/bin/env python3
"""D110: native binding, fixed node-score goldens, and count invariants."""

from __future__ import annotations

import csv
import copy
import json
import subprocess
import sys
import tempfile
from pathlib import Path

import pyarrow.parquet as pq

from count_test_support import assert_count_outputs, assert_profile_output, run, verify_alignment_cutoff
from disjoint_body_verify import ledger_row, make_inputs


def scientific_counters(output: Path) -> dict[tuple[str, ...], str]:
    with (output / "summary.tsv").open() as stream:
        rows = csv.reader(stream, delimiter="\t")
        next(rows)
        return {tuple(row[:3]): row[3] for row in rows
                if not row[2].startswith(("assignment_cache_", "aggregate_spill_"))}


def native_paths(vg: Path, gfa: Path, root: Path) -> tuple[Path, Path, Path, Path]:
    pg, xg, gbwt, ri = (root / name for name in ("graph.pg", "graph.xg", "paths.gbwt", "paths.ri"))
    for flag, destination in (("-p", pg), ("-x", xg)):
        with destination.open("wb") as output:
            subprocess.run([str(vg), "convert", "-g", str(gfa), flag], stdout=output, check=True)
    run([vg, "gbwt", "--num-threads", "2", "--num-jobs", "2", "-E", "-x", pg, "-o", gbwt])
    run([vg, "gbwt", "-r", ri, gbwt])
    # Existing receipt values are trusted, never re-hashed in the new route.
    # Zero digests make accidental strict legacy hashing fail this fixture.
    binding = root / "binding.json"
    binding.write_text(json.dumps({"schema": "pancollapse-r-index-binding-v1",
        "gbwt_sha256": "0" * 64, "gbwt_size_bytes": gbwt.stat().st_size,
        "r_index_sha256": "0" * 64, "r_index_size_bytes": ri.stat().st_size}))
    return xg, gbwt, ri, binding


def check_counts(reference: Path, candidate: Path) -> None:
    assert_count_outputs(reference, candidate)
    assert scientific_counters(candidate) == scientific_counters(reference), (
        scientific_counters(candidate), scientific_counters(reference))


def main() -> None:
    program, vg, generated, source, work = map(Path, sys.argv[1:])
    work.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix="native-", dir=work))
    xg, gbwt, ri, binding = native_paths(vg, source / "graph.gfa", root)
    bundle = generated / "count_bundle"
    index = root / "native.pci"
    build = run([program, "index", "--count-bundle", bundle, "--gbwt", gbwt,
                 "--threads", "4", "--out", index])
    assert index.read_bytes().startswith(b"PCANBND1"), build.stdout
    serial = root / "native-serial.pci"
    run([program, "index", "--count-bundle", bundle, "--gbwt", gbwt,
         "--threads", "1", "--out", serial])
    assert serial.read_bytes() == index.read_bytes(), "binding bytes depend on threads"
    bound = index
    index = root / "direct.pci"
    compile_args = [program, "index", "--from-annotation-index", bound,
                    "--xg", xg, "--gbwt", gbwt, "--r-index", ri,
                    "--r-index-manifest", binding, "--memory-budget", "1GiB"]
    run(compile_args + ["--threads", "4", "--out", index])
    direct_serial = root / "direct-serial.pci"
    run(compile_args + ["--threads", "1", "--out", direct_serial])
    assert index.read_bytes() == direct_serial.read_bytes(), "direct index bytes depend on threads"
    assert int.from_bytes(index.read_bytes()[8:12], "little") == 2, "direct index is not compact V2"
    destination = root / "unsupported-direct-source.pci"
    run([program, "index", "--from-annotation-index", index,
         "--out", destination], succeeds=False)
    assert not destination.exists(), "index accepted a direct index as a preparation source"
    # Counting must work with every compiler-only graph input unavailable.
    for path in (xg, gbwt, ri, binding, bound):
        path.rename(path.with_name(path.name + ".compiler-only"))
    native = [program, "count", "--gamp", generated / "reads.gamp",
              "--annotation-index", index, "--barcode-whitelist", source / "count_whitelist.txt",
              "--10x-mex"]
    node_score = native
    node_baseline = root / "node-score-1"
    for threads in (1, 2, 4, 8, 16, 128):
        output = root / f"node-score-{threads}"
        run(node_score + ["--cr7", "--pansc-strict-v1", "--threads", threads, "--out-dir", output])
        manifest = json.loads((output / "manifest.json").read_text())
        assert manifest["assignment_algorithm"] == "node-score-v1"
        assert manifest["assignment_score_window"] == 5
        if threads != 1:
            check_counts(node_baseline, output)
    for flag, profile in (("--cr7", "cr7-v1"), ("--pansc-strict-v1", "pansc-strict-v1")):
        output = root / f"node-score-{profile}"
        run(node_score + [flag, "--threads", "4", "--out-dir", output])
        assert_profile_output(node_baseline, output, profile)
    # Cache capacity changes retention only, never assignment or molecule rules.
    for label, limit in (("tiny", "1B"), ("generous", "64MiB")):
        output = root / f"cache-limits-{label}"
        arguments = ["--memory-budget", "1GiB"]
        arguments += ["--cache-limit", f"assignment={limit}"]
        run(node_score + ["--cr7", "--pansc-strict-v1", "--threads", "4",
                          *arguments, "--out-dir", output])
        check_counts(node_baseline, output)
        memory_plan = json.loads((output / "manifest.json").read_text())["memory_plan"]
        expected_limit = 1 if label == "tiny" else 64 * 1024 * 1024
        assert memory_plan["cache_limits"] == {
            "node": 0, "edge": 0, "assignment": expected_limit, "splice": 0}, memory_plan
        assert sum(memory_plan["cache_limits"].values()) <= memory_plan["cache_pool_bytes"]
    verify_alignment_cutoff(node_score, vg,
                            json.loads((source / "reads.gamp.json").read_text().splitlines()[0]),
                            root / "node-score-alignment-cutoff")
    forbidden = ("prepare_paths", "path_position", "exact_parent_models", "splice_geometry",
                 "identity_ledger_begin", "streamed_steps", "fastlocate_build")
    preload = run(native + ["--cr7", "--preload-only", "--out-dir", root / "preload"])
    assert not any(marker in preload.stderr.lower() for marker in forbidden), preload.stderr
    assert "reference_mode=direct-facts" in preload.stderr, preload.stderr
    assert "native-query-stats" not in preload.stderr, preload.stderr
    frozen = {
        "cr7-v1": {
            "counts": {"rows": 1, "sha256": "02f9a0f2a4f84dc038ae9f2b885918eb59e48b433f38f400e2af961b87f30df4"},
            "molecules": {"rows": 8, "sha256": "244a303e278b434a5d490eda72270c4010f7c7d62d6e49e43f56f2514753676f"}},
        "pansc-strict-v1": {
            "counts": {"rows": 1, "sha256": "2a51d17dcdb47ef4f1fd5039d304578b832e16f195c4b99d6d0634d9239a3bc6"},
            "molecules": {"rows": 8, "sha256": "a272547eff692933c0866b3ad28d76a894f35879ba652bfcfed8337c4569f238"}}}
    # Captured before legacy retirement from the frozen node-score fixture;
    # these expectations must not be regenerated by the implementation under test.
    manifest = json.loads((node_baseline / "manifest.json").read_text())
    assert {p["id"]: p["logical_tables"] for p in manifest["profiles"]} == frozen
    for role in ("xg", "gbwt", "r_index"):
        assert role not in manifest["inputs"], manifest["inputs"]
    derived_outputs = []
    for label, threads in (("serial", 1), ("parallel", 4)):
        base = native
        out = root / f"derived-reverse-{label}"
        run(base + ["--cr7", "--analysis-scope", "sensitivity-analysis",
                    "--profile-override", "cr7-v1:strand=reverse",
                    "--threads", threads, "--out-dir", out])
        derived_outputs.append(out)
    check_counts(*derived_outputs)

    # Native assignment caching is graph-evidence-only. Reuse identical graph
    # records while varying every molecule field that reaches the frozen CB/UMI
    # stage, then require exact thread-invariant biological output.
    # The four groups exercise exact, opposite-strand, no-evidence, and
    # uncorrectable-barcode paths rather than only an assignable happy path.
    fixture_records = [json.loads(line) for line in
                       (source / "reads.gamp.json").read_text().splitlines()]
    by_name: dict[str, list[dict]] = {}
    for record in fixture_records:
        by_name.setdefault(record["name"], []).append(record)
    valid_barcodes = (
        "AAACCCAAGTTTGGGA", "AAACCCAAGTTTGGGC", "AAACCCAAGTTTGGGT", "AAACCCAAGTTTGGGG",
    )
    cache_whitelist = root / "cache-whitelist.txt"
    cache_whitelist.write_text("".join(f"{barcode}\n" for barcode in sorted(valid_barcodes)))
    umis = (
        "ACGTACGTACGT", "TGCATGCATGCA", "CAGTCAGTCAGT", "GTACGTACGTAC",
        "ACTGACTGACTG", "GTCAGTCAGTCA", "CATGCATGCATG",
    )
    cache_records: list[dict] = []
    serial = 0

    def molecule_name(case: str, barcode: str) -> str:
        nonlocal serial
        quality = chr(33 + serial)
        # Keep the quality suffixes distinct and parseable without placing any
        # CB/UMI content into the graph-evidence cache signature.
        barcode_quality = f"{ord(quality):02x}" * 16
        umi_quality = f"{ord(quality):02x}" * 12
        name = (f"native-cache-{case}-{serial}_{barcode}_{umis[serial]}_"
                f"cy{barcode_quality}_uy{umi_quality}")
        serial += 1
        return name

    def append_clone(case: str, records: list[dict], barcode: str) -> None:
        name = molecule_name(case, barcode)
        for record in records:
            clone = copy.deepcopy(record)
            clone["name"] = name
            cache_records.append(clone)

    append_clone("exact-a", by_name[next(name for name in by_name if name.startswith("concord_"))],
                 valid_barcodes[0])
    append_clone("exact-b", by_name[next(name for name in by_name if name.startswith("concord_"))],
                 valid_barcodes[1])
    append_clone("wrongstrand-a", by_name[next(name for name in by_name if name.startswith("discordrev_"))],
                 valid_barcodes[2])
    append_clone("wrongstrand-b", by_name[next(name for name in by_name if name.startswith("discordrev_"))],
                 valid_barcodes[3])
    featureless = [{"sequence": "AAAAAAAAAA", "start": []}]
    append_clone("featureless-a", featureless, valid_barcodes[0])
    append_clone("featureless-b", featureless, valid_barcodes[1])
    append_clone("incorrectable", by_name[next(name for name in by_name if name.startswith("concord_"))],
                 "CCACCCAAGTTTGGGA")
    cache_json, cache_gamp = root / "cache-evidence.json", root / "cache-evidence.gamp"
    cache_json.write_text("".join(json.dumps(record) + "\n" for record in cache_records))
    with cache_gamp.open("wb") as output:
        subprocess.run([str(vg), "view", "-J", "-K", "-k", str(cache_json)],
                       stdout=output, check=True)
    cache_outputs = []
    for label, threads in (("serial", 1), ("parallel", 4)):
        base = native
        command = base.copy()
        command[command.index("--gamp") + 1] = cache_gamp
        command[command.index("--barcode-whitelist") + 1] = cache_whitelist
        out = root / f"cache-evidence-{label}"
        run(command + ["--cr7", "--pansc-strict-v1", "--threads", "4", "--out-dir", out])
        cache_outputs.append(out)
    check_counts(*cache_outputs)
    cache_manifest = json.loads((cache_outputs[1] / "manifest.json").read_text())
    assert cache_manifest["assignment_cache"]["hits"] > 0, cache_manifest["assignment_cache"]
    for profile in ("cr7-v1", "pansc-strict-v1"):
        terminals = cache_manifest["counters"]["profiles"][profile]
        assert terminals["assigned_unique"] > 0, terminals
        assert terminals["unassigned_no_evidence"] > 0, terminals
    assert cache_manifest["counters"]["global"]["noncountable_molecule_groups"] == 1, cache_manifest
    assert cache_manifest["counters"]["global"]["off_whitelist_uncorrectable_dropped"] == 0, cache_manifest

    # Direction-vetoed reads never become molecules through barcode repair.
    # Counts are the contract; rejected-shadow correction counters are not.
    records = [json.loads(line) for line in (source / "reads.gamp.json").read_text().splitlines()]
    antisense = next(record for record in records if record["name"].startswith("discordrev_"))
    for prefix, barcode in (("correctable", "CAACCCAAGTTTGGGA"),
                             ("uncorrectable", "CCACCCAAGTTTGGGA")):
        extra = json.loads(json.dumps(antisense))
        extra["name"] = prefix + "_" + extra["name"].replace("AAACCCAAGTTTGGGA", barcode)
        records.append(extra)
    barcode_json, barcode_gamp = root / "barcode-stage.json", root / "barcode-stage.gamp"
    barcode_json.write_text("".join(json.dumps(record) + "\n" for record in records))
    with barcode_gamp.open("wb") as output:
        subprocess.run([str(vg), "view", "-J", "-K", "-k", str(barcode_json)], stdout=output, check=True)
    barcode_outputs = []
    for label, threads in (("serial", 1), ("parallel", 4)):
        base = native
        command = base.copy()
        command[command.index("--gamp") + 1] = barcode_gamp
        out = root / f"barcode-stage-{label}"
        run(command + ["--cr7", "--pansc-strict-v1", "--threads", "4", "--out-dir", out])
        barcode_outputs.append(out)
    check_counts(*barcode_outputs)
    assert_count_outputs(node_baseline, barcode_outputs[1])
    counters = scientific_counters(barcode_outputs[1])
    assert counters[("global", ".", "noncountable_molecule_groups")] == "1", counters
    assert counters[("global", ".", "off_whitelist_uncorrectable_dropped")] == "0", counters

    # Molecules that cannot count never need a graph query, but their exact CBs
    # still contribute to the dataset-wide correction prior. Deliberately use
    # invalid node IDs so accidental scoring cannot make this assertion pass.
    excluded = []
    for label, barcode, umi in (("homopolymer", "AAACCCAAGTTTGGGA", "AAAAAAAAAAAA"),
                                ("umi-n", "AAACCCAAGTTTGGGA", "ACGTACGTACGN"),
                                ("barcode", "CCACCCAAGTTTGGGA", "ACGTACGTACGT")):
        record = copy.deepcopy(fixture_records[0])
        record["name"] = f"early-{label}_{barcode}_{umi}"
        record["subpath"][0]["path"]["mapping"][0]["position"]["node_id"] = "999999999"
        excluded.append(record)
    excluded_json, excluded_gamp = root / "early-excluded.json", root / "early-excluded.gamp"
    excluded_json.write_text("".join(json.dumps(record) + "\n" for record in excluded))
    with excluded_gamp.open("wb") as output:
        subprocess.run([str(vg), "view", "-J", "-K", "-k", str(excluded_json)], stdout=output, check=True)
    command = native.copy()
    command[command.index("--gamp") + 1] = excluded_gamp
    excluded_out = root / "early-excluded"
    excluded_run = run(command + ["--cr7", "--pansc-strict-v1", "--out-dir", excluded_out])
    assert "raw_models=0" in excluded_run.stderr, excluded_run.stderr
    early_counters = scientific_counters(excluded_out)
    assert early_counters[("global", ".", "noncountable_molecule_groups")] == "3", early_counters
    assert early_counters[("global", ".", "barcode_prior_groups")] == "3", early_counters
    assert early_counters[("global", ".", "exact_barcode_prior_groups")] == "2", early_counters
    for table in ("counts", "molecules"):
        assert pq.read_table(excluded_out / "parquet" / f"{table}.parquet").num_rows == 0

    # Empty GAMP must publish valid empty dictionaries/tables, not require a hit.
    empty = root / "empty.gamp"
    empty.write_bytes(b"")
    empty_command = native.copy()
    empty_command[empty_command.index("--gamp") + 1] = empty
    empty_out = root / "empty"
    run(empty_command + ["--cr7", "--pansc-strict-v1", "--out-dir", empty_out])
    assert pq.read_table(empty_out / "parquet/counts.parquet").num_rows == 0
    assert pq.read_table(empty_out / "parquet/molecules.parquet").num_rows == 0

    # Section bounds and native prerequisites fail before opening absent GAMP.
    broken = root / "truncated.pci"
    broken.write_bytes(index.read_bytes()[:128])
    invalid = native.copy()
    invalid[invalid.index("--annotation-index") + 1] = broken
    invalid[invalid.index("--gamp") + 1] = root / "missing.gamp"
    result = run(invalid + ["--cr7", "--out-dir", root / "broken"], succeeds=False)
    assert "cannot open GAMP" not in result.stderr, result.stderr
    for option in ("--xg", "--gbwt", "--r-index", "--r-index-manifest"):
        removed = run(native + [option, root / "missing-compiler-input", "--cr7",
                               "--out-dir", root / option.removeprefix("--")], succeeds=False)
        assert "index-time input" in removed.stderr, removed.stderr

    # PCI3 compilation and disjoint-body intrinsic orientation (both strands).
    case = root / "disjoint"
    case.mkdir()
    gfa, ledger, reads_json, annotation, reference, whitelist = make_inputs(case)
    # A thousand raw alias pairs exercise fanout without quotienting their
    # Parents or manufacturing new body geometry. All share one support sample.
    with gfa.open("a") as graph_out, ledger.open("a") as ledger_out, annotation.open("a") as annotation_out:
        for i in range(1000):
            exon, body = f"ALIAS_EXON_{i:04d}", f"ALIAS_BODY_{i:04d}"
            graph_out.write(f"P\t{exon}\t1+,2+\t*\nP\t{body}\t3+,4+\t*\n")
            for path, layer in ((exon, "exon"), (body, "body")):
                ledger_out.write("\t".join(ledger_row(path, path, "PLUS_GENE", layer, exon, "+")) + "\n")
                annotation_out.write("chr1\tt\ttranscript\t1\t20\t.\t+\t.\t"
                    f"ID={path};Parent=PLUS_GENE;source_gene=PLUS_GENE;"
                    "gene_type=protein_coding;transcript_type=protein_coding\n")
    dxg, dgbwt, dri, dbinding = native_paths(vg, gfa, case)
    gamp = case / "reads.gamp"
    with gamp.open("wb") as output:
        subprocess.run([str(vg), "view", "-J", "-K", "-k", str(reads_json)], stdout=output, check=True)
    pci3, bound = case / "facts.pci", case / "bound.pci"
    run([program, "index", "--annotation", annotation, "--annotation-reference", reference,
         "--path-identity-ledger", ledger, "--out", pci3])
    run([program, "index", "--from-annotation-index", pci3, "--gbwt", dgbwt, "--out", bound])
    direct = case / "direct.pci"
    run([program, "index", "--from-annotation-index", bound, "--xg", dxg,
         "--gbwt", dgbwt, "--r-index", dri, "--r-index-manifest", dbinding,
         "--threads", "4", "--out", direct])
    common = [program, "count", "--gamp", gamp,
              "--barcode-whitelist", whitelist, "--cr7", "--pansc-strict-v1", "--10x-mex"]
    disjoint_outputs = []
    for threads in (1, 4):
        output = case / f"native-{threads}"
        fanout = run(common + ["--annotation-index", direct, "--threads", threads,
                              "--out-dir", output])
        assert "reference_mode=direct-facts" in fanout.stderr, fanout.stderr
        disjoint_outputs.append(output)
    check_counts(*disjoint_outputs)
    print("native_count_verify: native binding, frozen node-score goldens, thread/profile/cutoff checks passed")


if __name__ == "__main__":
    main()
