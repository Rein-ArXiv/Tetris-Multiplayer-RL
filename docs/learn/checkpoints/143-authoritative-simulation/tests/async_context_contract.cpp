#include <boost/asio.hpp>
#include <cstdlib>
#include <iostream>
#include <memory>
#define CHECK(x) do{if(!(x)){std::cerr<<__LINE__<<": " #x "\n";std::exit(1);}}while(false)
namespace asio=boost::asio;
int main() {
    // stop prevents dispatch, but does not erase queued handlers or their captures.
    std::weak_ptr<int> weak;
    {
        asio::io_context io;
        auto owner=std::make_shared<int>(7);weak=owner;bool ran=false;
        asio::post(io,[hold=owner,&ran]{ran=(*hold==7);});owner.reset();
        io.stop();CHECK(io.run()==0);CHECK(!ran&&!weak.expired());
        io.restart();CHECK(io.run()==1);CHECK(ran&&weak.expired());
    }
    // Context destruction releases an undispatched capture without invoking it.
    bool ran=false;
    {
        asio::io_context io;auto owner=std::make_shared<int>(8);weak=owner;
        asio::post(io,[hold=owner,&ran]{ran=(*hold==8);});owner.reset();
        io.stop();CHECK(!weak.expired());
    }
    CHECK(!ran&&weak.expired());
    // A pending timer's cancellation queues an aborted completion; the owner stays alive.
    asio::io_context io;asio::steady_timer timer(io);
    auto owner=std::make_shared<int>(9);weak=owner;unsigned calls=0;
    timer.expires_after(std::chrono::hours(1));
    timer.async_wait([hold=owner,&calls](boost::system::error_code error){
        CHECK(error==asio::error::operation_aborted);CHECK(*hold==9);++calls;
    });owner.reset();
    CHECK(timer.cancel()==1);CHECK(!weak.expired()&&calls==0);
    io.run();CHECK(calls==1&&weak.expired());
    std::cout<<"stop/restart, undispatched ownership and cancelled completion passed\n";
}
