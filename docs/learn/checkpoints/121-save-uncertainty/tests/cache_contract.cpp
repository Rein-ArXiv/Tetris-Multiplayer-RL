#include "text/glyph_cache.h"
#include "font_reference.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"CHECK line %d: %s\n",__LINE__,#e);std::exit(1);}}while(false)
static bool close(float a,float b){return std::abs(a-b)<.0001f;}
int main(int argc,char** argv) {
    CHECK(argc==2);
    const auto path=std::filesystem::u8path(argv[1]);
    study_font::GlyphCache cache;
    CHECK(!cache.get(U'A',16,1)&&cache.size()==0&&cache.stats().raster_attempts==0);
    CHECK(cache.load_trusted(path));
    study_font::Font direct;CHECK(direct.load_trusted(path));
    std::size_t checked=0;
    for(int logical:{1,16,22,128}) {
        for(double scale:{.5,1.,1.0624,1.0625,1.25,2.,3.375}) {
            cache.clear();
            for(const auto& ref:font_reference) {
                const auto cp=char32_t(ref.cp);
                const auto entry=cache.get(cp,logical,scale);CHECK(entry);
                const auto planned=font_raster::plan(ref.cp,logical,scale);CHECK(planned);
                const float logical_scale=float(logical)/float(font_ascent-font_descent);
                const float device_scale=float(planned->device_height)/float(font_ascent-font_descent);
                CHECK(close(entry->advance,ref.advance*logical_scale));
                CHECK(close(entry->left_bearing,ref.lsb*logical_scale));
                CHECK(entry->bitmap.index==ref.index&&entry->bitmap.missing==(ref.index==0));
                const int x0=int(std::floor(ref.x0*device_scale)),y0=int(std::floor(-ref.y1*device_scale));
                const int x1=int(std::ceil(ref.x1*device_scale)),y1=int(std::ceil(-ref.y0*device_scale));
                CHECK(entry->bitmap.xoff==x0&&entry->bitmap.yoff==y0&&entry->bitmap.width==x1-x0&&entry->bitmap.height==y1-y0);
                const auto raw=direct.rasterize_device(cp,planned->device_height);CHECK(raw);
                CHECK(raw->coverage==entry->bitmap.coverage);
                CHECK(close(entry->logical_per_device,float(logical)/planned->device_height));
                const auto attempts=cache.stats().raster_attempts,hits=cache.stats().hits;
                CHECK(cache.get(cp,logical,scale)==entry);
                CHECK(cache.stats().raster_attempts==attempts&&cache.stats().hits==hits+1);
                ++checked;
            }
        }
    }
    cache.clear();
    const auto a=cache.get(U'A',16,1);CHECK(a);
    CHECK(cache.get(U'A',16,1.01)==a);
    const auto doubled=cache.get(U'A',16,2);CHECK(doubled&&doubled!=a&&doubled->advance==a->advance);
    const auto bigger=cache.get(U'A',32,1);CHECK(bigger&&bigger!=doubled&&bigger->bitmap.coverage==doubled->bitmap.coverage);
    CHECK(bigger->advance==2*a->advance);
    const auto small=cache.get(U'V',1,1);CHECK(small&&cache.get(U'V',1,1.25)==small); // distinct quantized scales, same integer height
    CHECK(cache.get(U'A',22,3.375)->logical_per_device==22.f/74.f);
    const auto count=cache.size(),attempts=cache.stats().raster_attempts;
    for(double scale:{0.,-1.,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::max()})CHECK(!cache.get(U'A',16,scale));
    CHECK(!cache.get(U'A',0,1)&&!cache.get(U'A',129,1)&&!cache.get(char32_t(0xD800),16,1));
    CHECK(cache.size()==count&&cache.stats().raster_attempts==attempts);
    CHECK(!cache.load_trusted(path.parent_path()/"absent-cache-font.ttf"));
    CHECK(cache.size()==count&&cache.get(U'A',16,1)==a);
    CHECK(cache.load_trusted(path)&&cache.size()==0);
    const auto replaced=cache.get(U'A',16,1);CHECK(replaced&&replaced!=a&&replaced->bitmap.coverage==a->bitmap.coverage);
    cache.clear();CHECK(cache.size()==0&&!a->bitmap.coverage.empty()); // immutable snapshot still owned
    for(char32_t cp=32;cp<32+study_font::GlyphCache::capacity;++cp)CHECK(cache.get(cp,16,1));
    CHECK(cache.size()==study_font::GlyphCache::capacity);
    const auto full_attempts=cache.stats().raster_attempts;
    CHECK(!cache.get(U'가',16,1)&&cache.stats().raster_attempts==full_attempts);
    CHECK(cache.get(U'A',16,1)&&cache.stats().raster_attempts==full_attempts); // full cache still serves hits
    cache.clear();CHECK(cache.get(U'가',16,1));
    cache.clear();const auto failed=cache.stats().raster_attempts;
    CHECK(!cache.get(U'가',128,16)&&cache.size()==0); // 2048 metric height, bitmap axis exceeds 1024
    CHECK(!cache.get(U'가',128,16)&&cache.stats().raster_attempts==failed+2); // failed work was not memoized
    CHECK(cache.get(U'가',128,2));
    CHECK(!direct.rasterize(U'A',129)&&!direct.metrics(129)&&!direct.horizontal(U'A',129));
    CHECK(direct.rasterize_device(U'A',129));
    CHECK(!direct.rasterize_device(U'A',0)&&!direct.rasterize_device(U'A',2049));
    std::shared_ptr<const study_font::CachedGlyph> survivor;
    {study_font::GlyphCache owner;CHECK(owner.load_trusted(path));survivor=owner.get(U'g',16,2);CHECK(survivor);}
    CHECK(!survivor->bitmap.coverage.empty());
    std::printf("CPU cache: %zu independent metric/box cases; warm reuse, equal device heights, key separation, reload, bounds, capacity, snapshot lifetime and retry passed\n",checked);
}
