#include "annotation_fact_index.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <openssl/evp.h>
#include <utility>

using namespace pancollapse::direct_count;

namespace {
void require(bool ok, const char* what) { if (!ok) throw std::runtime_error(what); }
void put(const std::filesystem::path& path, const std::string& text) {
    std::ofstream file(path, std::ios::binary); file << text;
    require(bool(file), "fixture write failed");
}
std::string get(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), {}};
}
template<class F> void rejects(F fn) {
    try { fn(); } catch (const std::exception&) { return; }
    throw std::runtime_error("invalid index was accepted");
}
std::string rechecksum(std::string bytes) {
    require(bytes.size() >= 64, "cannot checksum short fixture");
    bytes.resize(bytes.size() - 64);
    unsigned char digest[32]; unsigned length = 0;
    require(EVP_Digest(bytes.data(), bytes.size(), digest, &length, EVP_sha256(), nullptr) == 1 && length == 32,
            "fixture digest failed");
    constexpr char hex[] = "0123456789abcdef";
    for (unsigned char c : digest) { bytes += hex[c >> 4]; bytes += hex[c & 15]; }
    return bytes;
}
std::uint64_t little(const std::string& bytes, size_t offset, unsigned width) {
    require(offset + width <= bytes.size(), "prepared fixture offset out of range");
    std::uint64_t value = 0;
    for (unsigned i = 0; i < width; ++i)
        value |= std::uint64_t(static_cast<unsigned char>(bytes[offset + i])) << (8 * i);
    return value;
}
size_t prepared_rows_offset(const std::string& bytes) {
    size_t offset = 8 + 4;
    auto skip_string = [&] { const auto size = little(bytes, offset, 4); offset += 4 + size; };
    skip_string(); offset += 2; skip_string(); skip_string();
    const auto sources = little(bytes, offset, 8); offset += 8;
    for (std::uint64_t i = 0; i < sources; ++i) { skip_string(); skip_string(); offset += 8; }
    offset += 8; // outer row count
    skip_string(); offset += 1;
    const auto dictionary = little(bytes, offset, 8); offset += 8;
    for (std::uint64_t i = 0; i < dictionary; ++i) skip_string();
    offset += 8; // compiled row count
    require(offset <= bytes.size() - 64, "prepared fixture rows exceed payload");
    return offset;
}
}

