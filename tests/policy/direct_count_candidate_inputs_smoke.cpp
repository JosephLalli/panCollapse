#include "direct_count_candidate_inputs.hpp"

#include <stdexcept>

using namespace pancollapse::direct_count;

// Record the stream before hashing, so a digest collision cannot hide a framing bug.
struct RecordingDigest {
    std::string bytes;
    void byte(unsigned char value) { bytes.push_back(static_cast<char>(value)); }
    void integer(std::uint64_t value) {
        for (int shift = 56; shift >= 0; shift -= 8) byte(value >> shift);
    }
    void string(std::string_view value) { integer(value.size()); bytes.append(value); }
};

static std::string key(const std::vector<AssignmentCandidateInput>& exact,
                       const std::vector<AssignmentCandidateInput>& fallback = {}) {
    RecordingDigest digest;
    std::vector<const AssignmentCandidateInput*> rows;
    digest_assignment_inputs(digest, exact, rows);
    digest_assignment_inputs(digest, fallback, rows);
    return digest.bytes;
}

static void expect(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

static ParentFact parent_fact(std::string unique_parent) {
    ParentFact result;
    result.unique_parent = std::move(unique_parent);
    return result;
}

int main() {
    const ParentFact parent = parent_fact("sample#hap#Parent");
    const ParentFact same_parent = parent_fact(parent.unique_parent);
    const ParentFact other_parent = parent_fact("other#hap#Parent");
    const AssignmentCandidateInput a{&parent, 10, EvidenceTier::exon, EvidenceStrand::forward};
    const AssignmentCandidateInput b{&other_parent, -1, EvidenceTier::body, EvidenceStrand::reverse};
    expect(key({a, b, a}) == key({b, a, a}), "candidate ordering changed key");
    auto changed = a;
    changed.parent = &same_parent;
    expect(key({a}) == key({changed}), "pointer address entered key");
    changed.parent = &other_parent;
    expect(key({a}) != key({changed}), "Parent identity missing");
    changed = a; changed.score = -1;
    expect(key({a}) != key({changed}), "score missing");
    changed = a; changed.tier = EvidenceTier::partial_exon;
    expect(key({a}) != key({changed}), "tier missing");
    changed = a; changed.strand = EvidenceStrand::reverse;
    expect(key({a}) != key({changed}), "strand missing");
    changed = a; changed.body_sample_support = 1;
    expect(key({a}) != key({changed}), "body support missing");
    expect(key({a}) != key({}, {a}), "exact and fallback lists conflated");
    expect(key({a}) != key({a, a}), "candidate multiplicity lost");
    expect(key({a}, {b}) != key({a, b}), "candidate list boundary lost");
    const ParentFact ab = parent_fact("ab"), c = parent_fact("c");
    const ParentFact first = parent_fact("a"), bc = parent_fact("bc");
    auto x = a, y = a, z = a, w = a;
    x.parent = &ab; y.parent = &c; z.parent = &first; w.parent = &bc;
    expect(key({x, y}) != key({z, w}), "Parent string lengths not framed");
    return 0;
}
