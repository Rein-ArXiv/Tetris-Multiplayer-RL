#include "meta/review_reader.h"
#include <iostream>
static std::uint64_t number(const char* raw) {
    const std::string text(raw);std::uint64_t out=0;
    const auto parsed=std::from_chars(text.data(),text.data()+text.size(),out);
    if(parsed.ec!=std::errc{} || parsed.ptr!=text.data()+text.size() || !out)
        throw std::invalid_argument("positive integer policy required");
    return out;
}
int main(int argc,char** argv){try {
    if(argc!=7){std::cerr<<"review_results DB ROW_LIMIT REPEAT ONE_SIDED SHORT_COUNT SHORT_TICKS\n";return 2;}
    const auto limit=number(argv[2]);
    if(limit>study_meta::kMaxReviewRows)throw std::invalid_argument("row limit too large");
    study_meta::ReviewReader db(argv[1]);
    const auto report=study_meta::review_sample(db.read(static_cast<std::size_t>(limit)),
        {number(argv[3]),number(argv[4]),number(argv[5])},number(argv[6]));
    std::cout<<"최근 저장 행 표본="<<report.rows_read<<" 요청="<<report.requested
             <<" ID 범위="<<report.oldest<<".."<<report.newest<<" 이전 행 있음="<<report.has_older<<'\n';
    std::cout<<"정책 v1: 반복="<<report.policy.repeat_matches<<" 편향="<<report.policy.one_sided_wins
             <<" 짧음="<<report.policy.short_matches<<" 짧은 틱 상한="<<report.short_tick_limit<<'\n';
    for(const auto& pair:report.pairs) {
        std::cout<<"계정쌍 "<<pair.players[0]<<','<<pair.players[1]<<" 경기="<<pair.counts.games<<" 관찰=";
        if(!pair.flags)std::cout<<"표본 내 신호 없음";
        for(auto flag:{study_meta::repeated_pair,study_meta::one_sided,study_meta::short_results})
            if(pair.flags&flag)std::cout<<study_meta::review_label(flag)<<"; ";
        std::cout<<'\n';
    }
    std::cout<<"검토용 관찰 결과입니다. 제재나 지급 변경은 수행하지 않습니다.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
