#include "atlas_fake.h"
#include "renderer/glyph_cache.h"
#include "text/cached_paragraph.h"
#include "text/paragraph.h"
#include <cmath>
static bool close(float a,float b){return std::abs(a-b)<.0001f;}
int main(int argc,char** argv) {
    CHECK(argc==2);const auto path=std::filesystem::u8path(argv[1]);
    study_font::GlyphCache cpu;CHECK(cpu.load_trusted(path));
    study_font::Font reference;CHECK(reference.load_trusted(path));
    const auto label=study_labels::decode(u8"AV A\n가\n");CHECK(label);
    const auto logical=study_font::prepare_paragraph(reference,*label,22);CHECK(logical);
    auto gl=texture_api();
    {
        study_atlas::GlyphAtlas atlas(gl,512,512);CHECK(atlas.init());
        study_text::GpuGlyphCache gpu(atlas);
        for(double density:{.5,1.,1.0625,1.25,2.,3.375}) {
            const auto line=study_font::prepare_cached_paragraph(cpu,*label,22,density);CHECK(line);
            CHECK(line->count==logical->count&&line->layout.lines==logical->layout.lines);
            CHECK(line->layout.width==logical->layout.width&&line->layout.height==logical->layout.height);
            for(std::size_t i=0;i<line->count;++i) {
                const auto& item=line->items[i];
                CHECK(item.pen_x==logical->items[i].pen_x&&item.baseline==logical->items[i].baseline);
                const auto region=gpu.get(item.glyph);CHECK(region&&atlas.contains(*region));
                CHECK(region->ink.w==item.glyph->bitmap.width&&region->ink.h==item.glyph->bitmap.height);
            }
            const auto attempts=cpu.stats().raster_attempts;const auto calls=f.call,uploads=f.uploads;
            const auto warm=study_font::prepare_cached_paragraph(cpu,*label,22,density);CHECK(warm);
            for(std::size_t i=0;i<warm->count;++i)CHECK(warm->items[i].glyph==line->items[i].glyph&&gpu.get(warm->items[i].glyph));
            CHECK(cpu.stats().raster_attempts==attempts&&f.call==calls&&f.uploads==uploads);
        }
        CHECK(!study_font::prepare_cached_paragraph(cpu,{},16,0));
        CHECK(!study_font::prepare_cached_paragraph(cpu,{},129,1));
        auto invalid=*label;invalid.count=17;CHECK(!study_font::prepare_cached_paragraph(cpu,invalid,16,1));
        const auto cr=study_labels::decode("\r"),tab=study_labels::decode("\t");CHECK(cr&&tab);
        CHECK(!study_font::prepare_cached_paragraph(cpu,*cr,16,1)&&!study_font::prepare_cached_paragraph(cpu,*tab,16,1));
        const auto a=cpu.get(U'A',16,1);CHECK(a);const auto r=gpu.get(a);CHECK(r);
        CHECK(atlas.clear()&&!atlas.contains(*r));
        const auto before=f.uploads;const auto fresh=gpu.get(a);CHECK(fresh&&fresh->revision!=r->revision&&gpu.size()==1&&f.uploads==before+1);
        // Font reload produces a new allocation even when packed keys are equal.
        CHECK(cpu.load_trusted(path));const auto new_a=cpu.get(U'A',16,1);CHECK(new_a&&new_a!=a&&new_a->key==a->key);
        CHECK(gpu.get(new_a)&&gpu.size()==2&&f.uploads==before+2);
        CHECK(gpu.get(a)->ink.x==fresh->ink.x); // old immutable snapshot still means old pixels
        CHECK(atlas.clear());gpu.clear();
        const auto b=cpu.get(U'B',16,1);CHECK(b);
        f.upload_error=true;CHECK(!gpu.get(b)&&gpu.size()==0);f.upload_error=false;
        const auto retry=gpu.get(b);CHECK(retry&&retry->ink.x==1&&retry->ink.y==1&&gpu.size()==1);
        f.upload_error=true;CHECK(!atlas.clear()&&!gpu.get(b)&&gpu.size()==0);f.upload_error=false;
        CHECK(atlas.clear()&&gpu.get(b)&&gpu.size()==1);
        // Dropping lookup entries alone cannot free the shelf allocation.
        const auto kept=gpu.get(b);gpu.clear();CHECK(atlas.contains(*kept)&&gpu.size()==0);
        const auto duplicate=gpu.get(b);CHECK(duplicate&&duplicate->ink.x!=kept->ink.x);
        CHECK(atlas.clear());gpu.clear();cpu.clear();
        for(char32_t cp=32;cp<96;++cp) {const auto entry=cpu.get(cp,16,1);CHECK(entry&&gpu.get(entry));}
        CHECK(gpu.size()==64);const auto full_calls=f.call;
        CHECK(gpu.get(cpu.get(U'A',16,1))&&f.call==full_calls);
        cpu.clear();const auto extra=cpu.get(U'가',16,1);CHECK(extra&&!gpu.get(extra)&&f.call==full_calls);
        CHECK(atlas.clear()&&gpu.get(extra)&&gpu.size()==1);
    }
    CHECK(f.live.empty());
    // Page capacity is independent from the cache's entry capacity.
    {
        study_atlas::GlyphAtlas atlas(gl,4,4);CHECK(atlas.init());study_text::GpuGlyphCache gpu(atlas);
        const auto a=cpu.get(U'A',16,1);CHECK(a);const auto calls=f.call;
        CHECK(!gpu.get(a)&&gpu.size()==0&&f.call==calls&&atlas.revision()!=0);
        CHECK(!gpu.get(a)&&gpu.size()==0&&f.call==calls); // no hidden clear or poisoned hit
    }
    CHECK(f.live.empty());
    CHECK(close(cpu.get(U'V',22,3.375)->logical_per_device,22.f/74.f));
    std::puts("GPU cache: warm zero GL/raster work; density-independent pens, revision/reload identity, upload retry, clear recovery and capacity passed");
}
