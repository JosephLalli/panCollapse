#include "annotation_fact_index.hpp"
#include "prepared_ledger_codec.hpp"

#include <openssl/evp.h>
#include <array>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <limits>
#include <set>
#include <unistd.h>

namespace pancollapse::direct_count {
namespace {

constexpr std::string_view magic = "PCANIDX2";
constexpr uint32_t format_version = 2;
// Increment when annotation derivation or Parent interpretation changes.
constexpr std::string_view semantics = "pancollapse-annotation-facts-v2";
constexpr std::string_view prepared_magic = "PCANIDX3";
constexpr std::string_view prepared_semantics = "pancollapse-prepared-annotation-v1";
constexpr uint32_t missing_gene = std::numeric_limits<uint32_t>::max();

[[noreturn]] void invalid(std::string_view reason) {
    throw std::runtime_error("annotation index: " + std::string(reason));
}

uint64_t opened_file_size(std::ifstream& file) {
    if (!file) invalid("cannot open input");
    file.seekg(0, std::ios::end);
    const auto end = file.tellg();
    if (end < 0) invalid("cannot determine input size");
    file.seekg(0);
    if (!file) invalid("cannot seek input");
    return static_cast<uint64_t>(end);
}

class Digest {
  public:
    Digest() : context_(EVP_MD_CTX_new(), EVP_MD_CTX_free) {
        if (!context_ || EVP_DigestInit_ex(context_.get(), EVP_sha256(), nullptr) != 1)
            invalid("cannot initialize SHA-256");
    }
    void update(const char* data, size_t size) {
        if (EVP_DigestUpdate(context_.get(), data, size) != 1) invalid("SHA-256 failed");
    }
    std::string finish_with_suffix(std::string_view suffix) const {
        Digest copy;
        if (EVP_MD_CTX_copy_ex(copy.context_.get(), context_.get()) != 1)
            invalid("cannot copy SHA-256 state");
        copy.update(suffix.data(), suffix.size());
        return copy.finish();
    }
    std::string finish() {
        std::array<unsigned char, 32> hash{};
        unsigned length = 0;
        if (EVP_DigestFinal_ex(context_.get(), hash.data(), &length) != 1 || length != 32)
            invalid("cannot finalize SHA-256");
        constexpr char hex[] = "0123456789abcdef";
        std::string result(64, '0');
        for (size_t i = 0; i < hash.size(); ++i) {
            result[2*i] = hex[hash[i] >> 4];
            result[2*i+1] = hex[hash[i] & 15];
        }
        return result;
    }
  private:
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> context_;
};

class DigestBuffer : public std::streambuf {
  public:
    DigestBuffer(std::istream& source, uint64_t size, bool digest = true)
        : source_(source), remaining_(size), digest_(digest ? std::make_unique<Digest>() : nullptr) {}
    void require_consumed() const {
        if (remaining_ || gptr() != egptr()) invalid("unconsumed or truncated input");
    }
    std::string finish() {
        require_consumed();
        if (!digest_) invalid("checksum was not requested");
        return digest_->finish();
    }
    std::string full_digest(std::string_view suffix) const {
        require_consumed();
        if (!digest_) invalid("checksum was not requested");
        return digest_->finish_with_suffix(suffix);
    }
  protected:
    int_type underflow() override {
        if (gptr() && gptr() < egptr()) return traits_type::to_int_type(*gptr());
        if (!remaining_) return traits_type::eof();
        const auto size = static_cast<std::streamsize>(std::min<uint64_t>(remaining_, data_.size()));
        source_.read(data_.data(), size);
        if (source_.gcount() != size) invalid("truncated input");
        if (digest_) digest_->update(data_.data(), static_cast<size_t>(size));
        remaining_ -= static_cast<uint64_t>(size);
        loaded_ += static_cast<uint64_t>(size);
        setg(data_.data(), data_.data(), data_.data() + size);
        return traits_type::to_int_type(*gptr());
    }
    pos_type seekoff(off_type offset, std::ios_base::seekdir direction,
                     std::ios_base::openmode) override {
        if (offset != 0 || direction != std::ios_base::cur) return pos_type(off_type(-1));
        const auto unread = gptr() ? static_cast<uint64_t>(egptr() - gptr()) : 0;
        return pos_type(static_cast<off_type>(loaded_ - unread));
    }
  private:
    std::istream& source_;
    uint64_t remaining_, loaded_ = 0;
    std::array<char, 1 << 16> data_{};
    std::unique_ptr<Digest> digest_;
};

class Writer {
  public:
    explicit Writer(const std::filesystem::path& path) : out_(path, std::ios::binary) {
        if (!out_) invalid("cannot open staged output");
        buffer_.reserve(1 << 16);
    }
    void bytes(std::string_view value) {
        buffer_.append(value);
        if (buffer_.size() >= (1 << 16)) flush();
    }
    void number(uint64_t value, unsigned width) {
        std::array<char, 8> encoded{};
        for (unsigned i = 0; i < width; ++i) encoded[i] = static_cast<char>((value >> (8*i)) & 255);
        bytes(std::string_view(encoded.data(), width));
    }
    void string(std::string_view value) {
        if (value.size() > (1u << 20)) invalid("string exceeds 1 MiB limit");
        number(value.size(), 4); bytes(value);
    }
    void finish() {
        flush();
        out_ << digest_.finish();
        out_.flush();
        if (!out_) invalid("cannot close staged output");
        out_.close();
        if (!out_) invalid("cannot close staged output");
    }
  private:
    void flush() {
        digest_.update(buffer_.data(), buffer_.size());
        out_.write(buffer_.data(), static_cast<std::streamsize>(buffer_.size()));
        if (!out_) invalid("write failed");
        buffer_.clear();
    }
    std::ofstream out_;
    std::string buffer_;
    Digest digest_;
};

class Reader {
  public:
    explicit Reader(const std::filesystem::path& path, bool verify_checksum = true)
        : file_(path, std::ios::binary), size_(opened_file_size(file_)),
          verify_checksum_(verify_checksum),
          buffer_(file_, size_ >= 64 ? size_ - 64 : 0, verify_checksum), input_(&buffer_) {
        if (size_ < 100 || !file_) invalid("missing or short header");
        input_.exceptions(std::ios::badbit);
    }
    uint64_t remaining() const { return size_ - 64 - consumed_; }
    uint64_t size() const { return size_; }
    const std::string& full_sha256() const { return full_sha256_; }
    std::string bytes(uint64_t size) {
        if (size > remaining() || size > (1u << 20)) invalid("invalid string/section size");
        std::string result(static_cast<size_t>(size), '\0');
        input_.read(result.data(), static_cast<std::streamsize>(size));
        if (!input_) invalid("truncated field");
        consumed_ += size;
        return result;
    }
    uint64_t number(unsigned width) {
        const std::string encoded = bytes(width);
        uint64_t value = 0;
        for (unsigned i = 0; i < width; ++i)
            value |= uint64_t(static_cast<unsigned char>(encoded[i])) << (8*i);
        return value;
    }
    std::string string() { return bytes(number(4)); }
    uint64_t count(uint64_t minimum_width) {
        const auto value = number(8);
        if (value > remaining() / minimum_width) invalid("record count exceeds section bounds");
        return value;
    }
    std::string finish() {
        if (remaining()) invalid("unexpected trailing payload");
        std::string expected(64, '\0');
        file_.read(expected.data(), 64);
        if (!file_) invalid("truncated checksum trailer");
        const bool valid_hex = std::all_of(expected.begin(), expected.end(), [](char c) {
            return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        });
        if (!verify_checksum_) {
            if (!valid_hex) invalid("invalid checksum trailer");
            buffer_.require_consumed();
            return expected;
        }
        full_sha256_ = buffer_.full_digest(expected);
        const auto actual = buffer_.finish();
        if (actual != expected) invalid("checksum mismatch");
        return actual;
    }
  private:
    std::ifstream file_;
    uint64_t size_, consumed_ = 0;
    bool verify_checksum_;
    DigestBuffer buffer_;
    std::istream input_;
    std::string full_sha256_;
};

void sync_path(const std::filesystem::path& path, bool directory = false) {
    const int fd = open(path.c_str(), O_RDONLY | (directory ? O_DIRECTORY : 0));
    if (fd < 0) invalid("cannot open output for fsync");
    const int status = fsync(fd);
    close(fd);
    if (status != 0) invalid("fsync failed");
}

} // namespace

IndexedLedger read_index_ledger(const std::filesystem::path& filename,
    const std::function<void(size_t, size_t, uint64_t)>& progress) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) throw std::runtime_error("cannot open path identity ledger " + filename.string());
    const auto size = opened_file_size(file);
    DigestBuffer buffer(file, size);
    std::istream input(&buffer);
    input.exceptions(std::ios::badbit);
    auto ledger = path_identity::read(filename, true, progress,
                                     normalized_count_gene_identity, &input);
    return {std::move(ledger), {buffer.finish(), size}};
}

