# RAD Format Research Brief

**Status: Phase 0 researched. Baseline: alevin-fry v0.15.0 and libradicl
0.13.0.**

## Version baseline

- Local alevin-fry crate:
  `/mnt/ssd/lalli/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/alevin-fry-0.15.0`.
- alevin-fry online tag commit:
  `9f173996f61904d99c0a9dd42fbf383f727c29b7`.
- `Cargo.lock` pins `libradicl 0.13.0`.
- Local libradicl crate:
  `/mnt/ssd/lalli/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/libradicl-0.13.0`.
- libradicl online tag commit:
  `6cd08c35085be81783042694dab4bce33f828def`.

Use this pair as the V1 schema baseline.

## Classic RnaShort RAD schema

The target is classic `RnaShort` RAD. It has no magic bytes and no explicit
format version.

Wire order:

- `is_paired`: `u8`.
- `ref_count`: `u64`.
- For each target: `u16 name_len` followed by name bytes.
- `num_chunks`: `u64`; active panCollapse writing sets this to `0`, which libradicl
  treats as unknown and reads until EOF. A seekable writer may instead backpatch the exact
  final count, but that is not required for the active streaming-to-disk path.
- Three tag sections in order: file, read, alignment.
- File tag values immediately after the three tag sections.
- Chunks.

Tag schema:

- File tags: `cblen:U16`, `ulen:U16`.
- Read tags: `b` and `u` integer barcode/UMI types.
- Alignment tag: `compressed_ori_refid:U32`.
- File values: `cblen` then `ulen`.
- Avoid tags that trigger other RAD record families, including
  `num_barcodes`, `pos`, ATAC tags, and long-read `as`/`start`/`end`.

Each tag section is `u16 num_tags`, followed by tag descriptors: `u16 name_len`, name
bytes, and `u8 type_id`. Array type ID 7 also writes one length type ID byte and one
element type ID byte, but V1 does not need arrays. Integer type IDs are U8=1, U16=2,
U32=3, U64=4, and U128=9.

Record:

- `u32 naln`.
- Encoded cell barcode.
- Encoded UMI.
- `naln` values of `u32 compressed_ori_refid`.

Conceptual read record:

- `bc`: encoded raw cell barcode.
- `umi`: encoded raw UMI.
- `refs`: zero-based target IDs into the RAD header target dictionary.
- `dirs`: a parallel orientation vector; `dirs[i]` describes the orientation of
  `refs[i]`.

`refs` are compatibility target IDs, not genomic coordinates. In panCollapse V1, they are
indices into the lexicographically sorted canonical transcript target dictionary, such as
the ID for `TX_A`. alevin-fry later maps these target IDs to genes or gene states through
the supplied transcript-to-gene map.

Alignment encoding:

- `compressed_ori_refid = ref_id | 0x80000000` for forward.
- `compressed_ori_refid = ref_id` for reverse.
- `ref_id` is zero-based into header `ref_names`.
- Header target count must be less than `2^31`.
- libradicl reads the high bit into `dirs[i]` and the lower 31 bits into `refs[i]`.
- Every emitted `refs[i]` must have a corresponding `dirs[i]`.
- Direction is target-level RAD metadata consumed by alevin-fry's expected-orientation
  filtering. Do not derive it from an arbitrary graph-node orientation.

Barcode and UMI packing:

- Two bits per base.
- Last base is in the least significant bits.
- `A=00`, `C=01`, `G=10`, `T=11`; `N` is treated like `A`.
- Widths follow alevin-fry conversion: length 1..4 -> U8, 5..8 -> U16, 9..16 -> U32,
  17..32 -> U64, and >32 unsupported in V1.
- 16 bp cell barcodes and 10 or 12 bp UMIs fit in U32.
- Raw CB/UMI length mismatch against the configured lengths is a molecule-identity
  failure, skipped by default and fatal under `--molecule-identity-failures fail`.

Chunk:

- `u32 nbytes`, including the 8-byte chunk header.
- `u32 nrec`.
- Then record bytes.
- Header `num_chunks = 0` is valid for unknown chunk count; libradicl reads chunks until
  EOF. If a writer uses a nonzero value, that value is the known chunk count.

## Target dictionary and equivalence classes

- The RAD header `ref_names` is the target dictionary.
- `alevin-fry quant --tg-map` must cover every transcript in the RAD header.
- Sort and deduplicate target IDs per read before writing because transcript
  equivalence classes key on the raw target vector.

## Writer recommendation

Implement a native minimal C++ writer for this fixed `RnaShort` schema. Use
libradicl and alevin-fry as validation oracles instead of importing a mapper or
general writer stack into panCollapse.

