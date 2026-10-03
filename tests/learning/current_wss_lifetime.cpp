// Observe the current externally-owned/joined adapter during pending read/write.
#include "net/wss_client.h"
#include <iostream>
#include <vector>
int main(int argc,char** argv) {
    if(argc!=2||!net::net_init())return 2;
    for(int experiment=0;experiment<4;++experiment) {
        auto socket=net::wss_connect(argv[1]);if(!socket.valid())return 3;
        auto transport=socket.transport;
        std::weak_ptr<net::StreamTransport> weak=transport;
        // Experiment bytes are deliberately not game credentials.
        if(experiment%2) {
            const std::vector<unsigned char> bytes(1024,17);
            if(!net::tcp_send_all(socket,bytes.data(),bytes.size()))return 4;
        }
        transport->close();transport->close();
        if(transport->alive())return 5;
        net::tcp_close(socket);
        if(socket.valid()||weak.expired())return 8; // close changes state, not ownership.
        socket={};
        if(weak.expired())return 9; // The explicit transport owner remains.
        transport.reset(); // Last owner joins before member storage disappears.
        if(!weak.expired())return 6;
    }
    std::cout<<"current WSS repeated close with pending read/write released owners\n";
}
