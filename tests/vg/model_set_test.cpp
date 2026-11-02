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
int main() try {
    std::mt19937 random(118);
    for (unsigned trial = 0; trial < 300; ++trial) {
        Set a, b;
        std::set<std::uint32_t> va, vb, expected_union, expected_intersection, expected_difference;
        for (unsigned i = 0; i < 8192; ++i) {
            if ((random() % (trial % 7 + 2)) == 0) { append(a, i); va.insert(i); }
            if ((random() % (trial % 5 + 2)) == 0) { append(b, i); vb.insert(i); }
        }
        for (auto id : va) {
            expected_union.insert(id);
            (vb.contains(id) ? expected_intersection : expected_difference).insert(id);
        }
        expected_union.insert(vb.begin(), vb.end());
        require(values(decode(encode(a), 8192)) == va);
        require(values(unite(a, b)) == expected_union);
        require(values(intersection(a, b)) == expected_intersection);
        require(values(difference(a, b)) == expected_difference);
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
