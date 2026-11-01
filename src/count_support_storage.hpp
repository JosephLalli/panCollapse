#pragma once

#include <cstdint>
#include <functional>

#include <boost/container/flat_set.hpp>
#include <boost/container/small_vector.hpp>

namespace pancollapse::count_support {

using SupportIds = boost::container::small_vector<std::uint32_t, 1>;
using SupportSet = boost::container::flat_set<std::uint32_t, std::less<std::uint32_t>, SupportIds>;

}  // namespace pancollapse::count_support
