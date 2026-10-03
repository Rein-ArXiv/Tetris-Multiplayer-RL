#include "simulation/state_hash.h"
#include "presentation/accent_noise.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <limits>
#define CHECK(expr) do { if(!(expr)){std::fprintf(stderr,"CHECK failed line %d: %s\n",__LINE__,#expr);std::exit(1);} } while(false)
using study_hash::Bytes;
using study_round::Round;
using study_catalog::Kind;
static bool same(const study_hash::RoundBytes& a,const study_hash::RoundBytes& b) {
    return a.ok()&&b.ok()&&a.size()==b.size()&&std::equal(a.data(),a.data()+a.size(),b.data());
}
int main() {
    Bytes<0> empty;CHECK(empty.digest()==0xcbf29ce484222325ull);empty.u8(0);CHECK(!empty.ok()&&!empty.digest());
    Bytes<6> text;for(unsigned char c : {'f','o','o','b','a','r'})text.u8(c);
    CHECK(text.digest()==0x85944171f73967e8ull);
    Bytes<16> encoded;encoded.u32(0x12345678);encoded.i32(-2);encoded.u64(0x0102030405060708ull);
    const unsigned char expected[]{0x78,0x56,0x34,0x12,0xfe,0xff,0xff,0xff,8,7,6,5,4,3,2,1};
    CHECK(encoded.size()==16&&std::equal(encoded.data(),encoded.data()+16,expected));
    encoded.u8(1);CHECK(!encoded.digest()&&encoded.size()==16);encoded.u32(7);CHECK(encoded.size()==16);
    Bytes<3> short32;short32.u32(1);CHECK(!short32.ok()&&short32.size()==0);
    Bytes<7> short64;short64.u64(1);CHECK(!short64.ok()&&short64.size()==0);
    Bytes<4> minimum;minimum.i32(std::numeric_limits<std::int32_t>::min());CHECK(minimum.data()[3]==0x80&&minimum.data()[0]==0);
    // Canonical queue contents are independent of physical ring placement.
    study_next::Queue a,b;CHECK(a.push(Kind::I)&&a.push(Kind::O)&&a.push(Kind::T));
    CHECK(b.push(Kind::Z)&&b.pop()==Kind::Z&&b.push(Kind::I)&&b.push(Kind::O)&&b.push(Kind::T));
    Bytes<4> qa,qb;study_hash::append_queue(qa,a);study_hash::append_queue(qb,b);
    CHECK(qa.digest()==qb.digest()&&std::equal(qa.data(),qa.data()+4,qb.data()));
    // Same visible current+preview, different unseen fifth scripted kind.
    std::array<Kind,7> pattern{Kind::T,Kind::T,Kind::T,Kind::T,Kind::I,Kind::I,Kind::I};
    auto source=study_next::ScriptedSource::from_pattern(pattern,5);CHECK(source);
    auto left=Round::create(study_grid::Grid{},*source);CHECK(left);
    pattern[4]=Kind::Z;source=study_next::ScriptedSource::from_pattern(pattern,5);CHECK(source);
    auto right=Round::create(study_grid::Grid{},*source);CHECK(right);
    CHECK(left->kind()==right->kind()&&left->source_cursor()==right->source_cursor());
    CHECK(study_hash::state_hash(*left)!=study_hash::state_hash(*right));
    CHECK(left->tick(0,false,false,true)==study_round::Step::locked);
    CHECK(right->tick(0,false,false,true)==study_round::Step::locked);
    CHECK(left->next().peek(2)!=right->next().peek(2));
    // Inactive scripted tail does not participate.
    auto s1=study_next::ScriptedSource::from_pattern(pattern,1);pattern[6]=Kind::L;
    auto s2=study_next::ScriptedSource::from_pattern(pattern,1);CHECK(s1&&s2);
    auto r1=Round::create(study_grid::Grid{},*s1),r2=Round::create(study_grid::Grid{},*s2);CHECK(r1&&r2);
    CHECK(same(study_hash::state_bytes(*r1),study_hash::state_bytes(*r2)));
    auto original=Round::create_seeded(study_grid::Grid{},1);CHECK(original);
    auto clock=*original;CHECK(clock.tick(0)==study_round::Step::waiting);
    CHECK(clock.active()->origin.row==original->active()->origin.row);
    CHECK(study_hash::state_hash(clock)!=study_hash::state_hash(*original));
    auto pending=*original;CHECK(pending.add_garbage(1));CHECK(study_hash::state_hash(pending)!=study_hash::state_hash(*original));
    // Fault injection changes owned state through a read-only observation;
    // only the test casts away const, and the underlying object is non-const.
    auto bag_fault=*original;auto* seeded=const_cast<study_next::SeededBagSource*>(bag_fault.source().seeded());
    auto& bag=const_cast<study_bag::SevenBag&>(seeded->bag());CHECK(bag.take(0));
    CHECK(bag_fault.source().seeded()->rng_state()==original->source().seeded()->rng_state());
    CHECK(study_hash::state_hash(bag_fault)!=study_hash::state_hash(*original));
    auto bag_order=*original;
    auto* order_source=const_cast<study_next::SeededBagSource*>(bag_order.source().seeded());
    study_bag::SevenBag alternate;
    for(int i=0;i<4;++i)CHECK(alternate.take(0));
    CHECK(alternate.remaining()==original->source().seeded()->bag().remaining());
    const_cast<study_bag::SevenBag&>(order_source->bag())=alternate;
    CHECK(study_hash::state_hash(bag_order)!=study_hash::state_hash(*original));
    auto soft=*original;const_cast<study_soft_drop::Counter&>(soft.soft_drop()).remaining=2;
    CHECK(study_hash::state_hash(soft)!=study_hash::state_hash(*original));
    auto hole=*original;auto& holes=const_cast<study_holes::HoleSource&>(hole.hole_source());(void)holes.next();
    CHECK(study_hash::state_hash(hole)!=study_hash::state_hash(*original));
    // End states remain encoded; failed/stopped ticks preserve their bytes.
    auto end=*original;CHECK(end.add_garbage(20));CHECK(end.tick(0,false,false,true)==study_round::Step::game_over);
    const auto ended=study_hash::state_bytes(end);CHECK(end.tick(0)==study_round::Step::stopped);CHECK(same(ended,study_hash::state_bytes(end)));
    for(std::uint64_t seed=0;seed<1000;++seed) {
        auto one=Round::create_seeded(study_grid::Grid{},seed);CHECK(one);auto two=*one;
        study_presentation::AccentNoise visual(seed);
        for(int tick=0;tick<60;++tick) {
            const auto before=study_hash::state_bytes(*one);(void)one->ghost();(void)study_hash::state_hash(*one);
            CHECK(same(before,study_hash::state_bytes(*one)));
            for(int i=0;i<tick%7;++i)(void)visual.sample();
            CHECK(one->tick(tick%3-1,tick%5==0,tick%4==0,tick%13==0)==two.tick(tick%3-1,tick%5==0,tick%4==0,tick%13==0));
            CHECK(same(study_hash::state_bytes(*one),study_hash::state_bytes(two)));
            CHECK(study_hash::state_hash(*one)==study_hash::state_hash(two));
            const auto saved=study_hash::state_bytes(*one);auto result=one->tick(2);
            CHECK(result==study_round::Step::invalid||result==study_round::Step::stopped);
            CHECK(same(saved,study_hash::state_bytes(*one)));
        }
    }
    std::puts("Canonical bytes, vectors, overflow, queue layout, hidden state, source tails, copy histories and observer purity passed.");
}
