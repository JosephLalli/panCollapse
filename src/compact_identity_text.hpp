#pragma once

#include <atomic>
#include <compare>
#include <cstddef>
#include <functional>
#include <ostream>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace pancollapse {

// A small, shared, immutable string suitable for repeatedly stored identities.
class CompactIdentityText {
    struct Atom {
        explicit Atom(std::string_view text) : text(text) {}

        std::atomic_size_t references{1};
        const std::string text;
    };

public:
    CompactIdentityText() noexcept = default;
    CompactIdentityText(std::string_view text) : atom_(text.empty() ? nullptr : new Atom(text)) {}
    CompactIdentityText(const std::string& text) : CompactIdentityText(std::string_view(text)) {}
    CompactIdentityText(std::string&& text) : CompactIdentityText(std::string_view(text)) {}
    CompactIdentityText(const char* text) : CompactIdentityText(std::string_view(text)) {}

    CompactIdentityText(const CompactIdentityText& other) noexcept : atom_(other.atom_) { retain(); }
    CompactIdentityText(CompactIdentityText&& other) noexcept : atom_(std::exchange(other.atom_, nullptr)) {}

    ~CompactIdentityText() { release(); }

    CompactIdentityText& operator=(const CompactIdentityText& other) noexcept {
        if (this != &other) {
            CompactIdentityText copy(other);
            swap(copy);
        }
        return *this;
    }

    CompactIdentityText& operator=(CompactIdentityText&& other) noexcept {
        if (this != &other) {
            release();
            atom_ = std::exchange(other.atom_, nullptr);
        }
        return *this;
    }

    CompactIdentityText& operator=(std::string_view text) {
        CompactIdentityText replacement(text);
        swap(replacement);
        return *this;
    }
    CompactIdentityText& operator=(const std::string& text) { return *this = std::string_view(text); }
    CompactIdentityText& operator=(std::string&& text) { return *this = std::string_view(text); }
    CompactIdentityText& operator=(const char* text) { return *this = std::string_view(text); }

    void swap(CompactIdentityText& other) noexcept { std::swap(atom_, other.atom_); }

    const std::string& str() const noexcept { return atom_ ? atom_->text : empty_string(); }
    std::string_view view() const noexcept { return str(); }
    operator const std::string&() const noexcept { return str(); }
    operator std::string_view() const noexcept { return view(); }

    bool empty() const noexcept { return str().empty(); }
    std::size_t size() const noexcept { return str().size(); }
    const char* data() const noexcept { return str().data(); }
    const char* c_str() const noexcept { return str().c_str(); }
    // Process-local sharing token for compiled-index dictionaries, never serialized.
    const void* storage_identity() const noexcept { return atom_; }
    std::string substr(std::size_t pos = 0, std::size_t count = std::string::npos) const {
        return str().substr(pos, count);
    }
    std::size_t find(std::string_view text, std::size_t pos = 0) const noexcept {
        return view().find(text, pos);
    }
    std::size_t find(char character, std::size_t pos = 0) const noexcept {
        return view().find(character, pos);
    }
    std::size_t rfind(std::string_view text, std::size_t pos = std::string_view::npos) const noexcept {
        return view().rfind(text, pos);
    }
    std::size_t rfind(char character, std::size_t pos = std::string_view::npos) const noexcept {
        return view().rfind(character, pos);
    }
    char front() const { return str().front(); }
    char back() const { return str().back(); }
    auto begin() const noexcept { return str().begin(); }
    auto end() const noexcept { return str().end(); }
    char operator[](std::size_t pos) const noexcept { return str()[pos]; }

