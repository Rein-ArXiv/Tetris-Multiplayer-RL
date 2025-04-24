#include <iostream>
#include <csignal>
#include "server.h"

TetrisServer* server = nullptr;

void signalHandler(int signal) {
    std::cout << "신호 수신: " << signal << std::endl;
    if (server) {
        server->Stop();
    }
    exit(0);
}

int main(int argc, char* argv[]) {
    // 기본 포트 설정
    int port = 12345;
    
    // 명령행 인수에서 포트 읽기
    if (argc > 1) {
        port = std::atoi(argv[1]);
    }
    
    // 시그널 핸들러 설정
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    std::cout << "테트리스 서버 시작 (포트: " << port << ")" << std::endl;
    
    server = new TetrisServer(port);
    server->Start();
    
    // 사용자 입력으로 서버 제어
    std::string command;
    while (true) {
        std::cout << "명령 입력 (quit으로 종료): ";
        std::getline(std::cin, command);
        
        if (command == "quit") {
            break;
        }
    }
    
    server->Stop();
    delete server;
    
    return 0;
}