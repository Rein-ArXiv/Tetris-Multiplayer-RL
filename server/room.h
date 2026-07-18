// server/room.h — Custom Room (5자리 코드) 매칭 저장소
//
// 역할:
//   - ROOM_CREATE : 새 방 개설 + 코드 발행 + ROOM_INFO 회신
//   - ROOM_JOIN   : 기존 방 입장 + 양쪽에 ROOM_INFO 푸시
//   - READY       : 양쪽 플래그 추적 + 상대에게 포워딩
//   - ROOM_LEAVE  : 방 정리 + 남은 쪽에 ROOM_INFO(gonefull)
//   - 양쪽 READY 확인되면 matchmaker 와 동일한 relay::startPump 로 이관
//
// 스레드 모델: 각 playerConnThread 가 handleCreate/handleJoin 을 호출하고 그 안에서
//   자신의 소켓을 읽는 roomLoop_ 에 블로킹한다. 매치 시작 시점에는 두 스레드 중
//   "먼저 양쪽 Ready 를 관측한 쪽"이 starter 가 되어 상대 스레드의 exit 를 기다린
//   뒤 startPump 를 호출한다.

#pragma once
#include "../net/socket.h"
#include "matchmaker.h"

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace meta::client { class MetaClient; }

namespace relay {

class RoomRegistry {
public:
    RoomRegistry();

    // 매치 성립 시 startPump 에 넘겨줄 meta 클라이언트.
    // 미설정 시 unranked (MATCH_SUMMARY 투명 포워딩).
    void setMeta(meta::client::MetaClient* meta) { meta_ = meta; }

    // playerConnThread 에서 ROOM_CREATE 수신 직후 호출.
    // 방을 만들고 roomLoop_ 안에서 블로킹하다가 종료 시 리턴.
    // player_id=0 은 unranked (meta 연동 안 됨). 매치 성립 시 startPump 에 전달.
    // streamPrefix: playerConnThread 가 첫 프레임과 같은 recv 로 이미 끌어온
    //   후속 바이트 (완성 프레임 재직렬화분 + partial tail). roomLoop_ 의 수신
    //   버퍼 초기값이 된다 — 버리면 그 프레임들이 유실된다.
    void handleCreate(net::TcpSocket sock, uint32_t conn_id,
                      int64_t player_id, int elo,
                      const std::string& username, const std::string& token,
                      const std::string& selected_icon_id,
                      std::vector<uint8_t> streamPrefix = {});

    // playerConnThread 에서 ROOM_JOIN 수신 직후 호출.
    // 실패(notfound/full) 시 ROOM_INFO 회신 후 소켓 닫고 바로 리턴.
    void handleJoin(const std::string& code, net::TcpSocket sock, uint32_t conn_id,
                    int64_t player_id, int elo,
                    const std::string& username, const std::string& token,
                    const std::string& selected_icon_id,
                    std::vector<uint8_t> streamPrefix = {});

    // 모든 roomLoop_ 를 종료시킨다.
    void shutdown();

private:
    struct Entry {
        std::string    code;
        net::TcpSocket hostSock{};
        net::TcpSocket guestSock{};
        uint32_t       hostConn = 0;
        uint32_t       guestConn = 0;
        bool           hostPresent  = false;
        bool           guestPresent = false;
        bool           hostReady    = false;
        bool           guestReady   = false;
        bool           matchStarted = false;  // 한쪽이 starter 로 선점
        bool           hostExited   = false;  // player thread 가 read 루프를 빠져나옴
        bool           guestExited  = false;
        uint64_t       roomInfoVersion = 0;

        // 인증 메타 (meta 연동 시 채워짐. 0 = unranked)
        int64_t        hostPlayerId  = 0;
        int            hostElo       = 0;
        std::string    hostUsername;
        std::string    hostToken;
        std::string    hostSelectedIconId{"default"};
        int64_t        guestPlayerId = 0;
        int            guestElo      = 0;
        std::string    guestUsername;
        std::string    guestToken;
        std::string    guestSelectedIconId{"default"};
    };

    std::string generateCode_();   // mu 잡은 상태에서 호출
    uint64_t    nextSeed_();       // mu 잡은 상태에서 호출
    uint32_t    nextMatchId_();    // mu 잡은 상태에서 호출

    void roomLoop_(const std::string& code, bool isHost,
                   std::vector<uint8_t> streamPrefix = {});
    void sendRoomInfo_(const net::TcpSocket& sock, const std::string& code,
                       uint8_t status, uint8_t peerCount);
    void sendRoomInfoIfCurrent_(const net::TcpSocket& sock,
                                const std::string& code,
                                uint8_t status, uint8_t peerCount,
                                uint64_t expectedVersion);

    std::mutex              mu;
    std::condition_variable cv;
    std::unordered_map<std::string, Entry> rooms;
    // 방별 ROOM_INFO 순서를 유지하면서 느린 소켓 하나가 모든 방을 막지 않도록
    // 코드 해시로 나눈 송신 게이트를 사용한다.
    static constexpr size_t kRoomInfoShardCount = 64;
    std::array<std::mutex, kRoomInfoShardCount> roomInfoMu_;
    std::atomic<bool>       stopping{false};
    uint64_t                code_rng_state_ = 0;
    uint64_t                seed_state_     = 0;
    uint64_t                next_room_info_version_ = 1;
    uint32_t                next_match_id_  = 100000;  // 매치메이킹과 match_id 충돌 피해
    meta::client::MetaClient* meta_ = nullptr;
};

}  // namespace relay
