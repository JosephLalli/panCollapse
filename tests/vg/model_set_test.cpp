#include "model_set.hpp"
#include <iostream>
#include <random>
#include <set>

using namespace pancollapse::model_set;
void require(bool value) { if (!value) throw std::runtime_error("model-set test mismatch"); }
std::set<std::uint32_t> values(const Set& set) {
    std::set<std::uint32_t> result;
    each(set, [&](auto value) { result.insert(value); });
    return result;
}
void check_operations(const Set& a, const Set& b, Set& scratch, Set& yes, Set& no) {
    const auto va = values(a), vb = values(b);
    std::set<std::uint32_t> expected_union = vb, expected_intersection, expected_difference;
    for (auto id : va) {
        expected_union.insert(id);
        (vb.contains(id) ? expected_intersection : expected_difference).insert(id);
    }
    auto merged = a;
    unite(merged, b, scratch);
    require(values(merged) == expected_union);
    split(a, b, yes, no);
    require(values(yes) == expected_intersection && values(no) == expected_difference);
    require(!overlaps(yes, no));
    auto restored = yes;
    unite(restored, no, scratch);
    require(values(restored) == va);
    for (const auto* set : {&merged, &yes, &no}) {
        std::uint32_t prior = 0;
        bool first = true;
        for (const auto& block : *set) {
            require(!empty(block) && (first || block.id > prior));
            first = false; prior = block.id;
        }
    }
}
int main() try {
    Set scratch, yes, no;
    const Set none;
    Set a_edges, b_edges;
    for (auto id : {0U, 1U, 63U, 64U, 255U, 512U, UINT32_MAX}) append(a_edges, id);
    for (auto id : {1U, 62U, 64U, 256U, 513U, UINT32_MAX}) append(b_edges, id);
    for (unsigned repeat = 0; repeat < 3; ++repeat) {
        check_operations(a_edges, b_edges, scratch, yes, no);
        check_operations(a_edges, a_edges, scratch, yes, no);
        check_operations(none, b_edges, scratch, yes, no);
        check_operations(a_edges, none, scratch, yes, no);
        check_operations(none, none, scratch, yes, no);
    }
    std::mt19937 random(118);
    for (unsigned trial = 0; trial < 300; ++trial) {
        Set a, b;
        std::set<std::uint32_t> va;
        for (unsigned i = 0; i < 8192; ++i) {
            if ((random() % (trial % 7 + 2)) == 0) { append(a, i); va.insert(i); }
            if ((random() % (trial % 5 + 2)) == 0) append(b, i);
        }
        require(values(decode(encode(a), 8192)) == va);
        check_operations(a, b, scratch, yes, no);
        for (unsigned i = 0; i < 8192; ++i) require(contains(a, i) == va.contains(i));
    }
    for (unsigned pattern = 0; pattern < 3; ++pattern) {
        Set a;
        for (unsigned i = 0; i < 768; ++i)
            if (pattern == 0 ? i == 1 || i == 600 : pattern == 1 ? i % 256 < 200 : i % 2 == 0) append(a, i);
        const auto bytes = encode(a);
        require(values(decode(bytes, 768)) == values(a));
        for (std::size_t n = 0; n < bytes.size(); ++n) {
            bool failed = false;
            try { static_cast<void>(decode(std::span(bytes).first(n), 768)); }
            catch (const std::exception&) { failed = true; }
            require(failed);
        }
        bool failed = false;
        try { static_cast<void>(decode(bytes, 1)); } catch (const std::exception&) { failed = true; }
        require(failed);
    }
    require(decode(encode({}), 0).empty());
    Set one; append(one, 1);
    const auto set = encode(one);
    std::vector<std::byte> flags;
    put32(flags, 1); put32(flags, set.size()); flags.insert(flags.end(), set.begin(), set.end());
    require(decode_flags(flags, 2, 1).size() == 1);
    const auto rejects = [&](const auto& payload, unsigned count) {
        bool failed = false;
        try { static_cast<void>(decode_flags(payload, 2, count)); }
        catch (const std::exception&) { failed = true; }
        require(failed);
    };
    rejects(flags, 0); rejects(flags, 2);
    rejects(std::vector<std::byte>{}, 1);
    put32(flags, 4); put32(flags, set.size()); flags.insert(flags.end(), set.begin(), set.end());
    rejects(flags, 2); // one model cannot occupy two exact-flag partitions
    std::cout << "model-set codec and set-operation differential passed\n";
} catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