    friend bool operator==(const CompactIdentityText& left, const CompactIdentityText& right) noexcept {
        return left.atom_ == right.atom_ || left.view() == right.view();
    }
    friend std::strong_ordering operator<=>(const CompactIdentityText& left,
                                            const CompactIdentityText& right) noexcept {
        if (left.atom_ == right.atom_) return std::strong_ordering::equal;
        return left.view() <=> right.view();
    }
    friend bool operator==(const CompactIdentityText& left, std::string_view right) noexcept {
        return left.view() == right;
    }
    friend bool operator==(std::string_view left, const CompactIdentityText& right) noexcept {
        return left == right.view();
    }
    friend bool operator==(const CompactIdentityText& left, const std::string& right) noexcept {
        return left.view() == std::string_view(right);
    }
    friend bool operator==(const std::string& left, const CompactIdentityText& right) noexcept {
        return std::string_view(left) == right.view();
    }
    friend bool operator==(const CompactIdentityText& left, const char* right) noexcept {
        return left.view() == std::string_view(right);
    }
    friend bool operator==(const char* left, const CompactIdentityText& right) noexcept {
        return std::string_view(left) == right.view();
    }
    friend std::strong_ordering operator<=>(const CompactIdentityText& left, std::string_view right) noexcept {
        return left.view() <=> right;
    }
    friend std::strong_ordering operator<=>(std::string_view left, const CompactIdentityText& right) noexcept {
        return left <=> right.view();
    }
    friend std::strong_ordering operator<=>(const CompactIdentityText& left, const std::string& right) noexcept {
        return left.view() <=> std::string_view(right);
    }
    friend std::strong_ordering operator<=>(const std::string& left, const CompactIdentityText& right) noexcept {
        return std::string_view(left) <=> right.view();
    }
    friend std::strong_ordering operator<=>(const CompactIdentityText& left, const char* right) noexcept {
        return left.view() <=> std::string_view(right);
    }
    friend std::strong_ordering operator<=>(const char* left, const CompactIdentityText& right) noexcept {
        return std::string_view(left) <=> right.view();
    }
    friend std::string operator+(const CompactIdentityText& left, std::string_view right) {
        std::string result(left.str());
        result.append(right);
        return result;
    }
    friend std::string operator+(std::string_view left, const CompactIdentityText& right) {
        std::string result(left);
        result.append(right.str());
        return result;
    }
    friend std::string operator+(const CompactIdentityText& left, const CompactIdentityText& right) {
        return left + right.view();
    }
    friend std::string operator+(const CompactIdentityText& left, const std::string& right) {
        return left + std::string_view(right);
    }
    friend std::string operator+(const std::string& left, const CompactIdentityText& right) {
        return std::string_view(left) + right;
    }
    friend std::string operator+(const CompactIdentityText& left, const char* right) {
        return left + std::string_view(right);
    }
    friend std::string operator+(const char* left, const CompactIdentityText& right) {
        return std::string_view(left) + right;
    }
    friend std::ostream& operator<<(std::ostream& output, const CompactIdentityText& text) {
        return output << text.str();
    }

private:
    static const std::string& empty_string() noexcept {
        static const std::string empty;
        return empty;
    }
    void retain() noexcept {
        if (atom_) atom_->references.fetch_add(1, std::memory_order_relaxed);
    }
    void release() noexcept {
        if (atom_ && atom_->references.fetch_sub(1, std::memory_order_acq_rel) == 1) delete atom_;
    }

    Atom* atom_ = nullptr;
};

struct CompactIdentityTextHash {
    using is_transparent = void;

    std::size_t operator()(const CompactIdentityText& text) const noexcept {
        return std::hash<std::string_view>{}(text.view());
    }
    std::size_t operator()(std::string_view text) const noexcept {
        return std::hash<std::string_view>{}(text);
    }
};

struct CompactIdentityTextEqual {
    using is_transparent = void;

    bool operator()(const CompactIdentityText& left, const CompactIdentityText& right) const noexcept {
        return left == right;
    }
    bool operator()(const CompactIdentityText& left, std::string_view right) const noexcept {
        return left == right;
    }
    bool operator()(std::string_view left, const CompactIdentityText& right) const noexcept {
        return left == right;
    }
};

class CompactIdentityTextPool {
public:
    CompactIdentityText intern(std::string_view text) {
        const auto found = values_.find(text);
        if (found != values_.end()) return *found;
        return *values_.insert(CompactIdentityText(text)).first;
    }

    void clear() noexcept { values_.clear(); }
    std::size_t size() const noexcept { return values_.size(); }

private:
    std::unordered_set<CompactIdentityText, CompactIdentityTextHash, CompactIdentityTextEqual> values_;
};

}  // namespace pancollapse

namespace std {
template <>
struct hash<pancollapse::CompactIdentityText> {
    std::size_t operator()(const pancollapse::CompactIdentityText& text) const noexcept {
        return pancollapse::CompactIdentityTextHash{}(text);
    }
};
}  // namespace std
