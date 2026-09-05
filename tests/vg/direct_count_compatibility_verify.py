import hashlib
import json
import sys
from pathlib import Path

import pyarrow.parquet as pq


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        while block := handle.read(1 << 20):
            digest.update(block)
    return digest.hexdigest()


def canonical_sha256(value: object) -> str:
    encoded = json.dumps(
        value, ensure_ascii=True, sort_keys=True, separators=(",", ":")
    ).encode()
    return hashlib.sha256(encoded).hexdigest()


def load_count(root: Path) -> dict:
    manifest = json.loads((root / "manifest.json").read_text())
    assert manifest["schema"] == "pancollapse-count-output-v1"
    return manifest


def biological_counters(manifest: dict) -> dict:
    global_counters = {
        key: value
        for key, value in manifest["counters"]["global"].items()
        if not key.startswith("assignment_cache_")
    }
    return {
        "global": global_counters,
        "profiles": manifest["counters"]["profiles"],
    }


def profile_logical(manifest: dict) -> dict:
    return {
        profile["id"]: profile["logical_tables"]
        for profile in manifest["profiles"]
    }


def table_logical(manifest: dict) -> dict:
    return {
        name: (identity["rows"], identity["logical_sha256"])
        for name, identity in manifest["tables"].items()
    }


def validate_count_pair(
    source_root: Path, replay_root: Path, second_mode: str = "replayed"
) -> None:
    source = load_count(source_root)
    replay = load_count(replay_root)
    assert source["compatibility_bundle"]["mode"] == "produced"
    assert replay["compatibility_bundle"]["mode"] == second_mode
    assert (
        source["compatibility_bundle"]["content_id"]
        == replay["compatibility_bundle"]["content_id"]
    )
    for count_manifest in (source, replay):
        compatibility = count_manifest["compatibility_bundle"]
        path = Path(compatibility["manifest_path"])
        assert path.stat().st_size == compatibility["manifest_size_bytes"]
        assert sha256(path) == compatibility["manifest_sha256"]
    assert table_logical(source) == table_logical(replay)
    assert profile_logical(source) == profile_logical(replay)
    assert biological_counters(source) == biological_counters(replay)
    for relative in (
        "parquet/counts.parquet",
        "parquet/molecules.parquet",
        "parquet/barcodes.parquet",
        "parquet/features.parquet",
        "diagnostics/read_assignments.parquet",
    ):
        assert sha256(source_root / relative) == sha256(replay_root / relative)
    source_outputs = {
        key: value["output_sha256"]
        for key, value in source["outputs"].items()
        if key != "summary"
    }
    replay_outputs = {
        key: value["output_sha256"]
        for key, value in replay["outputs"].items()
        if key != "summary"
    }
    assert source_outputs == replay_outputs


def validate_compatibility(root: Path) -> None:
    manifest = json.loads((root / "manifest.json").read_text())
    assert manifest["schema"] == "pancollapse-read-compatibility-v1"
    assert manifest["content_id"] == "sha256:" + canonical_sha256(
        manifest["content"]
    )
    content = manifest["content"]
    assert content["compatibility_algorithm_id"] == (
        "pancollapse-ex50pas-compatibility-v1"
    )
    assert content["input_records"] == 11
    assert content["input_read_groups"] == 9
    assert content["molecule_status_counts"] == {
        "malformed": 0,
        "missing": 0,
        "unsupported": 0,
        "valid": 9,
    }
    assert [item["role"] for item in content["inputs"]] == ["gamp", "xg"]
    tables = {row["path"]: row for row in content["tables"]}
    assert set(tables) == {
        "parquet/exact_facts.parquet",
        "parquet/fact_sets.parquet",
        "parquet/read_rows.parquet",
        "parquet/structural_facts.parquet",
    }
    for relative, identity in tables.items():
        path = root / relative
        assert path.stat().st_size == identity["size_bytes"]
        assert sha256(path) == identity["sha256"]
        assert pq.read_metadata(path).num_rows == identity["rows"]

    reads = pq.read_table(root / "parquet/read_rows.parquet").to_pylist()
    facts = pq.read_table(root / "parquet/fact_sets.parquet").to_pylist()
    exact = pq.read_table(root / "parquet/exact_facts.parquet").to_pylist()
    structural = pq.read_table(
        root / "parquet/structural_facts.parquet"
    ).to_pylist()
    assert len(reads) == 9
    assert [row["ordinal"] for row in reads] == list(range(9))
    assert [row["fact_set_id"] for row in facts] == list(range(len(facts)))
    assert all(0 <= row["fact_set_id"] < len(facts) for row in reads)
    assert {row["tier"] for row in exact} == {"E", "P", "B"}
    assert {row["layer"] for row in structural} == {"S", "U"}