The active writer should stream `map.rad` to disk: write the prelude/header with
`num_chunks = 0`, write the three tag sections and file-tag values, then write complete
chunks incrementally. Only the current chunk needs buffering so its `nbytes` and `nrec`
can be emitted in the chunk header. The existing libradicl writer also supports the
conventional seek-and-backpatch path for seekable outputs, but panCollapse does not need
that path for active V1 disk output.

Under D042, V1 encodes retained transcript hits with their preserved target-relative
orientation: forward hits set the high bit and reverse hits leave it unset. Downstream
commands should choose the alevin-fry expected-orientation setting that matches the
experiment. For a forward-only fixture this remains:

```bash
alevin-fry generate-permit-list --expected-ori fw ...
```

Do not use synthetic all-forward orientation to indicate that a panCollapse-side strand
policy accepted the hit; panCollapse does not apply that policy in the active V1 design.

## Primary source citations

- Header and prelude order:
  `/mnt/ssd/lalli/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/libradicl-0.13.0/src/header.rs:173-195`,
  `:220-258`.
- Tag descriptor and section encoding:
  `/mnt/ssd/lalli/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/libradicl-0.13.0/src/rad_types.rs:26-35`,
  `:47-79`, `:118-132`.
- File tags written by alevin-fry convert:
  `/mnt/ssd/lalli/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/alevin-fry-0.15.0/src/convert.rs:280-369`.
- Runtime expectation for `cblen`:
  `/mnt/ssd/lalli/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/alevin-fry-0.15.0/src/utils.rs:296-359`;
  `/mnt/ssd/lalli/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/alevin-fry-0.15.0/src/cellfilter.rs:165-168`;
  `/mnt/ssd/lalli/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/alevin-fry-0.15.0/src/quant.rs:1469-1479`.
- Barcode/UMI packing and width selection:
  `/mnt/ssd/lalli/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/alevin-fry-0.15.0/src/convert.rs:75-94`,
  `:322-343`.
- Record layout and orientation:
  `/mnt/ssd/lalli/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/libradicl-0.13.0/src/record.rs:661-692`,
  `:1007-1012`, `:1042-1060`, `:1064-1088`.
- Conceptual fields `bc`, `umi`, `dirs`, and `refs`:
  `/mnt/ssd/lalli/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/libradicl-0.13.0/src/record.rs:484-488`.
- High-bit direction and lower-31-bit reference split:
  `/mnt/ssd/lalli/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/libradicl-0.13.0/src/utils.rs:15-16`,
  `/mnt/ssd/lalli/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/libradicl-0.13.0/src/record.rs:1054-1060`,
  `:1077-1086`, `:1460-1496`.
- Chunk framing:
  `/mnt/ssd/lalli/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/libradicl-0.13.0/src/chunk.rs:145-157`,
  `:169-224`.
- Unknown chunk-count handling:
  `/mnt/ssd/lalli/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/libradicl-0.13.0/src/header.rs:68-72`,
  `/mnt/ssd/lalli/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/libradicl-0.13.0/src/readers.rs:742-762`,
  `:791-812`.
- Seekable writer backpatching:
  `/mnt/ssd/lalli/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/libradicl-0.13.0/src/writers.rs:135-155`.
- `--expected-ori` CLI mapping:
  `/mnt/ssd/lalli/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/alevin-fry-0.15.0/src/main.rs:93-104`,
  `:288-315`;
  `/mnt/ssd/lalli/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/alevin-fry-0.15.0/src/cellfilter.rs:1295-1300`.

## Tiny conformance test outline

1. Write a temporary RAD input directory containing `map.rad` with:
   - one or two transcripts in the header;
   - one valid 16 bp barcode and 10 or 12 bp UMI;
   - one chunk;
   - sorted, deduplicated transcript hits with explicit forward and reverse orientation
     cases.
2. Decode with `alevin-fry view` or libradicl and assert header tags, file
   values, packed barcode/UMI values, `num_chunks = 0` unknown-count handling, and target
   IDs.
3. Run the installed alevin-fry v0.15.0 pipeline:

```bash
alevin-fry generate-permit-list -i rad-in -d fw -o permit -f 1 -t 1
alevin-fry collate -i permit -r rad-in -t 1
alevin-fry quant -i permit -m tg-map.tsv -o quant -r cr-like -t 1 --dump-eqclasses
```

4. Assert that `tg-map.tsv` covers every header transcript and that the final
   matrix or dumped equivalence classes match the expected molecule.
