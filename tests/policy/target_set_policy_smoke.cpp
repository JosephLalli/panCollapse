#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

static std::vector<std::string> sorted_unique(std::vector<std::string> targets) {
    std::sort(targets.begin(), targets.end());
    targets.erase(std::unique(targets.begin(), targets.end()), targets.end());
    return targets;
}

int main() {
    const std::vector<std::string> observed = sorted_unique({"TX_B", "TX_A", "TX_A"});
    const std::vector<std::string> expected = {"TX_A", "TX_B"};

    if (observed != expected) {
        std::cerr << "target set sort/dedup smoke failed\n";
        return 1;
    }

    return 0;
}
