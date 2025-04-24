#include <iostream>
#include <string>
#include "client.h"

int main(int argc, char* argv[]) {
    // 기본 설정
    std::string serverAddress = "localhost";
    int port = 12345;
    std::string playerName = "Player";
    
    // 명령행 인수 처리
    if (argc > 1) {
        serverAddress = argv[1];
    }
    
    if (argc > 2) {
        port = std::atoi(argv[2]);
    }
    
    if (argc > 3) {
        playerName = argv[3];
    } else {
        // 플레이어 이름 입력 받기
        std::cout << "플레이어 이름 입력: ";
        std::getline(std::cin, playerName);
        
        if (playerName.empty()) {
            playerName = "Player";
        }
    }
    
    std::cout << "테트리스 멀티플레이어 클라이언트" << std::endl;
    std::cout << "서버 주소: " << serverAddress << ":" << port << std::endl;
    std::cout << "플레이어 이름: " << playerName << std::endl;
    
    // 클라이언트 생성 및 연결
    TetrisClient client(serverAddress, port, playerName);
    
    std::cout << "서버에 연결 중..." << std::endl;
    if (!client.Connect()) {
        std::cerr << "서버 연결 실패!" << std::endl;
        return 1;
    }
    
    std::cout << "연결 성공! 게임 시작..." << std::endl;
    
    // 게임 실행
    client.Run();
    
    return 0;
}