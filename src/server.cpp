#include "server.h"
#include <iostream>
#include <chrono>
#include <algorithm>

TetrisServer::TetrisServer(int port)
    : isRunning(false), port(port), acceptor(io_context, tcp::endpoint(tcp::v4(), port)),
      nextClientId(1), updateInterval(0.05) { // 50ms 업데이트 간격
}

TetrisServer::~TetrisServer() {
    Stop();
}

void TetrisServer::Start() {
    if (isRunning) return;
    
    isRunning = true;
    
    // 연결 수락 시작
    AcceptConnections();
    
    // 서비스 스레드 시작
    serviceThread = std::thread([this]() {
        while (isRunning) {
            io_context.run_one();
        }
    });
    
    // 게임 업데이트 스레드 시작
    gameUpdateThread = std::thread([this]() {
        UpdateGames();
    });
    
    std::cout << "서버가 포트 " << port << "에서 시작되었습니다." << std::endl;
}

void TetrisServer::Stop() {
    if (!isRunning) return;
    
    isRunning = false;
    
    // 모든 연결 종료
    {
        std::lock_guard<std::mutex> lock(mutex);
        
        for (auto& client : clients) {
            if (client.second->socket.is_open()) {
                asio::error_code ec;
                client.second->socket.close(ec);
            }
        }
        
        clients.clear();
        players.clear();
    }
    
    // io_context 중지
    io_context.stop();
    
    // 스레드 종료 대기
    if (serviceThread.joinable()) {
        serviceThread.join();
    }
    
    if (gameUpdateThread.joinable()) {
        gameUpdateThread.join();
    }
    
    std::cout << "서버가 종료되었습니다." << std::endl;
}

void TetrisServer::AcceptConnections() {
    auto connection = std::make_shared<ClientConnection>(io_context);
    
    acceptor.async_accept(connection->socket, [this, connection](const asio::error_code& error) {
        HandleAccept(connection, error);
    });
}

void TetrisServer::HandleAccept(std::shared_ptr<ClientConnection> connection, const asio::error_code& error) {
    if (!error) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            
            // 새 클라이언트에 ID 할당
            int clientId = nextClientId++;
            connection->player = std::make_shared<Player>(clientId, "Player " + std::to_string(clientId));
            clients[clientId] = connection;
            players[clientId] = connection->player;
            
            std::cout << "새 클라이언트 연결: ID " << clientId << std::endl;
            
            // 사용 가능한 AI 모델 목록 전송
            std::vector<std::string> models = GetAvailableAIModels();
            std::string modelsStr;
            for (const auto& model : models) {
                if (!modelsStr.empty()) modelsStr += ",";
                modelsStr += model;
            }
            
            Packet aiModelsPacket;
            aiModelsPacket.type = PacketType::AI_MODELS;
            aiModelsPacket.clientId = 0; // 서버 메시지
            aiModelsPacket.data = modelsStr;
            
            connection->writeQueue.push_back(SerializePacket(aiModelsPacket));
            WriteToClient(connection);
            
            // 현재 게임 모드 전송
            Packet gameModePacket;
            gameModePacket.type = PacketType::GAME_MODE_CHANGED;
            gameModePacket.clientId = 0; // 서버 메시지
            gameModePacket.data = GameModeToString(gameMode);
            
            connection->writeQueue.push_back(SerializePacket(gameModePacket));
            WriteToClient(connection);
        }
        
        // 플레이어 목록 브로드캐스트
        BroadcastPlayerList();
        
        // 클라이언트로부터 읽기 시작
        ReadFromClient(connection);
    }
    
    // 다음 연결 수락 준비
    AcceptConnections();
}

void TetrisServer::ReadFromClient(std::shared_ptr<ClientConnection> connection) {
    auto buffer = std::make_shared<std::vector<char>>(1024);
    
    connection->socket.async_read_some(
        asio::buffer(*buffer),
        [this, connection, buffer](const asio::error_code& error, std::size_t bytesRead) {
            if (!error) {
                // 읽은 데이터를 버퍼에 추가
                connection->readBuffer.append(buffer->data(), bytesRead);
                
                // 완전한 패킷이 있는지 확인
                size_t pos;
                while ((pos = connection->readBuffer.find('\n')) != std::string::npos) {
                    std::string packetStr = connection->readBuffer.substr(0, pos);
                    connection->readBuffer.erase(0, pos + 1);
                    
                    // 패킷 처리
                    try {
                        Packet packet = DeserializePacket(packetStr);
                        packet.clientId = connection->player->GetId(); // 클라이언트 ID 설정
                        ProcessPacket(packet.clientId, packet);
                    } catch (const std::exception& e) {
                        std::cerr << "패킷 처리 오류: " << e.what() << std::endl;
                    }
                }
                
                // 계속 읽기
                ReadFromClient(connection);
            } else {
                // 연결 종료
                HandleClientDisconnect(connection->player->GetId());
            }
        }
    );
}