void CountFactCatalog::write_annotation_index(const std::filesystem::path& destination,
    const path_identity::PathIdentityLedger& ledger, const AnnotationIndexBinding& binding) const {
    write_annotation_index_impl(destination, ledger, binding, false);
}
void CountFactCatalog::write_prepared_annotation_index(const std::filesystem::path& destination,
    const path_identity::PathIdentityLedger& ledger, const AnnotationIndexBinding& binding) const {
    write_annotation_index_impl(destination, ledger, binding, true);
}
void CountFactCatalog::write_annotation_index_impl(const std::filesystem::path& destination,
    const path_identity::PathIdentityLedger& ledger, const AnnotationIndexBinding& binding, bool prepared) const {
    if (authority_ != "annotation" || parents_.size() != ledger.identities_by_parent.size())
        invalid("only complete annotation-derived catalogs can be indexed");
    const auto source = annotation_sources_.find("path_identity_ledger");
    if (source == annotation_sources_.end() || source->second.sha256 != binding.ledger_sha256 ||
        source->second.size_bytes != binding.ledger_size) invalid("ledger source identity mismatch");
    if (std::filesystem::exists(destination)) invalid("destination already exists");
    const auto absolute = std::filesystem::absolute(destination).lexically_normal();
    std::filesystem::create_directories(absolute.parent_path());
    std::string pattern = absolute.string() + ".tmp.XXXXXX";
    const int fd = mkstemp(pattern.data());
    if (fd < 0) invalid("cannot create exclusive temporary output");
    close(fd);
    const std::filesystem::path staged(pattern);
    try {
        Writer out(staged);
        out.bytes(prepared ? prepared_magic : magic);
        out.number(prepared ? 3 : format_version, 4); out.string(prepared ? prepared_semantics : semantics);
        out.number(static_cast<uint8_t>(annotation_format_), 1);
        out.number(static_cast<uint8_t>(reference_format_), 1);
        out.string(content_id_); out.string(authority_);
        out.number(annotation_sources_.size(), 8);
        for (const auto& [role, fact] : annotation_sources_) {
            out.string(role); out.string(fact.sha256); out.number(fact.size_bytes, 8);
        }
        out.number(ledger.rows_by_path.size(), 8);
        if (prepared) prepared_ledger::write(out, ledger);
        out.number(genes_.size(), 8);
        std::map<std::string, uint32_t, std::less<>> genes;
        if (genes_.size() >= missing_gene) invalid("too many gene IDs");
        for (const auto& [key, gene] : genes_) {
            genes.emplace(key, static_cast<uint32_t>(genes.size()));
            out.string(key); out.number(gene.gene_types.size(), 8);
            for (const auto& type : gene.gene_types) out.string(type);
            out.number(gene.gene_name.has_value(), 1);
            if (gene.gene_name) out.string(*gene.gene_name);
        }
        out.number(profile_genes_.size(), 8);
        for (const auto& [id, policies] : profile_genes_) {
            out.string(profile(id).id_string); out.string(effective_profile(id).effective_sha256);
            out.number(policies.size(), 8);
            for (const auto& [gene, policy] : policies) {
                out.number(genes.at(gene), 4); out.number(static_cast<uint8_t>(policy.competition), 1);
                out.string(policy.competition_reason); out.string(policy.equivalence_gene);
                out.string(policy.source_annotation); out.string(policy.nested_host);
            }
        }
        // Category sets and genes are shared dictionaries. Unique Parent names and
        // other identity text are reused from the checksum-bound ledger, not copied.
        std::map<std::vector<std::string>, uint32_t> category_sets;
        for (const auto [key, fact] : parents_) {
            static_cast<void>(key);
            category_sets.try_emplace(fact.raw_categories, 0);
        }
        out.number(category_sets.size(), 8);
        uint32_t category_id = 0;
        for (auto& [categories, id] : category_sets) {
            id = category_id++;
            out.number(categories.size(), 8);
            for (const auto& category : categories) out.string(category);
        }
        out.number(parents_.size(), 8);
        auto parent = ledger.identities_by_parent.begin();
        for (const auto [key, fact] : parents_) {
            if (parent == ledger.identities_by_parent.end() || parent->first != key)
                invalid("Parent coverage mismatch");
            const auto& identity = *parent->second;
            if (fact.unique_parent != key || fact.runtime_gene != identity.gene_id ||
                fact.canonical_transcript != identity.canonical_transcript ||
                fact.sample != identity.sample || fact.feature_layer != identity.feature_layer)
                invalid("Parent identity mismatch");
            std::array<char, 13> record{};
            prepared_ledger::encode(record.data(), genes.at(fact.count_gene.str()), 4);
            prepared_ledger::encode(record.data()+4, category_sets.at(fact.raw_categories), 4);
            prepared_ledger::encode(record.data()+8,
                fact.novel_origin_gene.empty() ? missing_gene : genes.at(fact.novel_origin_gene.str()), 4);
            record[12] = static_cast<char>((fact.strong_local_support ? 1u : 0u) |
                (fact.protected_primary_protein_nested_host ? 2u : 0u) |
                (fact.novel_paralog ? 4u : 0u));
            out.bytes(std::string_view(record.data(), record.size()));
            ++parent;
        }
        out.finish();
        sync_path(staged);
        // V3 accumulates its checksum during writing; no full verification reread.
        // Preserve the existing V2 publication contract for legacy callers.
        if (!prepared) {
        std::ifstream check(staged, std::ios::binary);
        const auto payload = std::filesystem::file_size(staged) - 64;
        DigestBuffer checked(check, payload);
        std::istream stream(&checked);
        stream.exceptions(std::ios::badbit);
        std::array<char, 1 << 16> block{};
        while (stream.read(block.data(), block.size()) || stream.gcount()) {}
        std::string expected(64, '\0'); check.read(expected.data(), 64);
        if (!check || checked.finish() != expected) invalid("staged checksum verification failed");
        }
        // link publishes without overwriting a racing destination; same filesystem.
        if (link(staged.c_str(), absolute.c_str()) != 0) invalid("cannot publish (destination exists or link failed)");
        // The published inode is complete even if post-publication cleanup or
        // directory durability fails. Make that state explicit to callers.
        try {
            std::filesystem::remove(staged);
            sync_path(absolute.parent_path(), true);
        } catch (const std::exception& error) {
            throw std::runtime_error("annotation index was published at " + absolute.string() +
                ", but cleanup/directory sync failed: " + error.what());
        }
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove(staged, ignored);
        throw;
    }
}

