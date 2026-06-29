#include <handlegraph/path_position_handle_graph.hpp>
#include <vg/io/stream.hpp>
#include <vg/vg.pb.h>
#include <xg.hpp>

#include <functional>
#include <iostream>
#include <istream>
#include <type_traits>

int main() {
    static_assert(std::is_base_of_v<handlegraph::PathPositionHandleGraph, xg::XG>);

    vg::MultipathAlignment alignment;

    xg::XG graph;
    handlegraph::PathPositionHandleGraph* positioned_graph = &graph;

    using ForEachFn = void (*)(
        std::istream&,
        const std::function<void(vg::MultipathAlignment&)>&,
        const std::function<void(size_t, size_t)>&);
    ForEachFn for_each_alignment = &vg::io::for_each<vg::MultipathAlignment>;

    if (alignment.subpath_size() != 0 || positioned_graph == nullptr || for_each_alignment == nullptr) {
        std::cerr << "VG link smoke failed\n";
        return 1;
    }

    return 0;
}