void TetrisServer::WriteToClient(std::shared_ptr<ClientConnection> connection) {
    if (connection->writeQueue.empty()) return;
    
    std::string data = connection->writeQueue.front() + "\n";
    
    asio::async_write(
        connection->socket,
        asio::buffer(data),
        [this, connection](const asio::error_code& error, std::size_t /*bytesWritten*/) {
            if (!error) {
                std::lock_guard<std::mutex> lock(mutex);
                if (!connection->writeQueue.empty()) {
                    connection->writeQueue.erase(connection->writeQueue.begin());
                    if (!connection->writeQueue.empty()) {
                        WriteToClient(connection);
                    }
                }
            } else {
                // 연결 종료
                HandleClientDisconnect(connection->player->GetId());
            }
        }
    );
}

void TetrisServer::HandleClientDisconnect(int clientId) {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (clients.find(clientId) != clients.end()) {
        std::cout << "클라이언트 연결 종료: ID " << clientId << std::endl;
        
        // 소켓 닫기
        if (clients[clientId]->socket.is_open()) {
            asio::error_code ec;
            clients[clientId]->socket.close(ec);
        }
        
        // 클라이언트와 플레이어 제거
        clients.erase(clientId);
        players.erase(clientId);
        
        // 플레이어 목록 업데이트
        BroadcastPlayerList();
    }
}

void TetrisServer::ProcessPacket(int clientId, const Packet& packet) {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (players.find(clientId) == players.end() && packet.type != PacketType::CONNECT) {
        return;
    }
    switch (packet.type) {
        case PacketType::CONNECT: {
            // 연결 패킷 처리
            ConnectPacket connectPacket = DeserializeConnectPacket(packet.data);
            players[clientId]->GetName() = connectPacket.playerName;
            BroadcastPlayerList();
            break;
        }
        
        case PacketType::DISCONNECT: {
            // 연결 종료 패킷 처리
            HandleClientDisconnect(clientId);
            break;
        }
        
        case PacketType::INPUT: {
            // 입력 패킷 처리
            InputPacket inputPacket = DeserializeInputPacket(packet.data);
            players[clientId]->SetLastInputAction(inputPacket.action);
            players[clientId]->GetGame().ProcessAction(inputPacket.action);
            
            // 각 플레이어의 게임 상태 브로드캐스트
            BroadcastGameState(clientId);
            break;
        }
        
        case PacketType::CHAT_MESSAGE: {
            // 채팅 메시지 브로드캐스트 (모든 클라이언트에게 전달)
            for (const auto& client : clients) {
                Packet chatPacket;
                chatPacket.type = PacketType::CHAT_MESSAGE;
                chatPacket.clientId = clientId;
                chatPacket.data = packet.data;
                
                client.second->writeQueue.push_back(SerializePacket(chatPacket));
                WriteToClient(client.second);
            }
            break;
        }
        
        case PacketType::ADD_AI: {
            // AI 추가 요청
            if (gameMode == GameMode::SINGLE_PLAYER || gameMode == GameMode::MULTIPLAYER) {
                // 단일 플레이어나 일반 멀티플레이어 모드에서는 AI 추가 불가
                break;
            }
            
            AIModelType modelType = StringToAIModelType(packet.data);
            AddAIPlayer(modelType);
            break;
        }
        
        case PacketType::REMOVE_AI: {
            // AI 제거 요청
            int aiId = std::stoi(packet.data);
            RemoveAIPlayer(aiId);
            break;
        }
        
        case PacketType::CHANGE_GAME_MODE: {
            // 게임 모드 변경 요청
            GameMode newMode = StringToGameMode(packet.data);
            ConfigureGameMode(newMode);
            
            // 모든 클라이언트에게 모드 변경 알림
            Packet notifyPacket;
            notifyPacket.type = PacketType::GAME_MODE_CHANGED;
            notifyPacket.clientId = 0; // 서버 메시지
            notifyPacket.data = GameModeToString(newMode);
            
            for (const auto& client : clients) {
                client.second->writeQueue.push_back(SerializePacket(notifyPacket));
                WriteToClient(client.second);
            }
            break;
        }
        default:
            break;
    }
}

