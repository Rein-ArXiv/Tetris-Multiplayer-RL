// Lesson 13 demo: inspect CPU vertex layout only. No GL calls here.
#include "renderer/mesh.h"

#include <cstddef>
#include <iostream>
#include <vector>

int main() {
    using namespace study_mesh;

    const Triangle tri = make_triangle();

    // Element count vs byte count (never sizeof(pointer)/sizeof(container)).
    const std::size_t count  = tri.size();
    const std::size_t stride = sizeof(tri[0]);
    const std::size_t bytes  = count * stride;

    std::cout << "count=" << count << '\n';
    std::cout << "stride=" << stride << '\n';
    std::cout << "bytes=" << bytes << '\n';
    std::cout << "offset x=" << offsetof(Vertex2, x) << '\n';
    std::cout << "offset y=" << offsetof(Vertex2, y) << '\n';

    for (std::size_t i = 0; i < count; ++i) {
        std::cout << "v" << i << "=("
                  << tri[i].x << ", " << tri[i].y << ")\n";
    }

    // Vector control object vs its heap buffer.
    std::vector<Vertex2> verts;
    verts.reserve(3);   // not a size limit; only reserves buffer space
    std::cout << "after reserve size=" << verts.size()
              << " cap>=3 " << (verts.capacity() >= 3) << '\n';

    verts.push_back(tri[0]);
    verts.push_back(tri[1]);
    verts.push_back(tri[2]);
    std::cout << "after push size=" << verts.size()
              << " cap>=3 " << (verts.capacity() >= 3) << '\n';
    std::cout << "copy matches="
              << (verts[0].x == tri[0].x && verts[0].y == tri[0].y) << '\n';

    const auto saved_capacity = verts.capacity();
    verts.clear();   // removes elements; former element pointers are invalid
    std::cout << "after clear size=" << verts.size() << '\n';
    std::cout << "capacity unchanged " << (verts.capacity() == saved_capacity) << '\n';

    return 0;
}
