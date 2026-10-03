// Lesson 13 contract tests: explicit checks that survive NDEBUG.
// Do NOT use assert here; assert is compiled out in release builds.
#include "renderer/mesh.h"

#include <cstddef>
#include <cstdio>
#include <vector>

namespace {
int g_failures = 0;

void require(bool ok, const char* what) {
    if (ok) {
        std::printf("ok: %s\n", what);
    } else {
        std::printf("FAIL: %s\n", what);
        ++g_failures;
    }
}
} // namespace

int main() {
    using namespace study_mesh;

    // Coordinates and order.
    const Triangle tri = make_triangle();
    require(tri.size() == 3, "triangle has 3 vertices");
    require(tri[0].x == -0.5f && tri[0].y == -0.5f, "v0 = (-0.5,-0.5)");
    require(tri[1].x ==  0.5f && tri[1].y == -0.5f, "v1 = (0.5,-0.5)");
    require(tri[2].x ==  0.0f && tri[2].y ==  0.5f, "v2 = (0.0,0.5)");

    // Member offsets, stride, byte count.
    require(offsetof(Vertex2, x) == 0, "offsetof(x) == 0");
    require(offsetof(Vertex2, y) == sizeof(float), "offsetof(y) == sizeof(float)");
    require(sizeof(Vertex2) == 2 * sizeof(float), "sizeof(Vertex2) == 2*sizeof(float)");
    require(sizeof(tri[0]) == sizeof(Vertex2), "element stride == sizeof(Vertex2)");
    require(tri.size() * sizeof(tri[0]) == byte_count(tri),
            "byte count == size * stride");
    require(tri.size() * sizeof(tri[0]) == 24, "byte count == 24");

    // Empty vector.
    const std::vector<Vertex2> none;
    require(none.size() == 0, "default vector size == 0");

    // reserve / push / clear capacity rules.
    std::vector<Vertex2> verts;
    verts.reserve(3);
    require(verts.size() == 0, "reserve does not change size");
    require(verts.capacity() >= 3, "reserve raises capacity to >= 3");

    verts.push_back(tri[0]);
    verts.push_back(tri[1]);
    verts.push_back(tri[2]);
    require(verts.size() == 3, "three pushes -> size 3");
    require(verts.capacity() >= 3, "capacity still >= 3");

    // Changed element copy must be non-aliasing.
    const Vertex2 snapshot = verts[0];
    verts[0].x = 1.25f;
    require(verts[0].x == 1.25f, "element writable in place");
    require(snapshot.x == -0.5f, "value copy does not alias element");

    const auto saved_capacity = verts.capacity();
    verts.clear();
    require(verts.size() == 0, "clear resets size to 0");
    require(verts.capacity() == saved_capacity, "clear preserves capacity");
    // Intentionally no verts.data() access after clear().

    if (g_failures == 0) {
        std::printf("ALL CHECKS PASSED\n");
        return 0;
    }
    std::printf("%d CHECK(S) FAILED\n", g_failures);
    return 1;
}