void TetrisServer::BroadcastGameState(int sourcePlayerId) {
    if (players.find(sourcePlayerId) == players.end()) return;
    
    // 현재 플레이어의 게임 상태 생성
    GameStatePacket statePacket;
    const Game& game = players[sourcePlayerId]->GetGame();
    statePacket.board = game.GetBoardState();
    statePacket.currentBlockType = static_cast<int>(game.GetCurrentBlock().type);
    statePacket.nextBlockType = static_cast<int>(game.GetNextBlock().type);
    statePacket.score = game.GetScore();
    statePacket.level = game.GetLevel();
    statePacket.linesCleared = game.GetLinesCleared();
    statePacket.isGameOver = game.IsGameOver();
    
    // 모든 클라이언트에게 전송
    for (const auto& client : clients) {
        Packet packet;
        packet.type = PacketType::GAME_STATE;
        packet.clientId = sourcePlayerId;
        packet.data = SerializeGameStatePacket(statePacket);
        
        client.second->writeQueue.push_back(SerializePacket(packet));
        WriteToClient(client.second);
    }
    
    // 라인 클리어 시 쓰레기 라인 처리
    int lastClearedLines = game.GetLastClearedLines();
    if (lastClearedLines > 1) {
        ProcessClearedLines(players[sourcePlayerId], lastClearedLines);
    }
}

void TetrisServer::BroadcastPlayerList() {
    PlayerListPacket listPacket;
    
    for (const auto& player : players) {
        listPacket.playerIds.push_back(player.first);
        listPacket.playerNames.push_back(player.second->GetName());
    }
    
    // 모든 클라이언트에게 전송
    for (const auto& client : clients) {
        Packet packet;
        packet.type = PacketType::PLAYER_LIST;
        packet.clientId = 0; // 서버 메시지
        packet.data = SerializePlayerListPacket(listPacket);
        
        client.second->writeQueue.push_back(SerializePacket(packet));
        WriteToClient(client.second);
    }
}

void TetrisServer::SendGarbageLines(int sourcePlayerId, int targetPlayerId, int lineCount) {
    if (players.find(targetPlayerId) == players.end()) return;
    
    // 대상 플레이어에게 쓰레기 라인 추가
    players[targetPlayerId]->GetGame().AddGarbageLines(lineCount);
    
    // 쓰레기 라인 패킷 전송
    for (const auto& client : clients) {
        GarbageLinesPacket garbagePacket;
        garbagePacket.targetPlayerId = targetPlayerId;
        garbagePacket.lineCount = lineCount;
        
        Packet packet;
        packet.type = PacketType::GARBAGE_LINES;
        packet.clientId = sourcePlayerId;
        packet.data = SerializeGarbageLinesPacket(garbagePacket);
        
        client.second->writeQueue.push_back(SerializePacket(packet));
        WriteToClient(client.second);
    }
}

void TetrisServer::UpdateGames() {
    auto lastTime = std::chrono::high_resolution_clock::now();
    
    while (isRunning) {
        auto currentTime = std::chrono::high_resolution_clock::now();
        double deltaTime = std::chrono::duration<double>(currentTime - lastTime).count();
        lastTime = currentTime;
        
        {
            std::lock_guard<std::mutex> lock(mutex);
            
            // 모든 플레이어의 게임 업데이트
            for (auto& player : players) {
                player.second->GetGame().Update(deltaTime);
                
                // 게임 상태가 변경되었으면 브로드캐스트
                if (player.second->GetGame().GetLastClearedLines() > 0) {
                    BroadcastGameState(player.first);
                }
            }
        }
        
        // 업데이트 간격만큼 대기
        std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(updateInterval * 1000)));
    }
}

void TetrisServer::ProcessClearedLines(std::shared_ptr<Player> player, int clearedLines) {
    // 2줄 이상 클리어 시 다른 플레이어들에게 쓰레기 라인 추가
    if (clearedLines <= 1) return;
    
    int garbageLines = clearedLines - 1; // 클리어한 라인 수보다 하나 적게 쓰레기 라인 추가
    int sourcePlayerId = player->GetId();
    
    // 모든 플레이어에게 쓰레기 라인 추가 (자신 제외)
    for (const auto& targetPlayer : players) {
        int targetPlayerId = targetPlayer.first;
        
        if (targetPlayerId != sourcePlayerId) {
            SendGarbageLines(sourcePlayerId, targetPlayerId, garbageLines);
        }
    }
}
