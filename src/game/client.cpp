#include "client.h"
#include <iostream>
#include <chrono>
#include <thread>

TetrisClient::TetrisClient(const std::string& serverAddress, int port, const std::string& playerName)
    : serverAddress(serverAddress), port(port), playerName(playerName), socket(io_context),
      connected(false), clientId(-1), mainWin(nullptr), localGameWin(nullptr), infoWin(nullptr) {
}

TetrisClient::~TetrisClient() {
    Disconnect();
    CleanupUI();
}

bool TetrisClient::Connect() {
    try {
        // 서버에 연결
        tcp::resolver resolver(io_context);
        auto endpoints = resolver.resolve(serverAddress, std::to_string(port));
        asio::connect(socket, endpoints);
        
        // 연결 패킷 전송
        ConnectPacket connectPacket;
        connectPacket.playerName = playerName;
        
        Packet packet;
        packet.type = PacketType::CONNECT;
        packet.clientId = 0; // 아직 ID 할당 안됨
        packet.data = SerializeConnectPacket(connectPacket);
        
        writeQueue.push_back(SerializePacket(packet));
        asio::write(socket, asio::buffer(writeQueue.front() + "\n"));
        writeQueue.pop_back();
        
        connected = true;
        
        // 네트워크 스레드 시작
        networkThread = std::thread([this]() {
            NetworkLoop();
        });
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "연결 오류: " << e.what() << std::endl;
        return false;
    }
}

void TetrisClient::Disconnect() {
    if (!connected) return;
    
    connected = false;
    
    // 연결 종료 패킷 전송
    try {
        Packet packet;
        packet.type = PacketType::DISCONNECT;
        packet.clientId = clientId;
        packet.data = "";
        
        asio::write(socket, asio::buffer(SerializePacket(packet) + "\n"));
    } catch (...) {
        // 이미 연결이 끊어졌을 수 있음
    }
    
    // 소켓 닫기
    if (socket.is_open()) {
        asio::error_code ec;
        socket.close(ec);
    }
    
    // 네트워크 스레드 종료 대기
    if (networkThread.joinable()) {
        networkThread.join();
    }
}

void TetrisClient::Run() {
    // UI 초기화
    InitializeUI();
    
    // 게임 루프
    bool exit = false;
    auto lastTime = std::chrono::high_resolution_clock::now();
    
    while (!exit && connected) {
        // 경과 시간 계산
        auto currentTime = std::chrono::high_resolution_clock::now();
        double deltaTime = std::chrono::duration<double>(currentTime - lastTime).count();
        lastTime = currentTime;
        
        // 입력 처리
        HandleInput();
        
        // UI 업데이트
        UpdateUI();
        
        // 짧은 대기
        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60fps
    }
}

void TetrisClient::NetworkLoop() {
    try {
        while (connected) {
            ReadFromServer();
            WriteToServer();
        }
    } catch (const std::exception& e) {
        std::cerr << "네트워크 오류: " << e.what() << std::endl;
        connected = false;
    }
}

void TetrisClient::ReadFromServer() {
    char data[1024];
    asio::error_code ec;
    size_t length = socket.read_some(asio::buffer(data), ec);
    
    if (ec) {
        throw asio::system_error(ec);
    }
    
    readBuffer.append(data, length);
    
    // 완전한 패킷이 있는지 확인
    size_t pos;
    while ((pos = readBuffer.find('\n')) != std::string::npos) {
        std::string packetStr = readBuffer.substr(0, pos);
        readBuffer.erase(0, pos + 1);
        
        // 패킷 처리
        try {
            Packet packet = DeserializePacket(packetStr);
            ProcessPacket(packet);
        } catch (const std::exception& e) {
            std::cerr << "패킷 처리 오류: " << e.what() << std::endl;
        }
    }
}

void TetrisClient::WriteToServer() {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (writeQueue.empty()) return;
    
    try {
        asio::write(socket, asio::buffer(writeQueue.front() + "\n"));
        writeQueue.erase(writeQueue.begin());
    } catch (const std::exception& e) {
        std::cerr << "데이터 전송 오류: " << e.what() << std::endl;
        connected = false;
    }
}

void TetrisClient::ProcessPacket(const Packet& packet) {
    std::lock_guard<std::mutex> lock(mutex);
    
    switch (packet.type) {
        case PacketType::GAME_STATE: {
            // 게임 상태 패킷 처리
            GameStatePacket statePacket = DeserializeGameStatePacket(packet.data);
            
            if (packet.clientId == clientId) {
                // 자신의 게임 상태 업데이트
                UpdateLocalGame(statePacket);
            } else {
                // 다른 플레이어의 게임 상태 업데이트
                UpdateOpponentGame(packet.clientId, statePacket);
            }
            break;
        }
        
        case PacketType::PLAYER_LIST: {
            // 플레이어 목록 패킷 처리
            PlayerListPacket listPacket = DeserializePlayerListPacket(packet.data);
            UpdatePlayerList(listPacket);
            break;
        }
        
        case PacketType::GARBAGE_LINES: {
            // 쓰레기 라인 패킷 처리
            GarbageLinesPacket garbagePacket = DeserializeGarbageLinesPacket(packet.data);
            
            if (garbagePacket.targetPlayerId == clientId) {
                ProcessGarbageLines(garbagePacket.lineCount);
            }
            break;
        }
        
        default:
            break;
    }
}

void TetrisClient::SendInputAction(InputAction action) {
    std::lock_guard<std::mutex> lock(mutex);
    
    InputPacket inputPacket;
    inputPacket.action = action;
    
    Packet packet;
    packet.type = PacketType::INPUT;
    packet.clientId = clientId;
    packet.data = SerializeInputPacket(inputPacket);
    
    writeQueue.push_back(SerializePacket(packet));
}

