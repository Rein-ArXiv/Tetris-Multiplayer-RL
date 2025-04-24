#ifndef CLIENT_H
#define CLIENT_H

#include <string>
#include <vector>
#include <map>
#include <thread>
#include <mutex>
#include <atomic>
#include <asio.hpp>
#include <ncurses.h>
#include "game.h"
#include "network_protocol.h"

using asio::ip::tcp;

class TetrisClient {
public:
    TetrisClient(const std::string& serverAddress, int port, const std::string& playerName);
    ~TetrisClient();
    
    // 클라이언트 제어
    bool Connect();
    void Disconnect();
    void Run();
    bool IsConnected() const { return connected; }
    
    // 네트워크 통신
    void SendInputAction(InputAction action);
    
    // 게임 상태 관리
    void UpdateLocalGame(const GameStatePacket& packet);
    void UpdateOpponentGame(int playerId, const GameStatePacket& packet);
    void ProcessGarbageLines(int lineCount);
    void UpdatePlayerList(const PlayerListPacket& packet);
    
private:
    // 연결 정보
    std::string serverAddress;
    int port;
    std::string playerName;
    asio::io_context io_context;
    tcp::socket socket;
    std::atomic<bool> connected;
    int clientId;
    
    // 스레드 및 동기화
    std::thread networkThread;
    std::mutex mutex;
    
    // 게임 상태
    Game localGame;
    std::map<int, Game> opponentGames;
    std::map<int, std::string> playerNames;
    
    // 네트워크 버퍼
    std::string readBuffer;
    std::vector<std::string> writeQueue;
    
    // ncurses 윈도우
    WINDOW* mainWin;
    WINDOW* localGameWin;
    std::map<int, WINDOW*> opponentWins;
    WINDOW* infoWin;
    
    // 통신 함수
    void NetworkLoop();
    void ReadFromServer();
    void WriteToServer();
    void ProcessPacket(const Packet& packet);
    
    // UI 함수
    void InitializeUI();
    void UpdateUI();
    void CleanupUI();
    void HandleInput();
};

#endif // CLIENT_H