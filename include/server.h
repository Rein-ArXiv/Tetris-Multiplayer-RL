#ifndef SERVER_H
#define SERVER_H

#include <vector>
#include <map>
#include <mutex>
#include <thread>
#include <functional>
#include <asio.hpp>
#include "player.h"
#include "network_protocol.h"
#include "ai_player.h"
#include "game_mode.h"

using asio::ip::tcp;

class TetrisServer {
public:
    TetrisServer(int port);
    ~TetrisServer();
    
    // 서버 제어
    void Start();
    void Stop();
    
    // 패킷 처리
    void ProcessPacket(int clientId, const Packet& packet);
    
    // 브로드캐스트 및 타겟팅된 패킷 전송
    void BroadcastGameState(int sourcePlayerId);
    void BroadcastPlayerList();
    void SendGarbageLines(int sourcePlayerId, int targetPlayerId, int lineCount);

    // AI 플레이어 추가 함수
    bool AddAIPlayer(AIModelType modelType);
    
    // AI 플레이어 제거 함수
    bool RemoveAIPlayer(int playerId);
    
    // 게임 모드 설정 함수
    void ConfigureGameMode(GameMode mode);
    
    // 사용 가능한 AI 모델 목록 가져오기
    std::vector<std::string> GetAvailableAIModels() const;
    
private:
    // 클라이언트 연결 구조체
    struct ClientConnection {
        tcp::socket socket;
        std::string readBuffer;
        std::vector<std::string> writeQueue;
        std::shared_ptr<Player> player;
        
        ClientConnection(asio::io_context& io_context)
            : socket(io_context) {}
    };
    
    // 서버 상태
    bool isRunning;
    int port;
    asio::io_context io_context;
    tcp::acceptor acceptor;
    std::thread serviceThread;
    std::mutex mutex;
    
    // 클라이언트 및 플레이어 관리
    std::map<int, std::shared_ptr<ClientConnection>> clients;
    std::map<int, std::shared_ptr<Player>> players;
    int nextClientId;
    
    // 게임 갱신 관련
    std::thread gameUpdateThread;
    double updateInterval;
    
    // 연결 및 통신 함수
    void AcceptConnections();
    void HandleAccept(std::shared_ptr<ClientConnection> connection, const asio::error_code& error);
    void ReadFromClient(std::shared_ptr<ClientConnection> connection);
    void WriteToClient(std::shared_ptr<ClientConnection> connection);
    void HandleClientDisconnect(int clientId);
    
    // 게임 갱신 함수
    void UpdateGames();
    void ProcessClearedLines(std::shared_ptr<Player> player, int clearedLines);

    // 게임 모드
    GameMode gameMode;
    // AI 관련 함수

    std::shared_ptr<Player> CreateAIPlayer(int playerId, AIModelType modelType);
    void StartAllAI();
    void StopAllAI();
    
    // 게임 모드 설정
    void SetGameMode(GameMode mode);
    GameMode GetGameMode() const { return gameMode; }
};

#endif // SERVER_H