void TetrisClient::UpdateLocalGame(const GameStatePacket& packet) {
    // 서버로부터 받은 게임 상태로 로컬 게임 업데이트
    // (실제 구현에서는 더 복잡한 동기화 로직이 필요할 수 있음)
}

void TetrisClient::UpdateOpponentGame(int playerId, const GameStatePacket& packet) {
    // 다른 플레이어의 게임 상태 업데이트
    if (opponentGames.find(playerId) == opponentGames.end()) {
        opponentGames[playerId] = Game();
    }
    
    // 게임 상태 업데이트 로직
}

void TetrisClient::ProcessGarbageLines(int lineCount) {
    // 로컬 게임에 쓰레기 라인 추가
    localGame.AddGarbageLines(lineCount);
}

void TetrisClient::UpdatePlayerList(const PlayerListPacket& packet) {
    // 플레이어 목록 업데이트
    playerNames.clear();
    
    for (size_t i = 0; i < packet.playerIds.size(); ++i) {
        int id = packet.playerIds[i];
        std::string name = packet.playerNames[i];
        
        playerNames[id] = name;
        
        // 자신의 ID 확인
        if (name == playerName) {
            clientId = id;
        }
    }
}

void TetrisClient::InitializeUI() {
    // ncurses 초기화
    initscr();
    start_color();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    timeout(0);  // 논블로킹 getch()
    curs_set(0); // 커서 숨기기
    
    // 색상 쌍 설정
    init_pair(1, COLOR_CYAN, COLOR_BLACK);    // I 블록
    init_pair(2, COLOR_BLUE, COLOR_BLACK);    // J 블록
    init_pair(3, COLOR_WHITE, COLOR_BLACK);   // L 블록
    init_pair(4, COLOR_YELLOW, COLOR_BLACK);  // O 블록
    init_pair(5, COLOR_GREEN, COLOR_BLACK);   // S 블록
    init_pair(6, COLOR_MAGENTA, COLOR_BLACK); // T 블록
    init_pair(7, COLOR_RED, COLOR_BLACK);     // Z 블록
    init_pair(8, COLOR_WHITE, COLOR_BLACK);   // 고스트 블록
    init_pair(9, COLOR_BLACK, COLOR_WHITE);   // 쓰레기 라인
    
    // 윈도우 생성
    int screenHeight, screenWidth;
    getmaxyx(stdscr, screenHeight, screenWidth);
    
    // 메인 윈도우 (전체 화면)
    mainWin = newwin(screenHeight, screenWidth, 0, 0);
    box(mainWin, 0, 0);
    
    // 로컬 게임 윈도우
    int gameWidth = 12;  // 게임 필드 폭 + 테두리
    int gameHeight = 22; // 게임 필드 높이 + 테두리
    localGameWin = newwin(gameHeight, gameWidth, 1, 1);
    
    // 정보 윈도우
    infoWin = newwin(5, screenWidth - 2, screenHeight - 6, 1);
    
    // 창 새로고침
    refresh();
    wrefresh(mainWin);
    wrefresh(localGameWin);
    wrefresh(infoWin);
}

void TetrisClient::UpdateUI() {
    // 화면 지우기
    wclear(mainWin);
    wclear(localGameWin);
    wclear(infoWin);
    
    // 윈도우 테두리 그리기
    box(mainWin, 0, 0);
    box(localGameWin, 0, 0);
    box(infoWin, 0, 0);
    
    // 로컬 게임 그리기
    localGame.Draw(localGameWin);
    
    // 상대방 게임 그리기
    int opponentX = 15;
    for (auto& opponent : opponentGames) {
        int playerId = opponent.first;
        Game& game = opponent.second;
        
        // 이미 윈도우가 없으면 생성
        if (opponentWins.find(playerId) == opponentWins.end()) {
            opponentWins[playerId] = newwin(12, 10, 1, opponentX);
            opponentX += 12;
        }
        
        // 상대방 게임 그리기
        wclear(opponentWins[playerId]);
        box(opponentWins[playerId], 0, 0);
        
        // 게임 상태 미니 표시
        // (실제 구현에서는 더 작은 크기로 표시)
        
        // 플레이어 이름 표시
        if (playerNames.find(playerId) != playerNames.end()) {
            mvwprintw(opponentWins[playerId], 0, 1, "%s", playerNames[playerId].c_str());
        }
        
        wrefresh(opponentWins[playerId]);
    }
    
    // 정보 윈도우에 플레이어 목록과 점수 표시
    mvwprintw(infoWin, 1, 2, "Player: %s (ID: %d)", playerName.c_str(), clientId);
    mvwprintw(infoWin, 2, 2, "Score: %d  Level: %d", localGame.GetScore(), localGame.GetLevel());
    
    int playerListX = 30;
    mvwprintw(infoWin, 1, playerListX, "Players:");
    for (const auto& player : playerNames) {
        mvwprintw(infoWin, 2, playerListX, "%d: %s", player.first, player.second.c_str());
        playerListX += player.second.length() + 5;
    }
    
    // 윈도우 새로고침
    wrefresh(mainWin);
    wrefresh(localGameWin);
    wrefresh(infoWin);
}

void TetrisClient::CleanupUI() {
    // 윈도우 정리
    if (localGameWin) delwin(localGameWin);
    
    for (auto& win : opponentWins) {
        delwin(win.second);
    }
    
    if (infoWin) delwin(infoWin);
    if (mainWin) delwin(mainWin);
    
    // ncurses 종료
    endwin();
}

void TetrisClient::HandleInput() {
    int key = getch();
    
    if (key != ERR) {
        InputAction action = localGame.HandleInput(key);
        
        if (action != InputAction::NONE) {
            SendInputAction(action);
        }
    }
}