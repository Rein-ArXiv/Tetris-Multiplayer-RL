#include "meta/http_sender.h"
#include "meta/retry_submission.h"
#include <thread>
#include "tools/sample_match.h"
#include <charconv>
#include <cstdio>
#include <cstring>
#include <limits>
// A trusted local simulation fixture supplies a result; no client win claim is read.
int main(int argc,char** argv) {
    if(argc!=3)return 2;
    int port=0;std::uint64_t key=0;
    auto p=std::from_chars(argv[1],argv[1]+std::strlen(argv[1]),port);
    auto k=std::from_chars(argv[2],argv[2]+std::strlen(argv[2]),key);
    if(p.ec!=std::errc{} || *p.ptr || k.ec!=std::errc{} || *k.ptr || port<1 || port>65535 || key==0)return 2;
    const auto record=sample_match(key);if(!record)return 1;
    study_net::MatchSubmission submission(3);
    if(!submission.prepare(*record))return 1;
    const auto result=study_meta::retry_submission(submission,study_meta::HttpSender(port),
        []{return std::chrono::steady_clock::now();},
        [](std::chrono::milliseconds delay){std::this_thread::sleep_for(delay);},
        std::chrono::milliseconds{5000});
    if(result!=study_meta::RetryExit::confirmed) {
        std::puts(result==study_meta::RetryExit::stopped ? "automatic retry stopped" : "unconfirmed: budget exhausted");
        return 3;
    }
    std::printf("confirmed key=%llu row=%llu ticks=%llu\n",static_cast<unsigned long long>(key),
        static_cast<unsigned long long>(submission.receipt()->row),static_cast<unsigned long long>(record->ticks));
}
