#include "tests/review_fixture.h"
#include <iostream>
int main(int argc,char** argv){try {
    if(argc!=2)return 2;
    study_meta::SqliteResults store(argv[1]);store.seed_demo();
    for(std::uint64_t key=901;key<=905;++key)
        study_authority_test::require(store.put(study_review_test::match(key,key%2==0)).status==study_meta::SqlStatus::accepted);
    std::cout<<"fixture: actual rule results stored; repeat this command to verify identical keys\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