CountFactCatalog CountFactCatalog::load_annotation_index(const std::filesystem::path& filename,
    const path_identity::PathIdentityLedger& ledger, const AnnotationIndexBinding& binding,
    const std::vector<ProfileId>& required_profiles) {
    return load_annotation_index_impl(filename, &ledger, &binding, required_profiles, nullptr, nullptr, true);
}
PreparedAnnotationReference CountFactCatalog::load_prepared_annotation_index(
    const std::filesystem::path& filename, const std::vector<ProfileId>& required_profiles) {
    PreparedAnnotationReference result;
    result.facts = load_annotation_index_impl(filename, nullptr, nullptr, required_profiles,
        &result.ledger, &result.binding, true);
    return result;
}
PreparedAnnotationReference CountFactCatalog::load_prepared_annotation_index_trusted(
    const std::filesystem::path& filename, const std::vector<ProfileId>& required_profiles) {
    PreparedAnnotationReference result;
    result.facts = load_annotation_index_impl(filename, nullptr, nullptr, required_profiles,
        &result.ledger, &result.binding, false);
    return result;
}
CountFactCatalog CountFactCatalog::load_annotation_index_impl(const std::filesystem::path& filename,
    const path_identity::PathIdentityLedger* legacy_ledger, const AnnotationIndexBinding* legacy_binding,
    const std::vector<ProfileId>& required_profiles,
    path_identity::PathIdentityLedger* prepared_ledger, AnnotationIndexBinding* prepared_binding,
    bool verify_checksum) {
    Reader in(filename, verify_checksum);
    const auto file_magic = in.bytes(magic.size());
    const auto version = in.number(4); const auto meaning = in.string();
    const bool prepared = file_magic == prepared_magic && version == 3 && meaning == prepared_semantics;
    if (!prepared && (file_magic != magic || version != format_version || meaning != semantics))
        invalid("unsupported magic, format, or annotation semantics version");
    if (prepared && !prepared_ledger) invalid("PCANIDX3 contains its ledger; omit --path-identity-ledger");
    if (!prepared && !legacy_ledger) invalid("PCANIDX2 requires --path-identity-ledger; upgrade with index --from-annotation-index");
    CountFactCatalog result;
    const auto annotation_format = in.number(1), reference_format = in.number(1);
    if (annotation_format > 1 || reference_format > 1) invalid("unknown annotation format");
    result.annotation_format_ = static_cast<AnnotationFormat>(annotation_format);
    result.reference_format_ = static_cast<AnnotationFormat>(reference_format);
    result.content_id_ = in.string(); result.authority_ = in.string();
    if (!result.content_id_.starts_with("sha256:") || result.content_id_.size() != 71 ||
        result.authority_ != "annotation") invalid("invalid source content identity");
    const auto sources = in.count(12);
    if (sources != 3) invalid("incomplete source identities");
    for (uint64_t i = 0; i < sources; ++i) {
        auto role = in.string(); auto sha = in.string(); auto size = in.number(8);
        const bool valid_sha = sha.size() == 64 && std::all_of(sha.begin(), sha.end(),
            [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
        if (!valid_sha || !result.annotation_sources_.emplace(role, AnnotationSourceFile{sha, size}).second)
            invalid("invalid or duplicate source identity");
    }
    for (const char* role : {"annotation", "annotation_reference", "path_identity_ledger"})
        if (!result.annotation_sources_.contains(role)) invalid("missing source identity");
    Digest source_identity;
    for (const char* role : {"annotation", "annotation_reference", "path_identity_ledger"}) {
        const auto& sha = result.annotation_sources_.at(role).sha256;
        source_identity.update(sha.data(), sha.size()); source_identity.update("\n", 1);
    }
    if (result.content_id_ != "sha256:" + source_identity.finish()) invalid("source content identity mismatch");
    const auto& bound = result.annotation_sources_.at("path_identity_ledger");
    const auto rows = in.number(8);
    if (prepared) {
        *prepared_binding = {bound.sha256, bound.size_bytes};
        *prepared_ledger = prepared_ledger::read(in, rows);
    }
    const auto& ledger = prepared ? *prepared_ledger : *legacy_ledger;
    const auto& binding = prepared ? *prepared_binding : *legacy_binding;
    if (bound.sha256 != binding.ledger_sha256 || bound.size_bytes != binding.ledger_size ||
        rows != ledger.rows_by_path.size()) invalid("index does not match path identity ledger");
    const auto gene_count = in.count(13);
    if (gene_count >= missing_gene) invalid("too many gene IDs");
    std::vector<CompactIdentityText> genes;
    genes.reserve(static_cast<size_t>(gene_count));
    for (uint64_t i = 0; i < gene_count; ++i) {
        GeneFact gene; gene.count_gene = in.string();
        if (gene.count_gene.empty() || (!genes.empty() && !(genes.back() < gene.count_gene)))
            invalid("gene IDs are not unique and lexical");
        const auto types = in.count(4);
        for (uint64_t j = 0; j < types; ++j) gene.gene_types.push_back(in.string());
        const auto named = in.number(1);
        if (named > 1) invalid("invalid nullable gene name");
        if (named) gene.gene_name = in.string();
        genes.emplace_back(gene.count_gene);
        result.genes_.emplace(gene.count_gene, std::move(gene));
    }
    const auto profiles = in.count(20);
    if (profiles < 1 || profiles > 2) invalid("invalid profile count");
    for (uint64_t i = 0; i < profiles; ++i) {
        const auto id = parse_profile_id(in.string());
        if (in.string() != effective_profile(id).effective_sha256 || result.profile_genes_.contains(id))
            invalid("profile definition mismatch or duplicate");
        auto& policies = result.profile_genes_[id];
        if (in.count(21) != gene_count) invalid("incomplete profile gene coverage");
        for (uint64_t j = 0; j < gene_count; ++j) {
            if (in.number(4) != j) invalid("invalid profile gene order");
            const auto competition = in.number(1);
            if (competition > 2) invalid("invalid competition enum");
            ProfileGeneFact fact;
            fact.profile_id = id; fact.count_gene = genes[j].str();
            fact.competition = static_cast<CompetitionClass>(competition);
            fact.competition_reason = in.string(); fact.equivalence_gene = in.string();
            fact.source_annotation = in.string(); fact.nested_host = in.string();
            if (!result.genes_.contains(fact.equivalence_gene)) invalid("unknown equivalence gene");
            policies.emplace(fact.count_gene, std::move(fact));
        }
    }
    for (const auto id : required_profiles)
        if (!result.profile_genes_.contains(id)) invalid("requested profile is absent; rebuild index");
    for (const auto& [id, policies] : result.profile_genes_) {
        static_cast<void>(id);
        for (const auto& [gene, policy] : policies) {
            static_cast<void>(gene);
            const auto canonical = policies.find(policy.equivalence_gene);
            if (canonical == policies.end() || canonical->second.equivalence_gene != policy.equivalence_gene ||
                canonical->second.competition != policy.competition)
                invalid("noncanonical or inconsistent equivalence-gene policy");
        }
    }
    const auto set_count = in.count(8);
    std::vector<std::vector<std::string>> category_sets;
    for (uint64_t i = 0; i < set_count; ++i) {
        const auto count = in.count(4);
        if (count > 128) invalid("too many Parent categories");
        std::vector<std::string> categories;
        for (uint64_t j = 0; j < count; ++j) categories.push_back(in.string());
        if (!std::is_sorted(categories.begin(), categories.end()) ||
            std::adjacent_find(categories.begin(), categories.end()) != categories.end() ||
            (!category_sets.empty() && !(category_sets.back() < categories)))
            invalid("category dictionaries are not unique and lexical");
        category_sets.push_back(std::move(categories));
    }
    const auto parents = in.count(13);
    if (parents != ledger.identities_by_parent.size()) invalid("Parent coverage mismatch");
    result.parents_.reserve(static_cast<size_t>(parents));
    result.parents_.compiled_dictionaries(std::move(genes), std::move(category_sets));
    auto parent = ledger.identities_by_parent.begin();
    for (uint64_t first = 0; first < parents;) {
        const auto batch = std::min<uint64_t>(parents-first, 8192);
        const auto block = in.bytes(batch*13);
        for (uint64_t offset = 0; offset < batch; ++offset, ++parent) {
            const char* record = block.data()+offset*13;
            result.parents_.append_compiled(parent->second,
                prepared_ledger::decode(record,4), prepared_ledger::decode(record+4,4),
                prepared_ledger::decode(record+8,4), static_cast<uint8_t>(record[12]));
        }
        first += batch;
    }
    // A deterministic derivation identity covers formats, semantic version,
    // source binding, profile definitions, dictionaries and numeric Parent rows.
    result.annotation_index_derivation_id_ = "sha256:" + in.finish();
    result.annotation_index_file_sha256_ = in.full_sha256();
    result.annotation_index_file_size_ = in.size();
    return result;
}

} // namespace pancollapse::direct_count
