// Exercises the same native WSS transport as Session, without rendering or audio.
#include "../net/wss_client.h"
#include "../net/session.h"
#include "../meta/http_client.h"
#include "../meta/account_client.h"
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <thread>

namespace {
constexpr auto kInitialResponseDeadline = std::chrono::seconds(5);
constexpr size_t kInitialResponseLimit = 64 * 1024;

int read_initial_response(net::TcpSocket& socket) {
    const auto end = std::chrono::steady_clock::now() + kInitialResponseDeadline;
    std::vector<uint8_t> received;
    std::vector<uint8_t> pending;

    while (std::chrono::steady_clock::now() < end) {
        std::vector<uint8_t> input;
        if (!net::tcp_recv_some(socket, input)) {
            if (!received.empty())
                std::cout.write(reinterpret_cast<const char*>(received.data()), received.size());
            return received.empty() ? 6 : 8;
        }
        if (input.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        if (received.size() + input.size() > kInitialResponseLimit) {
            std::cerr << "initial WSS response exceeded 64 KiB\n";
            std::cout.write(reinterpret_cast<const char*>(received.data()), received.size());
            std::cout.write(reinterpret_cast<const char*>(input.data()), input.size());
            return 8;
        }
        received.insert(received.end(), input.begin(), input.end());
        pending.insert(pending.end(), input.begin(), input.end());

        std::vector<net::Frame> frames;
        if (!net::parse_frames(pending, frames)) {
            std::cerr << "invalid initial game frame from WSS peer\n";
            std::cout.write(reinterpret_cast<const char*>(received.data()), received.size());
            return 8;
        }
        for (const auto& frame : frames) {
            if (frame.type == net::MsgType::ROOM_INFO ||
                frame.type == net::MsgType::SERVER_REJECT) {
                std::cout.write(reinterpret_cast<const char*>(received.data()), received.size());
                if (frame.type == net::MsgType::SERVER_REJECT)
                    std::cerr << "server sent SERVER_REJECT\n";
                return 0;
            }
        }
    }

    std::cerr << "timed out waiting for ROOM_INFO or SERVER_REJECT\n";
    std::cout.write(reinterpret_cast<const char*>(received.data()), received.size());
    return 7;
}
}

int main(int argc,char** argv) {
    if(argc==4 && std::string(argv[1])=="--account") {
        meta::client::MetaClient api(argv[3]);
        const std::string action = argv[2];
        if (action == "paths") {
            meta::client::AccountStore store(api.baseUrl());
            std::cout << store.token_path() << "\n";
            return 0;
        }
        auto result = (action == "bootstrap" || action == "bootstrap-retry") ? meta::client::bootstrap_account(api)
            : action == "import" ? meta::client::import_legacy_account(api)
            : action == "create" ? meta::client::bootstrap_account(api, true)
            : action == "resume" ? meta::client::resume_account_change(api)
            : meta::client::change_saved_account(api, action);
        if (action == "bootstrap-retry" && result.unsaved) {
            std::cout << "save-required\n" << std::flush;
            std::cin.get(); // Test harness repairs the local destination before retrying.
            result = meta::client::save_created_account(api, result.token);
        }
        std::cout<<result.message<<"\n"; // Never print bearer secrets, including on failure.
        return result.ok ? 0 : result.unsaved ? 10 : result.pending ? 8 : 9;
    }
    if(argc==5 && std::string(argv[1])=="--session") {
        // Same issuer callback, worker and shutdown path as the graphical client.
        meta::client::MetaClient meta(argv[3]);
        if(!meta.valid())return 4;
        std::ifstream file(argv[4]);std::string account;file>>account;
        net::Session session;
        session.SetTicketIssuer([&](const std::string& token){return meta.request_game_ticket(token);});
        if(!session.RoomCreate(argv[2],443,120,2,account))return 5;
        auto end=std::chrono::steady_clock::now()+std::chrono::seconds(8);
        while(std::chrono::steady_clock::now()<end) {
            if(session.hasFailed())return 6;
            if(session.roomState()==net::RoomState::Waiting) {
                session.RoomLeave();session.Close();return 0;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return 7;
    }
    if(argc!=3)return 2;
    std::ifstream file(argv[2],std::ios::binary);
    std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(file)),{});
    if(bytes.empty())return 3;
    auto socket=net::wss_connect(argv[1]);
    if(!socket.valid())return 4;
    if(!net::tcp_send_all(socket,bytes.data(),bytes.size()))return 5;
    const int result = read_initial_response(socket);
    net::tcp_close(socket);
    return result;
}