int main(int argc, char** argv) {
    try {
        static_assert(ParentFactTable::compact_record_bytes() == 24);
        require(argc == 2, "expected real conflict ledger fixture");
        ParentFactTable empty_parents;
        require(empty_parents.begin() == empty_parents.end() &&
                    empty_parents.find("missing") == empty_parents.end(),
                "empty compact Parent table lookup changed");
        rejects([&] { static_cast<void>(empty_parents.parent_by_rank(0)); });
        rejects([&] { static_cast<void>(empty_parents.parent_key(0)); });
        const auto root = std::filesystem::temp_directory_path() /
            ("annotation-index-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directory(root);
        const auto ledger_path = root / "ledger.tsv";
        std::string ledger_bytes = get(argv[1]);
        if (!ledger_bytes.empty() && ledger_bytes.back() == '\n') ledger_bytes.pop_back();
        put(ledger_path, ledger_bytes); // Exercise exact hash without terminal newline.
        auto indexed = read_index_ledger(ledger_path);
        require(indexed.binding.ledger_sha256 == sha256_file(ledger_path), "streaming ledger hash mismatch");
        require(indexed.ledger.qualified_target_count() == 2, "lost qualified source-gene targets");
        std::string annotation = "##gff-version 3\n";
        for (const auto& [parent, identity] : indexed.ledger.identities_by_parent) {
            const std::string gene = "gene_" + parent.str();
            annotation += "chr1\tt\tgene\t1\t100\t.\t+\t.\tID=" + gene + ";gene_type=protein_coding\n";
            annotation += "chr1\tt\ttranscript\t1\t100\t.\t+\t.\tID=" + parent.str() +
                ";Parent=" + gene + ";source_gene=" + identity->source_gene.str() +
                ";gene_type=protein_coding;transcript_type=protein_coding;tag=strong_local_support\n";
        }
        put(root / "a.gff3", annotation);
        put(root / "r.gff3", "##gff-version 3\n");
        const std::vector<ProfileId> profiles{ProfileId::cr7_v1, ProfileId::pansc_strict_v1};
        auto facts = CountFactCatalog::from_annotation(root / "a.gff3", root / "r.gff3",
            indexed.ledger, profiles, AnnotationFormat::gff3, AnnotationFormat::gff3,
            indexed.binding.ledger_sha256);
        auto moved_ledger = std::move(indexed.ledger);
        require(facts.parent_by_rank(0).unique_parent == moved_ledger.identities_by_parent.begin()->first,
                "moving finalized ledger invalidated compact Parent identity pointers");
        require(facts.parents().compact_record_bytes() == 24,
                "Parent catalog row is not the 24-byte compact record");
        require(facts.parents().category_reference_count() == facts.parents().size() &&
                    facts.parents().category_dictionary_size() < facts.parents().size(),
                "Parent rows did not share category dictionary entries");
        for (std::size_t rank = 0; rank < facts.parents().size(); ++rank) {
            const auto& key = facts.parent_key(rank);
            const ParentFact by_rank = facts.parent_by_rank(rank);
            require(key == by_rank.unique_parent, "Parent lexical rank lost its key");
            if (rank != 0) {
                require(facts.parent_key(rank - 1).view() < key.view(),
                        "Parent lexical ranks are not sorted");
            }
            const auto direct = facts.candidate(effective_profile(ProfileId::cr7_v1), key, 100,
                                                EvidenceTier::exon, EvidenceStrand::forward);
            const auto ranked = facts.candidate_by_rank(effective_profile(ProfileId::cr7_v1), rank,
                                                         100, EvidenceTier::exon,
                                                         EvidenceStrand::forward);
            require(direct.gene == ranked.gene &&
                        direct.equivalence_gene == ranked.equivalence_gene &&
                        direct.categories == ranked.categories &&
                        direct.novel_origin_gene == ranked.novel_origin_gene,
                    "ranked Parent candidate changed policy fields");
        }
        CountFactCatalog copied_facts = facts;
        CountFactCatalog moved_facts = std::move(copied_facts);
        require(moved_facts.parent_key(0) == facts.parent_key(0) &&
                    moved_facts.parent_by_rank(0).canonical_transcript ==
                        facts.parent_by_rank(0).canonical_transcript,
                "catalog copy or move invalidated stable ledger identities");
        const auto first = root / "first.pci", second = root / "second.pci";
        facts.write_annotation_index(first, moved_ledger, indexed.binding);
        facts.write_annotation_index(second, moved_ledger, indexed.binding);
        const auto bytes = get(first);
        require(bytes == get(second), "index bytes are nondeterministic");
        rejects([&] { facts.write_annotation_index(first, moved_ledger, indexed.binding); });
        require(get(first) == bytes, "overwrite refusal changed original");
        std::filesystem::remove(root / "a.gff3");
        std::filesystem::remove(root / "r.gff3");
        auto loaded = CountFactCatalog::load_annotation_index(first, moved_ledger, indexed.binding, profiles);
        const auto loaded_hash = loaded.annotation_index_file_sha256();
        require(loaded_hash == sha256_file(first) && loaded.annotation_index_file_size() == bytes.size(),
                "streamed whole-index identity mismatch");
        const auto original_inode = root / "loaded-inode.pci";
        std::filesystem::rename(first, original_inode);
        put(first, "a replacement must not change the identity of loaded bytes");
        require(loaded.annotation_index_file_sha256() == loaded_hash && loaded.annotation_index_file_size() == bytes.size(),
                "loaded artifact identity followed a replaced pathname");
        std::filesystem::remove(first);
        std::filesystem::rename(original_inode, first);
        const auto roundtrip = root / "roundtrip.pci";
        loaded.write_annotation_index(roundtrip, moved_ledger, indexed.binding);
        require(get(roundtrip) == bytes,
                "PCANIDX2 raw/index roundtrip changed existing bytes");
        require(loaded.content_id() == facts.content_id() && loaded.authority() == facts.authority(), "source identity changed");
        require(loaded.parents().size() == facts.parents().size(), "Parent count changed");
        require(loaded.parents().compact_record_bytes() == 24 &&
                    loaded.parents().category_dictionary_size() ==
                        facts.parents().category_dictionary_size(),
                "loaded index did not retain compact shared Parent storage");
        for (const auto [parent, expected] : facts.parents()) {
            const auto actual = loaded.parent(parent);
            require(actual.count_gene == expected.count_gene && actual.raw_categories == expected.raw_categories &&
                    actual.runtime_gene == expected.runtime_gene && actual.canonical_transcript == expected.canonical_transcript &&
                    actual.feature_layer == expected.feature_layer && actual.sample == expected.sample &&
                    actual.strong_local_support == expected.strong_local_support &&
                    actual.protected_primary_protein_nested_host == expected.protected_primary_protein_nested_host &&
                    actual.novel_paralog == expected.novel_paralog && actual.novel_origin_gene == expected.novel_origin_gene,
                    "Parent fact roundtrip changed");
            for (auto id : profiles) {
                const auto a = facts.candidate(id, parent, 100, EvidenceTier::exon, EvidenceStrand::forward);
                const auto b = loaded.candidate(id, parent, 100, EvidenceTier::exon, EvidenceStrand::forward);
                require(a.gene == b.gene && a.competition == b.competition && a.categories == b.categories &&
                    a.gene_types == b.gene_types && a.equivalence_gene == b.equivalence_gene && a.nested_host == b.nested_host,
                    "candidate roundtrip changed");
            }
        }
        const auto prepared_path = root / "prepared.pci";
        facts.write_prepared_annotation_index(prepared_path, moved_ledger, indexed.binding);
        auto prepared = CountFactCatalog::load_prepared_annotation_index(prepared_path, profiles);
        const auto prepared_bytes = get(prepared_path);
        // The trusted PCI3 route keeps the complete parser/framing checks but
        // deliberately does not hash its multi-gigabyte payload a second time.
        // A syntactically valid, wrong trailer therefore remains strict-only.
        auto wrong_prepared_trailer = prepared_bytes;
        wrong_prepared_trailer.back() = wrong_prepared_trailer.back() == '0' ? '1' : '0';
        put(second, wrong_prepared_trailer);
        rejects([&] { CountFactCatalog::load_prepared_annotation_index(second, profiles); });
        auto trusted = CountFactCatalog::load_prepared_annotation_index_trusted(second, profiles);
        require(trusted.facts.annotation_index_file_sha256().empty() &&
                    trusted.ledger.rows_by_path.size() == moved_ledger.rows_by_path.size(),
                "trusted PCI3 route unexpectedly hashed or skipped parsing");
        auto malformed_trusted = wrong_prepared_trailer;
        malformed_trusted.front() ^= 1;
        put(second, malformed_trusted);
        rejects([&] { CountFactCatalog::load_prepared_annotation_index_trusted(second, profiles); });
        auto moved_prepared = std::move(prepared);
        require(moved_prepared.binding.ledger_sha256 == indexed.binding.ledger_sha256 &&
                    moved_prepared.binding.ledger_size == indexed.binding.ledger_size,
                "prepared index lost ledger binding");
        require(moved_prepared.ledger.qualified_target_count() == moved_ledger.qualified_target_count() &&
                    moved_prepared.ledger.rows_by_path.size() == moved_ledger.rows_by_path.size() &&
                    moved_prepared.ledger.identities_by_parent.size() == moved_ledger.identities_by_parent.size(),
                "prepared index lost ledger cardinalities");
        for (const auto& [path, row] : moved_ledger.rows_by_path) {
            const auto restored = moved_prepared.ledger.rows_by_path.find(path);
            require(restored != moved_prepared.ledger.rows_by_path.end() &&
                        restored->second.vg_path_length == row.vg_path_length &&
                        moved_prepared.ledger.target_transcript(restored->second.annotation) ==
                            moved_ledger.target_transcript(row.annotation),
                    "prepared index changed path target identity");
        }
        for (const auto& [parent, identity] : moved_prepared.ledger.identities_by_parent) {
            require(identity->unique_parent == parent,
                    "prepared Parent pointer no longer identifies its Parent");
            const auto actual = moved_prepared.facts.parent(parent);
            require(actual.unique_parent == parent && actual.runtime_gene == identity->gene_id,
                    "prepared facts no longer bind reconstructed Parent identity");
        }
        for (size_t size : {size_t(0), size_t(8), prepared_bytes.size()-1}) {
            put(second, prepared_bytes.substr(0, size));
            rejects([&] { CountFactCatalog::load_prepared_annotation_index(second, profiles); });
        }
        for (size_t offset : {size_t(0), size_t(8), prepared_bytes.size()-1}) {
            auto corrupt = prepared_bytes; corrupt[offset] ^= 1; put(second, corrupt);
            rejects([&] { CountFactCatalog::load_prepared_annotation_index(second, profiles); });
        }
        const auto prepared_rows = prepared_rows_offset(prepared_bytes);
        auto invalid_length = prepared_bytes;
        std::fill(invalid_length.begin() + prepared_rows, invalid_length.begin() + prepared_rows + 8, '\0');
        put(second, rechecksum(invalid_length));
        rejects([&] { CountFactCatalog::load_prepared_annotation_index(second, profiles); });
        auto invalid_parent_rank = prepared_bytes;
        std::fill(invalid_parent_rank.begin() + prepared_rows + 8,
                  invalid_parent_rank.begin() + prepared_rows + 12, static_cast<char>(0xff));
        put(second, rechecksum(invalid_parent_rank));
        rejects([&] { CountFactCatalog::load_prepared_annotation_index(second, profiles); });
        auto invalid_text_id = prepared_bytes;
        std::fill(invalid_text_id.begin() + prepared_rows + 12,
                  invalid_text_id.begin() + prepared_rows + 16, static_cast<char>(0xff));
        put(second, rechecksum(invalid_text_id));
        rejects([&] { CountFactCatalog::load_prepared_annotation_index(second, profiles); });
        if (moved_ledger.rows_by_path.size() > 1) {
            auto unordered_rows = prepared_bytes;
            std::swap_ranges(unordered_rows.begin() + prepared_rows,
                             unordered_rows.begin() + prepared_rows + 64,
                             unordered_rows.begin() + prepared_rows + 64);
            put(second, rechecksum(unordered_rows));
            rejects([&] { CountFactCatalog::load_prepared_annotation_index(second, profiles); });
        }
        auto wrong = indexed.binding; wrong.ledger_sha256.assign(64, '0');
        rejects([&] { CountFactCatalog::load_annotation_index(first, moved_ledger, wrong, profiles); });
        for (size_t size : {size_t(0), size_t(8), bytes.size()-1}) {
            put(second, bytes.substr(0, size));
            rejects([&] { CountFactCatalog::load_annotation_index(second, moved_ledger, indexed.binding, profiles); });
        }
        for (size_t offset : {size_t(0), size_t(8), bytes.size()-1}) {
            auto corrupt = bytes; corrupt[offset] ^= 1; put(second, corrupt);
            rejects([&] { CountFactCatalog::load_annotation_index(second, moved_ledger, indexed.binding, profiles); });
        }
        auto invalid_flags = bytes;
        invalid_flags[invalid_flags.size() - 65] = static_cast<char>(255);
        put(second, rechecksum(invalid_flags));
        rejects([&] { CountFactCatalog::load_annotation_index(second, moved_ledger, indexed.binding, profiles); });
        auto noncanonical = bytes;
        // First appearance is the gene dictionary; second is profile one's
        // equivalence target. Make two self-canonical targets into a cycle.
        const std::string a = "ENSG00000238009", b = "ENSG00000241860";
        const auto pa = noncanonical.find(a, noncanonical.find(a) + a.size());
        const auto pb = noncanonical.find(b, noncanonical.find(b) + b.size());
        require(pa != std::string::npos && pb != std::string::npos, "missing equivalence fixture");
        noncanonical.replace(pa, a.size(), b);
        noncanonical.replace(pb, b.size(), a);
        put(second, rechecksum(noncanonical));
        rejects([&] { CountFactCatalog::load_annotation_index(second, moved_ledger, indexed.binding, profiles); });
        for (const auto& entry : std::filesystem::directory_iterator(root))
            require(entry.path().filename().string().find(".tmp.") == std::string::npos, "staged file leaked");
        std::filesystem::remove_all(root);
        std::cout << "annotation index smoke passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
}
