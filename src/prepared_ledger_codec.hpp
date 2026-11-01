#pragma once

#include "path_identity_ledger.hpp"
#include <algorithm>
#include <array>
#include <iostream>
#include <limits>
#include <unordered_map>
#ifdef PANCOLLAPSE_PARALLEL_INDEX_SORT
#include <parallel/algorithm>
#endif

namespace pancollapse::direct_count::prepared_ledger {

inline void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(std::string("compiled ledger: ") + message);
}
template<class Row> auto texts(Row& row) {
    using Pointer = decltype(&row.vg_path_name);
    auto& a = row.annotation;
    return std::array<Pointer, 13>{&row.vg_path_name, &a.unique_parent, &a.canonical_transcript,
        &a.gene_id, &a.source_path_or_contig, &a.sample, &a.feature_layer,
        &a.exon_unique_parent, &a.gene_locus, &a.source_gene, &a.strand, &a.start, &a.end};
}
inline void encode(char* out, uint64_t value, unsigned width) {
    for (unsigned i = 0; i < width; ++i) out[i] = static_cast<char>((value >> (8*i)) & 255);
}
inline uint64_t decode(const char* input, unsigned width) {
    uint64_t value = 0;
    for (unsigned i = 0; i < width; ++i) value |= uint64_t(static_cast<unsigned char>(input[i])) << (8*i);
    return value;
}

template<class Writer> void write(Writer& out, const path_identity::PathIdentityLedger& ledger) {
    constexpr uint32_t missing = std::numeric_limits<uint32_t>::max();
    check(ledger.rows_by_path.frozen() && ledger.identities_by_parent.frozen(), "source is not finalized");
    check(ledger.rows_by_path.size() < missing && ledger.identities_by_parent.size() < missing,
          "compiled ranks exceed uint32");
    std::vector<CompactIdentityText> dictionary{CompactIdentityText{}};
    std::unordered_map<const void*, uint32_t> ids;
    ids.reserve(ledger.rows_by_path.size() * 2 + 1);
    ids.emplace(nullptr, 0);
    auto add = [&](const CompactIdentityText& text) {
        if (!ids.contains(text.storage_identity())) {
            check(dictionary.size() < missing, "too many dictionary strings");
            ids.emplace(text.storage_identity(), 0); dictionary.push_back(text);
        }
    };
    for (const auto& [key, row] : ledger.rows_by_path) {
        static_cast<void>(key);
        for (const auto* text : texts(row)) add(*text);
    }
    std::vector<CompactIdentityText> extra;
    auto add_string = [&](std::string_view text) { extra.emplace_back(text); add(extra.back()); };
    for (const auto& [key, value] : ledger.canonical_gene) { add_string(key); add_string(value); }
    for (const auto& [key, targets] : ledger.qualified_targets) {
        add(key);
        for (const auto& [source, target] : targets) {
            add(source); add(target.target); add(target.source_gene); add(target.gene_id);
        }
    }
    auto add_map = [&](const auto& map) {
        for (const auto& [key, value] : map) { add(key); add(value); }
    };
    add_map(ledger.normalized_source_genes); add_map(ledger.escaped_targets); add_map(ledger.original_transcripts);
    std::cerr << "panCollapse: indexing: phase=identity_dictionary_sort strings=" << dictionary.size() << '\n';
#ifdef PANCOLLAPSE_PARALLEL_INDEX_SORT
    __gnu_parallel::sort(dictionary.begin(), dictionary.end());
#else
    std::sort(dictionary.begin(), dictionary.end());
#endif
    // Pointer identity accelerates lookup only; canonical IDs follow lexical bytes.
    uint32_t unique = 0;
    for (size_t i = 0; i < dictionary.size(); ++i) {
        if (i && dictionary[i-1] != dictionary[i]) ++unique;
        ids.at(dictionary[i].storage_identity()) = unique;
    }
    dictionary.erase(std::unique(dictionary.begin(), dictionary.end()), dictionary.end());
    auto id = [&](const CompactIdentityText& text) { return ids.at(text.storage_identity()); };
    auto string_id = [&](std::string_view text) {
        auto found = std::lower_bound(dictionary.begin(), dictionary.end(), text,
            [](const auto& left, auto right) { return left.view() < right; });
        check(found != dictionary.end() && found->view() == text, "missing auxiliary string");
        return static_cast<uint32_t>(found - dictionary.begin());
    };
    out.string(ledger.source_path.string()); out.number(ledger.has_body_layer, 1);
    out.number(dictionary.size(), 8);
    for (const auto& text : dictionary) out.string(text.view());
    std::cerr << "panCollapse: indexing: phase=identity_rows rows=" << ledger.rows_by_path.size() << '\n';
    std::vector<uint32_t> parent_ranks(ledger.rows_by_path.size(), missing);
    uint32_t parent_rank = 0;
    for (const auto& [key, identity] : ledger.identities_by_parent) {
        static_cast<void>(key);
        const auto* row = reinterpret_cast<const path_identity::PathIdentityRow*>(identity);
        check(row->lexical_rank < parent_ranks.size(), "invalid representative row rank");
        parent_ranks[row->lexical_rank] = parent_rank++;
    }
    out.number(ledger.rows_by_path.size(), 8);
    std::array<char, 64> record{};
    size_t rank = 0;
    for (const auto& [key, row] : ledger.rows_by_path) {
        check(key == row.vg_path_name && row.lexical_rank == rank && !row.annotation.audit,
              "source row is not a validated runtime identity");
        uint32_t parent = parent_ranks[rank];
        if (parent == missing) parent = ledger.identities_by_parent.index_of(row.annotation.unique_parent);
        encode(record.data(), row.vg_path_length, 8); encode(record.data()+8, parent, 4);
        size_t offset = 12;
        for (const auto* text : texts(row)) { encode(record.data()+offset, id(*text), 4); offset += 4; }
        out.bytes(std::string_view(record.data(), record.size()));
        if (++rank % 1000000 == 0) std::cerr << "panCollapse: indexing: phase=identity_rows written=" << rank << '\n';
    }
    parent_ranks.clear(); parent_ranks.shrink_to_fit();
    out.number(ledger.identities_by_parent.size(), 8);
    std::array<char, 8> parent_record{};
    for (const auto& [key, identity] : ledger.identities_by_parent) {
        encode(parent_record.data(), id(key), 4);
        encode(parent_record.data()+4, reinterpret_cast<const path_identity::PathIdentityRow*>(identity)->lexical_rank, 4);
        out.bytes(std::string_view(parent_record.data(), parent_record.size()));
    }
    out.number(ledger.canonical_gene.size(), 8);
    for (const auto& [key, value] : ledger.canonical_gene) {
        out.number(string_id(key), 4); out.number(string_id(value), 4);
    }
    out.number(ledger.qualified_targets.size(), 8);
    for (const auto& [canonical, targets] : ledger.qualified_targets) {
        out.number(id(canonical), 4); out.number(targets.size(), 8);
        for (const auto& [source, target] : targets) {
            for (const auto* text : {&source, &target.target, &target.source_gene, &target.gene_id})
                out.number(id(*text), 4);
        }
    }
    auto write_map = [&](const auto& map) {
        std::vector<std::pair<uint32_t, uint32_t>> ordered;
        ordered.reserve(map.size());
        for (const auto& [key, value] : map) ordered.emplace_back(id(key), id(value));
        std::sort(ordered.begin(), ordered.end());
        out.number(ordered.size(), 8);
        for (const auto& [key, value] : ordered) { out.number(key, 4); out.number(value, 4); }
    };
    write_map(ledger.normalized_source_genes); write_map(ledger.escaped_targets); write_map(ledger.original_transcripts);
}

