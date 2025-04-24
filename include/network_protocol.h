#ifndef NETWORK_PROTOCOL_H
#define NETWORK_PROTOCOL_H

#include <string>
#include <vector>
#include "game.h"
#include "player.h"

// 패킷 타입 정의
enum class PacketType {
    CONNECT,
    DISCONNECT,
    INPUT,
    GAME_STATE,
    PLAYER_LIST,
    GARBAGE_LINES,
    CHAT_MESSAGE
    ADD_AI,             // AI 플레이어 추가 요청
    REMOVE_AI,          // AI 플레이어 제거 요청
    AI_MODELS,          // 사용 가능한 AI 모델 목록
    CHANGE_GAME_MODE,   // 게임 모드 변경 요청
    GAME_MODE_CHANGED,  // 게임 모드 변경 알림
};

// 패킷 기본 구조
struct Packet {
    PacketType type;
    int clientId;
    std::string data;
};

// 연결 패킷
struct ConnectPacket {
    std::string playerName;
};

// 입력 패킷
struct InputPacket {
    InputAction action;
};

// 게임 상태 패킷
struct GameStatePacket {
    std::vector<std::vector<int>> board;
    int currentBlockType;
    int nextBlockType;
    int score;
    int level;
    int linesCleared;
    bool isGameOver;
};

// 플레이어 목록 패킷
struct PlayerListPacket {
    std::vector<int> playerIds;
    std::vector<std::string> playerNames;
};

// 쓰레기 라인 패킷
struct GarbageLinesPacket {
    int targetPlayerId;
    int lineCount;
};

// 직렬화/역직렬화 함수
std::string SerializePacket(const Packet& packet);
Packet DeserializePacket(const std::string& data);

std::string SerializeConnectPacket(const ConnectPacket& packet);
ConnectPacket DeserializeConnectPacket(const std::string& data);

std::string SerializeInputPacket(const InputPacket& packet);
InputPacket DeserializeInputPacket(const std::string& data);

std::string SerializeGameStatePacket(const GameStatePacket& packet);
GameStatePacket DeserializeGameStatePacket(const std::string& data);

std::string SerializePlayerListPacket(const PlayerListPacket& packet);
PlayerListPacket DeserializePlayerListPacket(const std::string& data);

std::string SerializeGarbageLinesPacket(const GarbageLinesPacket& packet);
GarbageLinesPacket DeserializeGarbageLinesPacket(const std::string& data);

#endif // NETWORK_PROTOCOL_H