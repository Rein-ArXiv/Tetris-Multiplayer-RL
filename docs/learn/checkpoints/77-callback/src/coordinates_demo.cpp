#include "renderer/coordinates.h"
#include "renderer/projection_cases.h"
#include "renderer/mesh.h"
#include <cstdio>
int main(int argc, char** argv) {
    const auto* experiment = study_projection::find_case(argc > 1 ? argv[1] : "base");
    if (argc > 2 || !experiment) {
        std::fputs("Usage: coordinates_demo [base|w2|scaled|oversized]\n", stderr);
        return 2;
    }
    const study_coordinates::Viewport viewport{10,20,200,100};
    std::printf("%s: viewport=(10,20,200,100); point predicate is not triangle visibility\n",experiment->name);
    for (const auto& v : study_mesh::make_triangle()) {
        const auto clip=study_projection::clip_for(*experiment,v.x,v.y);
        const auto ndc=study_coordinates::perspective_divide(clip);
        if (!ndc) return 1;
        const auto window=study_coordinates::to_window(*ndc,viewport);
        if (!window) return 1;
        std::printf("clip=(%.2f,%.2f,%.2f,%.2f) inside=%d NDC=(%.2f,%.2f,%.2f) window=(%.2f,%.2f,%.2f)\n",
            clip.x,clip.y,clip.z,clip.w,study_coordinates::inside_clip_volume(clip),
            ndc->x,ndc->y,ndc->z,window->x,window->y,window->depth);
    }
}
