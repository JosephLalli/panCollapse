#pragma once

#include <algorithm>
#include <iterator>
#include <map>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace pancollapse {

// Validate arbitrary input order with a tree, then retain only lexical rows.
// Freezing invalidates iterators/references; subsequent lookups and moves do not.
// Keys remain immutable and mapped values may still receive runtime bindings.
template<class Key, class Value, class Compare = std::less<>>
class FrozenFlatMap {
    using Tree = std::map<Key, Value, Compare>;
  public:
    using value_type = typename Tree::value_type;
    template<bool Const>
    class Iterator {
        using TreeIterator = std::conditional_t<Const, typename Tree::const_iterator,
                                                       typename Tree::iterator>;
        using Pointer = std::conditional_t<Const, const typename Tree::value_type*,
                                                 typename Tree::value_type*>;
        friend class FrozenFlatMap;
        explicit Iterator(TreeIterator value) : tree_(value) {}
        explicit Iterator(Pointer value) : flat_(value), is_flat_(true) {}
      public:
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = typename Tree::value_type;
        using difference_type = std::ptrdiff_t;
        using pointer = Pointer;
        using reference = std::conditional_t<Const, const value_type&, value_type&>;
        Iterator() = default;
        reference operator*() const { return is_flat_ ? *flat_ : *tree_; }
        pointer operator->() const { return &operator*(); }
        Iterator& operator++() { if (is_flat_) ++flat_; else ++tree_; return *this; }
        Iterator operator++(int) { auto prior = *this; ++*this; return prior; }
        Iterator& operator--() { if (is_flat_) --flat_; else --tree_; return *this; }
        Iterator operator--(int) { auto prior = *this; --*this; return prior; }
        bool operator==(const Iterator& other) const {
            return is_flat_ == other.is_flat_ &&
                   (is_flat_ ? flat_ == other.flat_ : tree_ == other.tree_);
        }
      private:
        TreeIterator tree_{};
        Pointer flat_ = nullptr;
        bool is_flat_ = false;
    };
    using iterator = Iterator<false>;
    using const_iterator = Iterator<true>;

    FrozenFlatMap() = default;
    FrozenFlatMap(FrozenFlatMap&&) = default;
    FrozenFlatMap& operator=(FrozenFlatMap&&) = default;
    FrozenFlatMap(const FrozenFlatMap&) = delete;
    FrozenFlatMap& operator=(const FrozenFlatMap&) = delete;

    // Construction of already validated lexical data without temporary tree nodes.
    // Only finish() publishes a usable frozen catalog; references survive its move.
    class SortedBuilder {
      public:
        explicit SortedBuilder(std::size_t count) : expected_(count) { rows_.reserve(count); }
        template<class K, class V> void append(K&& key, V&& value) {
            if (rows_.size() >= expected_ ||
                (!rows_.empty() && !compare_(rows_.back().first, key)))
                throw std::runtime_error("compiled catalog keys are not unique and lexical");
            rows_.emplace_back(std::forward<K>(key), std::forward<V>(value));
        }
        FrozenFlatMap finish() && {
            if (rows_.size() != expected_) throw std::runtime_error("compiled catalog row count mismatch");
            FrozenFlatMap result;
            result.rows_ = std::move(rows_); result.frozen_ = true;
            return result;
        }
      private:
        std::size_t expected_;
        std::vector<value_type> rows_;
        Compare compare_{};
    };

    std::size_t size() const { return frozen_ ? rows_.size() : tree_.size(); }
    bool empty() const { return size() == 0; }
    bool frozen() const { return frozen_; }
    std::size_t storage_bytes() const { return rows_.capacity() * sizeof(value_type); }
    iterator begin() { return frozen_ ? iterator(rows_.data()) : iterator(tree_.begin()); }
    iterator end() { return frozen_ ? iterator(flat_end()) : iterator(tree_.end()); }
    const_iterator begin() const {
        return frozen_ ? const_iterator(rows_.data()) : const_iterator(tree_.begin());
    }
    const_iterator end() const {
        return frozen_ ? const_iterator(flat_end()) : const_iterator(tree_.end());
    }
    template<class Lookup> iterator find(const Lookup& key) {
        if (!frozen_) return iterator(tree_.find(key));
        const auto rank = lower_rank(key);
        return rank < rows_.size() && !compare_(key, rows_[rank].first)
            ? iterator(rows_.data() + rank) : end();
    }
    template<class Lookup> const_iterator find(const Lookup& key) const {
        if (!frozen_) return const_iterator(tree_.find(key));
        const auto rank = lower_rank(key);
        return rank < rows_.size() && !compare_(key, rows_[rank].first)
            ? const_iterator(rows_.data() + rank) : end();
    }
    template<class Lookup> std::size_t count(const Lookup& key) const {
        return find(key) == end() ? 0 : 1;
    }
    template<class Lookup> Value& at(const Lookup& key) {
        const auto found = find(key);
        if (found == end()) throw std::out_of_range("flat catalog key is absent");
        return found->second;
    }
    template<class Lookup> const Value& at(const Lookup& key) const {
        const auto found = find(key);
        if (found == end()) throw std::out_of_range("flat catalog key is absent");
        return found->second;
    }
    template<class... Args> std::pair<iterator, bool> emplace(Args&&... args) {
        if (frozen_) throw std::logic_error("cannot insert into a frozen catalog");
        auto [found, inserted] = tree_.emplace(std::forward<Args>(args)...);
        return {iterator(found), inserted};
    }
    template<class Transform> void freeze(Transform&& transform) {
        if (frozen_) throw std::logic_error("catalog is already frozen");
        rows_.reserve(tree_.size());
        while (!tree_.empty()) {
            auto next = tree_.begin();
            transform(next->second);
            rows_.emplace_back(next->first, std::move(next->second));
            tree_.erase(next);
        }
        frozen_ = true;
    }
    void freeze() { freeze([](Value&) {}); }
    const value_type& at_rank(std::size_t rank) const {
        if (!frozen_) throw std::logic_error("catalog ranks require frozen storage");
        return rows_.at(rank);
    }
    template<class Lookup> std::size_t index_of(const Lookup& key) const {
        if (!frozen_) {
            const auto found = tree_.find(key);
            if (found == tree_.end()) throw std::out_of_range("catalog key is absent");
            return static_cast<std::size_t>(std::distance(tree_.begin(), found));
        }
        const auto rank = lower_rank(key);
        if (rank == rows_.size() || compare_(key, rows_[rank].first))
            throw std::out_of_range("flat catalog key is absent");
        return rank;
    }
  private:
    template<class Lookup> std::size_t lower_rank(const Lookup& key) const {
        return static_cast<std::size_t>(std::lower_bound(rows_.begin(), rows_.end(), key,
            [&](const value_type& row, const Lookup& wanted) {
                return compare_(row.first, wanted);
            }) - rows_.begin());
    }
    value_type* flat_end() { return rows_.empty() ? rows_.data() : rows_.data() + rows_.size(); }
    const value_type* flat_end() const {
        return rows_.empty() ? rows_.data() : rows_.data() + rows_.size();
    }
    Tree tree_;
    std::vector<value_type> rows_;
    Compare compare_{};
    bool frozen_ = false;
};

}  // namespace pancollapse
