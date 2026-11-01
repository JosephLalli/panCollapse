#include "compact_identity_text.hpp"

#include <cassert>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

using pancollapse::CompactIdentityText;
using pancollapse::CompactIdentityTextPool;

void check(bool condition) {
    if (!condition) throw std::runtime_error("compact identity text test failed");
}

namespace pancollapse::test {
void unpolluted_string_operations() {
    const std::string_view view = "U";
    const std::string text = "U";
    check(view == "U" && text != std::string_view("V"));
}
}  // namespace pancollapse::test

int main() {
    static_assert(sizeof(void*) != 8 || sizeof(CompactIdentityText) == 8);

    CompactIdentityText empty;
    CompactIdentityText alpha("alpha");
    CompactIdentityText from_string(std::string("alpha"));
    CompactIdentityText from_view(std::string_view("alpha"));
    check(empty.empty() && empty.size() == 0);
    check(alpha == from_string && alpha == from_view && alpha == "alpha");
    check("alpha" == alpha && alpha == std::string("alpha"));
    check(alpha < "beta" && "aardvark" < alpha && alpha < std::string("beta"));
    check(alpha.str() == "alpha" && alpha.view() == "alpha");
    const std::string copied_string(alpha);
    const std::string_view copied_view(alpha);
    check(copied_string == "alpha" && copied_view == "alpha");
    check(alpha.substr(1, 3) == "lph" && alpha.find("ph") == 2 && alpha.rfind("a") == 4);
    check(alpha.find('p') == 2 && alpha.rfind('a') == 4);
    check(alpha.front() == 'a' && alpha.back() == 'a' && alpha[1] == 'l');
    check(std::string(alpha.data()) == "alpha" && std::string(alpha.c_str()) == "alpha");
    check((alpha + "-x") == "alpha-x" && ("x-" + alpha) == "x-alpha");
    check((alpha + std::string("-x")) == "alpha-x" && (std::string("x-") + alpha) == "x-alpha");
    check((alpha + from_string) == "alphaalpha");
    std::ostringstream output;
    output << alpha;
    check(output.str() == "alpha");

    const std::string& stable = alpha;
    CompactIdentityText copy = alpha;
    CompactIdentityText moved = std::move(copy);
    check(stable == "alpha" && moved.data() == alpha.data());
    moved = moved;
    auto* same_object = &moved;
    moved = std::move(*same_object);
    check(moved == "alpha");
    moved = std::string_view("gamma");
    check(moved == "gamma" && alpha == "alpha");

    CompactIdentityText survivor;
    {
        CompactIdentityTextPool pool;
        const CompactIdentityText one = pool.intern("shared");
        const CompactIdentityText two = pool.intern(std::string_view("shared"));
        check(pool.size() == 1 && one.data() == two.data());
        survivor = one;
        pool.clear();
        check(survivor == "shared");
    }
    check(survivor == "shared");
    pancollapse::test::unpolluted_string_operations();
}
