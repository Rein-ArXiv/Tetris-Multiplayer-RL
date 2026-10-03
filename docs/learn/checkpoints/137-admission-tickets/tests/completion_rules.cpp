#include "net/completion_rules.h"
#include <cstdio>
using namespace study_net;
int main() {
    if(classify_submission(0,123,997)!=Submission::immediate)return 1;
    if(classify_submission(-1,997,997)!=Submission::pending)return 2;
    if(classify_submission(-1,10054,997)!=Submission::rejected)return 3;
    if(classify_completion(true,2,995,995)!=CompletionKind::data)return 4;
    if(classify_completion(true,0,0,995)!=CompletionKind::eof)return 5;
    if(classify_completion(false,9,995,995)!=CompletionKind::cancelled)return 6;
    if(classify_completion(false,0,10054,995)!=CompletionKind::error)return 7;
    std::puts("positive-length TCP submission/completion rules passed");
}
