#include "client/ranking_controller.h"
#include "meta/ranking_task.h"
#include <charconv>
#include <iostream>
#include <thread>
int main(int argc,char** argv) {
    if(argc!=2)return 2;
    const std::string_view input=argv[1];int port=0;
    const auto parsed=std::from_chars(input.data(),input.data()+input.size(),port);
    if(parsed.ec!=std::errc{} || parsed.ptr!=input.data()+input.size() || port<1 || port>65535)return 2;
    study_ranking_ui::Controller<study_meta::RankingTask> controller(std::make_unique<study_meta::RankingTask>(port));
    if(!controller.request())return 3;
    while(!controller.poll())std::this_thread::sleep_for(std::chrono::milliseconds(1));
    if(controller.view().status!=study_ranking_ui::Status::ready){std::cout<<"ranking unavailable\n";return 4;}
    std::cout<<study_meta::ranking_json(controller.view().rows).dump()<<"\n";
}
