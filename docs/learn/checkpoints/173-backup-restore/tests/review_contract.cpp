#include "meta/review_reader.h"
#include "tests/review_fixture.h"
#include <iostream>
#include <functional>
using namespace study_meta;
using study_authority_test::require;
static void rejects(const std::function<void()>& f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}require(caught);}
int main(int argc,char** argv){try {
    if(argc!=2)return 2;
    const ReviewPolicy policy{3,2,2};
    require(review_flags({2,1,1,1},policy)==0);
    require(review_flags({3,2,0,2},policy)==(repeated_pair|one_sided|short_results));
    require(review_flags({3,0,0,3},policy)==(repeated_pair|short_results));
    require(review_flags({UINT64_MAX,UINT64_MAX,0,0},policy)==(repeated_pair|one_sided));
    rejects([&]{review_flags({UINT64_MAX,UINT64_MAX,1,0},policy);});
    rejects([&]{review_flags({1,0,0,2},policy);});
    rejects([&]{review_flags({}, {0,1,1});});
    ReviewSample rows{{{3,202,101,101,9},{2,101,202,101,10},{1,202,101,0,11}},3,false};
    auto report=review_sample(rows,policy,10);
    require(report.pairs.size()==1 && report.pairs[0].players==std::array<std::uint64_t,2>{101,202});
    require(report.pairs[0].counts.games==3 && report.pairs[0].counts.lower_wins==2 && report.pairs[0].counts.short_games==2);
    require(report.pairs[0].flags==7 && report.newest==3 && report.oldest==1);
    auto malformed=rows;malformed.rows[1].row=3;rejects([&]{review_sample(malformed,policy,10);});
    malformed=rows;malformed.rows[0].winner=999;rejects([&]{review_sample(malformed,policy,10);});
    malformed=rows;malformed.rows[0].ticks=0;rejects([&]{review_sample(malformed,policy,10);});
    require(review_sample({{},4,false},policy,10).pairs.empty());
    rejects([&]{review_sample({{},4,true},policy,10);});
    rows={{{1,UINT64_MAX,1,UINT64_MAX,UINT64_MAX}},1,false};
    require(review_sample(rows,policy,10).pairs[0].counts.upper_wins==1);
    SqliteResults store(argv[1]);store.seed_demo();
    ReviewReader reader(argv[1]);require(reader.read_only() && reader.read(4).rows.empty());
    rejects([&]{reader.read(0);});rejects([&]{reader.read(kMaxReviewRows+1);});
    for(std::uint64_t key=1;key<=3;++key)require(store.put(study_review_test::match(key,key%2==0)).status==SqlStatus::accepted);
    const auto balance=store.balance(101);const auto career=store.career(101);
    auto sample=reader.read(2);require(sample.has_older && sample.rows.size()==2);
    report=review_sample(sample,policy,1000);
    require(report.rows_read==2 && report.pairs.size()==1 && !(report.pairs[0].flags&repeated_pair));
    const auto all=reader.read(3);require(!all.has_older && all.rows.size()==3);
    require(review_sample(all,policy,1000).pairs[0].flags==7);
    // A receipt retry does not create a new row or a new observation.
    require(store.put(study_review_test::match(3,false)).status==SqlStatus::accepted);
    require(reader.read(10).rows.size()==3 && store.count()==3 && store.balance(101)==balance);
    require(store.career(101).rp==career.rp && store.career(101).xp==career.xp);
    {ReviewReader reopened(argv[1]);require(reopened.read(10).rows.size()==3);}
    std::cout<<"thresholds, impossible counters, canonical pairs, draw, order, sample scope, read-only DB, real simulation and unchanged rewards passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