def validate_denominator_compatibility(root: Path) -> None:
    manifest = json.loads((root / "manifest.json").read_text())
    content = manifest["content"]
    assert content["input_records"] == 2
    assert content["input_read_groups"] == 2
    assert content["molecule_status_counts"] == {
        "malformed": 1,
        "missing": 0,
        "unsupported": 0,
        "valid": 1,
    }
    reads = pq.read_table(root / "parquet/read_rows.parquet").to_pylist()
    exact = pq.read_table(root / "parquet/exact_facts.parquet").to_pylist()
    structural = pq.read_table(
        root / "parquet/structural_facts.parquet"
    ).to_pylist()
    assert [(row["original_name"], row["molecule_status"]) for row in reads] == [
        ("badname", "malformed"),
        ("unaligned", "valid"),
    ]
    evidence_ids = {
        row["fact_set_id"] for row in exact + structural
    }
    assert reads[0]["fact_set_id"] in evidence_ids
    assert reads[1]["fact_set_id"] not in evidence_ids


def assert_compatibility_bytes_equal(left: Path, right: Path) -> None:
    for relative in (
        "manifest.json",
        "parquet/read_rows.parquet",
        "parquet/fact_sets.parquet",
        "parquet/exact_facts.parquet",
        "parquet/structural_facts.parquet",
    ):
        assert sha256(left / relative) == sha256(right / relative)


def main() -> None:
    if len(sys.argv) != 14:
        raise SystemExit(
            "usage: verify.py COMPAT SOURCE REPLAY DIRECT_OVERRIDE REPLAY_OVERRIDE "
            "ONE_COMPAT ONE_SOURCE TAGGED_COMPAT TAGGED_SOURCE TAGGED_REPLAY "
            "DENOM_COMPAT DENOM_SOURCE DENOM_REPLAY"
        )
    (
        compatibility,
        source,
        replay,
        direct_override,
        replay_override,
        one_compatibility,
        one_source,
        tagged_compatibility,
        tagged_source,
        tagged_replay,
        denominator_compatibility,
        denominator_source,
        denominator_replay,
    ) = map(
        Path, sys.argv[1:]
    )
    validate_compatibility(compatibility)
    validate_compatibility(one_compatibility)
    validate_compatibility(tagged_compatibility)
    assert_compatibility_bytes_equal(compatibility, one_compatibility)
    assert_compatibility_bytes_equal(compatibility, tagged_compatibility)
    validate_count_pair(source, replay)
    validate_count_pair(source, one_source, "produced")
    validate_count_pair(tagged_source, tagged_replay)
    base_profiles = profile_logical(load_count(source))
    tagged_profiles = profile_logical(load_count(tagged_source))
    assert base_profiles["cr7-v1"]["counts"]["rows"] == 1
    assert tagged_profiles["cr7-v1"]["counts"]["rows"] == 0
    assert (
        base_profiles["pansc-strict-v1"]
        == tagged_profiles["pansc-strict-v1"]
    )

    direct = load_count(direct_override)
    replayed = load_count(replay_override)
    assert table_logical(direct) == table_logical(replayed)
    assert profile_logical(direct) == profile_logical(replayed)
    assert biological_counters(direct) == biological_counters(replayed)

    validate_denominator_compatibility(denominator_compatibility)
    validate_count_pair(denominator_source, denominator_replay)
    denominator_count = load_count(denominator_source)
    assert denominator_count["counters"]["global"]["input_read_groups"] == 2
    assert denominator_count["counters"]["global"][
        "raw_molecule_malformed_groups"
    ] == 1
    assert denominator_count["counters"]["global"][
        "raw_molecule_skipped_groups"
    ] == 1


if __name__ == "__main__":
    main()
