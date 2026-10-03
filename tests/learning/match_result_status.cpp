#include "net/match_result.h"
#include <cstdlib>
#include <cstring>
#include <iostream>
int main(){
    for(unsigned raw=0;raw<256;++raw){
        const auto status=static_cast<net::ResultStatus>(raw);
        const bool confirmed=status==net::ResultStatus::Applied || status==net::ResultStatus::Draw;
        if(net::rating_after_result(300,0,status)!=(confirmed?0:300))return 1;
        if(net::rating_after_result(0,300,status)!=(confirmed?300:0))return 1;
        if(std::strlen(net::result_status_text(status))==0)return 1;
    }
    if(std::strstr(net::result_status_text(net::ResultStatus::SaveFailed),"not confirmed")==nullptr)return 1;
    if(std::strstr(net::result_status_text(net::ResultStatus::Unknown),"did not provide")==nullptr)return 1;
    std::cout<<"current result status: only confirmed profile updates; unknown/failure preserve profile\n";
}
