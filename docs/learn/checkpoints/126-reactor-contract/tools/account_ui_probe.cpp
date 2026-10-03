#include "client/account_controller.h"
#include "meta/account_task.h"
#include <charconv>
#include <iostream>
#include <thread>
int main(int argc,char** argv){
    if(argc!=3 && !(argc==4 && std::string_view(argv[3])=="--hold"))return 2;
    const std::string_view input=argv[1];int port=0;
    const auto parsed=std::from_chars(input.data(),input.data()+input.size(),port);
    if(parsed.ec!=std::errc{}||parsed.ptr!=input.data()+input.size()||port<1||port>65535)return 2;
    study_account_ui::Controller<study_meta::AccountTask> controller(
        std::make_unique<study_meta::AccountTask>(port,std::filesystem::u8path(argv[2])));
    if(!controller.request())return 3;
    std::size_t frames=0;
    while(!controller.poll()){
        if(controller.request())return 4; // duplicate input must not start another job
        ++frames;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    const auto& view=controller.view();
    std::cout<<"frames="<<frames<<" status="<<int(view.status)<<" unsaved="<<view.has_unsaved_key;
    if(view.profile)std::cout<<" id="<<view.profile->player_id;
    std::cout<<"\n";
    std::cout.flush();
    if(argc==4){std::string line;std::getline(std::cin,line);}
    return view.status==study_account_ui::Status::online?0:5;
}