template<class Reader> path_identity::PathIdentityLedger read(Reader& in, uint64_t expected_rows) {
    path_identity::PathIdentityLedger ledger;
    ledger.source_path = in.string();
    const auto body = in.number(1); check(body <= 1, "invalid body-layer flag"); ledger.has_body_layer = body;
    const auto string_count = in.count(4);
    check(string_count > 0 && string_count < std::numeric_limits<uint32_t>::max(), "invalid dictionary count");
    std::vector<CompactIdentityText> dictionary;
    dictionary.reserve(string_count);
    for (uint64_t i = 0; i < string_count; ++i) {
        auto text = in.string();
        check((i == 0 && text.empty()) || (i && dictionary.back().view() < text),
              "dictionary is not unique and lexical");
        dictionary.emplace_back(text);
        if (i && i % 10000000 == 0) std::cerr << "panCollapse: initialization: phase=compiled_identity_dictionary strings=" << i << '\n';
    }
    auto text = [&](uint64_t id) -> const CompactIdentityText& {
        check(id < dictionary.size(), "invalid dictionary reference"); return dictionary[id];
    };
    const auto row_count = in.count(64);
    check(row_count == expected_rows && row_count > 0 && row_count < std::numeric_limits<uint32_t>::max(),
          "invalid row count");
    typename decltype(ledger.rows_by_path)::SortedBuilder rows(row_count);
    std::vector<uint32_t> parent_ranks(row_count);
    bool observed_body = false, observed_exon = false;
    // Bulk records avoid billions of scalar stream reads and temporary strings.
    for (uint64_t first = 0; first < row_count;) {
        const auto batch = std::min<uint64_t>(row_count-first, 8192);
        const auto block = in.bytes(batch * 64);
        for (uint64_t offset = 0; offset < batch; ++offset) {
            const char* record = block.data()+offset*64;
            path_identity::PathIdentityRow row;
            row.vg_path_length = decode(record, 8); check(row.vg_path_length > 0, "zero path length");
            parent_ranks[first+offset] = decode(record+8, 4);
            size_t field_offset = 12;
            for (auto* field : texts(row)) { *field = text(decode(record+field_offset, 4)); field_offset += 4; }
            const auto& a = row.annotation;
            check(!row.vg_path_name.empty() && !a.unique_parent.empty() && !a.canonical_transcript.empty() &&
                  !a.gene_id.empty() && (a.feature_layer == "exon" || a.feature_layer == "body") &&
                  (a.strand == "+" || a.strand == "-"), "invalid runtime identity fields");
            observed_body |= a.feature_layer == "body"; observed_exon |= a.feature_layer == "exon";
            row.lexical_rank = first+offset;
            auto key = row.vg_path_name; rows.append(std::move(key), std::move(row));
        }
        first += batch;
        if (first % (8192*128) == 0) std::cerr << "panCollapse: initialization: phase=compiled_identity_rows rows=" << first << '\n';
    }
    ledger.rows_by_path = std::move(rows).finish();
    check(observed_exon && observed_body == ledger.has_body_layer, "body/exon layer mismatch");
    const auto parent_count = in.count(8);
    check(parent_count > 0 && parent_count <= row_count, "invalid Parent count");
    typename decltype(ledger.identities_by_parent)::SortedBuilder parents(parent_count);
    for (uint64_t first = 0; first < parent_count;) {
        const auto batch = std::min<uint64_t>(parent_count-first, 8192);
        const auto block = in.bytes(batch*8);
        for (uint64_t offset = 0; offset < batch; ++offset) {
            const char* record = block.data()+offset*8;
            const auto key = text(decode(record, 4)); const auto row_rank = decode(record+4, 4);
            check(row_rank < row_count && parent_ranks[row_rank] == first+offset, "invalid Parent representative rank");
            const auto* identity = &ledger.rows_by_path.at_rank(row_rank).second.annotation;
            check(key == identity->unique_parent, "Parent representative identity mismatch");
            parents.append(key, identity);
        }
        first += batch;
    }
    ledger.identities_by_parent = std::move(parents).finish();
    for (size_t row_rank = 0; row_rank < row_count; ++row_rank) {
        check(parent_ranks[row_rank] < parent_count, "row Parent rank out of bounds");
        const auto& a = ledger.rows_by_path.at_rank(row_rank).second.annotation;
        const auto& p = ledger.identities_by_parent.at_rank(parent_ranks[row_rank]);
        check(a.unique_parent == p.first && a == *p.second, "row disagrees with Parent identity");
    }
    parent_ranks.clear(); parent_ranks.shrink_to_fit();
    auto read_pairs = [&](auto& map, bool string_values) {
        const auto count = in.count(8); uint64_t previous = 0;
        for (uint64_t i = 0; i < count; ++i) {
            const auto key = in.number(4), value = in.number(4);
            check(!i || previous < key, "auxiliary map is not unique and lexical"); previous = key;
            if constexpr (std::is_same_v<std::decay_t<decltype(map)>, decltype(ledger.canonical_gene)>) {
                static_cast<void>(string_values); map.emplace(text(key).str(), text(value).str());
            } else { static_cast<void>(string_values); map.emplace(text(key), text(value)); }
        }
    };
    read_pairs(ledger.canonical_gene, true);
    const auto qualified = in.count(12); uint64_t prior_canonical = 0;
    for (uint64_t i = 0; i < qualified; ++i) {
        const auto canonical = in.number(4);
        check(!i || prior_canonical < canonical, "qualified targets are not lexical"); prior_canonical = canonical;
        auto& targets = ledger.qualified_targets[text(canonical)];
        const auto count = in.count(16); check(count > 1, "invalid qualified target group");
        uint64_t prior_source = 0;
        for (uint64_t j = 0; j < count; ++j) {
            const auto source = in.number(4), target = in.number(4), normalized = in.number(4), gene = in.number(4);
            check((!j || prior_source < source) && source == normalized, "invalid qualified source"); prior_source = source;
            targets.emplace(text(source), path_identity::QualifiedTranscriptTarget{text(target), text(normalized), text(gene)});
        }
    }
    read_pairs(ledger.normalized_source_genes, false); read_pairs(ledger.escaped_targets, false);
    read_pairs(ledger.original_transcripts, false);
    return ledger;
}

} // namespace pancollapse::direct_count::prepared_ledger
