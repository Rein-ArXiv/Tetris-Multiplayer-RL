#include "network_protocol.h"
#include <sstream>
#include <iomanip>

// 패킷 직렬화 함수들

std::string SerializePacket(const Packet& packet) {
    std::stringstream ss;
    ss << static_cast<int>(packet.type) << ":" << packet.clientId << ":" << packet.data;
    return ss.str();
}

Packet DeserializePacket(const std::string& data) {
    Packet packet;
    std::stringstream ss(data);
    std::string token;
    
    // 패킷 타입 파싱
    if (std::getline(ss, token, ':')) {
        packet.type = static_cast<PacketType>(std::stoi(token));
    }
    
    // 클라이언트 ID 파싱
    if (std::getline(ss, token, ':')) {
        packet.clientId = std::stoi(token);
    }
    
    // 나머지 데이터 파싱
    std::getline(ss, packet.data, '\0');
    
    return packet;
}

std::string SerializeConnectPacket(const ConnectPacket& packet) {
    return packet.playerName;
}

ConnectPacket DeserializeConnectPacket(const std::string& data) {
    ConnectPacket packet;
    packet.playerName = data;
    return packet;
}

std::string SerializeInputPacket(const InputPacket& packet) {
    return std::to_string(static_cast<int>(packet.action));
}

InputPacket DeserializeInputPacket(const std::string& data) {
    InputPacket packet;
    packet.action = static_cast<InputAction>(std::stoi(data));
    return packet;
}

std::string SerializeGameStatePacket(const GameStatePacket& packet) {
    std::stringstream ss;
    
    // 보드 상태 직렬화
    for (const auto& row : packet.board) {
        for (int cell : row) {
            ss << cell << ",";
        }
        ss << ";";
    }
    
    // 기타 게임 상태 정보
    ss << packet.currentBlockType << ":"
       << packet.nextBlockType << ":"
       << packet.score << ":"
       << packet.level << ":"
       << packet.linesCleared << ":"
       << (packet.isGameOver ? "1" : "0");
    
    return ss.str();
}

GameStatePacket DeserializeGameStatePacket(const std::string& data) {
    GameStatePacket packet;
    std::stringstream ss(data);
    std::string token, rowData;
    
    // 보드 상태 파싱
    std::string boardData;
    std::getline(ss, boardData, ':');
    
    std::stringstream boardStream(boardData);
    while (std::getline(boardStream, rowData, ';')) {
        if (rowData.empty()) continue;
        
        std::vector<int> row;
        std::stringstream rowStream(rowData);
        std::string cellData;
        
        while (std::getline(rowStream, cellData, ',')) {
            if (cellData.empty()) continue;
            row.push_back(std::stoi(cellData));
        }
        
        if (!row.empty()) {
            packet.board.push_back(row);
        }
    }
    
    // 기타 게임 상태 정보 파싱
    std::getline(ss, token, ':');
    packet.currentBlockType = std::stoi(token);
    
    std::getline(ss, token, ':');
    packet.nextBlockType = std::stoi(token);
    
    std::getline(ss, token, ':');
    packet.score = std::stoi(token);
    
    std::getline(ss, token, ':');
    packet.level = std::stoi(token);
    
    std::getline(ss, token, ':');
    packet.linesCleared = std::stoi(token);
    
    std::getline(ss, token, '\0');
    packet.isGameOver = (token == "1");
    
    return packet;
}

std::string SerializePlayerListPacket(const PlayerListPacket& packet) {
    std::stringstream ss;
    
    // 플레이어 ID 직렬화
    for (int id : packet.playerIds) {
        ss << id << ",";
    }
    ss << ":";
    
    // 플레이어 이름 직렬화
    for (const auto& name : packet.playerNames) {
        ss << name << ",";
    }
    
    return ss.str();
}

PlayerListPacket DeserializePlayerListPacket(const std::string& data) {
    PlayerListPacket packet;
    std::stringstream ss(data);
    std::string token, idList, nameList;
    
    // ID 목록과 이름 목록 분리
    std::getline(ss, idList, ':');
    std::getline(ss, nameList, '\0');
    
    // ID 목록 파싱
    std::stringstream idStream(idList);
    while (std::getline(idStream, token, ',')) {
        if (!token.empty()) {
            packet.playerIds.push_back(std::stoi(token));
        }
    }
    
    // 이름 목록 파싱
    std::stringstream nameStream(nameList);
    while (std::getline(nameStream, token, ',')) {
        if (!token.empty()) {
            packet.playerNames.push_back(token);
        }
    }
    
    return packet;
}

std::string SerializeGarbageLinesPacket(const GarbageLinesPacket& packet) {
    return std::to_string(packet.targetPlayerId) + ":" + std::to_string(packet.lineCount);
}

GarbageLinesPacket DeserializeGarbageLinesPacket(const std::string& data) {
    GarbageLinesPacket packet;
    std::stringstream ss(data);
    std::string token;
    
    std::getline(ss, token, ':');
    packet.targetPlayerId = std::stoi(token);
    
    std::getline(ss, token, '\0');
    packet.lineCount = std::stoi(token);
    
    return packet;
}