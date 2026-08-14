// server/log.h — 릴레이 로그: 한 줄을 조립한 뒤 한 번의 write 로 내보낸다.
//
// 왜 필요했나
//   기존 로그는 전부 `std::cerr << a << b << c` 형태였다. 이 표현은 원자적이지
//   않다 — 삽입 연산자 하나하나가 별도의 출력 연산이라, 다른 스레드가 그 사이에
//   끼어들면 두 매치의 로그가 한 줄에 엉킨다. 스레드 모델은 연결마다 스레드를
//   두므로 상시로, 루프 릴레이도 샤드 스레드와 오프로드 워커가 있어 부하가
//   오르면 실제로 섞였다. 섞인 로그는 "그 시각 그 사람이 왜 끊겼는지" 를 못
//   맞추므로 문의 대응에 쓸 수 없다.
//
//   여기서는 줄 전체를 std::string 에 조립한 뒤 write(2) 를 정확히 한 번 부른다.
//   파이프로 받을 때 PIPE_BUF(4096) 이하의 write 는 커널이 쪼개지 않으므로,
//   이 로그의 줄 길이(수십~수백 바이트)에서는 인터리브가 구조적으로 불가능하다.
//
// CPU 예산
//   배포 대상은 저전력 쿼드코어다. 로깅이 트래픽 처리와 경쟁하면 안 되므로 두
//   가지를 지킨다.
//     1) 포워딩 hot path 에는 호출 자체를 두지 않는다. 접속·매치 시작/종료 같은
//        연결 수명당 몇 번뿐인 이벤트만 찍는다.
//     2) 임계값 아래의 호출은 인자를 아예 만들지 않는다. 매크로가 먼저 레벨을
//        검사하므로, 꺼진 레벨의 로그는 원자적 load 한 번이 전부다.
//   iostream 을 쓰지 않는 이유도 같다 — ostringstream 은 호출마다 로케일과
//   스트림 상태를 끌고 오는 반면, 여기서는 std::to_chars 로 정수를 바로 찍는다.
//
// 레벨
//   Error < Warn < Info < Debug. 기본은 Info — 접속 하나하나가 아니라 거절·종료·
//   매치 수명·주기 상태만 남는 수준이다. 운영에서 조용히 돌리려면 --log-level
//   warn, 재현 조사에는 debug 를 쓴다.

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>

namespace relay {

enum class LogLevel : int { Error = 0, Warn = 1, Info = 2, Debug = 3 };

// 임계값. 기동 시 한 번 정하고 그 뒤로는 읽기만 하지만, 여러 루프 스레드가
// 동시에 읽으므로 원자적으로 둔다.
extern std::atomic<int> g_log_threshold;

void        set_log_level(LogLevel lv);
bool        parse_log_level(const std::string& s, LogLevel& out);
const char* log_level_name(LogLevel lv);

inline bool log_enabled(LogLevel lv)
{
    return static_cast<int>(lv) <= g_log_threshold.load(std::memory_order_relaxed);
}

// 한 줄 버퍼. 조립이 끝나면 log_emit 이 타임스탬프와 레벨을 앞에 붙여 한 번에
// 내보낸다. 인스턴스는 항상 스택에 있고 스레드를 넘지 않는다.
class LogLine {
public:
    explicit LogLine(LogLevel lv) : lv_(lv) { buf_.reserve(160); }

    LogLine& operator<<(const char* s);
    LogLine& operator<<(const std::string& s);
    LogLine& operator<<(char c);
    LogLine& operator<<(bool b);
    LogLine& operator<<(int v);
    LogLine& operator<<(unsigned v);
    LogLine& operator<<(long v);
    LogLine& operator<<(unsigned long v);
    LogLine& operator<<(long long v);
    LogLine& operator<<(unsigned long long v);

    LogLevel           level() const { return lv_; }
    const std::string& text()  const { return buf_; }

private:
    LogLevel    lv_;
    std::string buf_;
};

// 줄 하나를 완성해 단일 write 로 내보낸다.
void log_emit(const LogLine& line);

// "0x" 접두의 16진 표기. seed 처럼 비트 패턴 자체가 의미인 값에 쓴다 — 클라이언트
// 트레이스도 같은 표기라 두 로그를 눈으로 맞춰 볼 수 있다. iostream 의 std::hex
// 는 스트림 상태를 바꾸는 조작자라 여기 문법에 맞지 않아 함수로 둔다.
std::string log_hex(unsigned long long v);

// 임계값 아래면 인자를 평가조차 하지 않는다. do/while 로 감싸 if 문 뒤에서도
// 안전하게 쓰인다.
#define RELAY_LOG_AT(level, expr)                                              \
    do {                                                                       \
        if (::relay::log_enabled(level)) {                                     \
            ::relay::LogLine _relay_ll{level};                                 \
            _relay_ll << expr;                                                 \
            ::relay::log_emit(_relay_ll);                                      \
        }                                                                      \
    } while (0)

#define RLOG_ERROR(expr) RELAY_LOG_AT(::relay::LogLevel::Error, expr)
#define RLOG_WARN(expr)  RELAY_LOG_AT(::relay::LogLevel::Warn,  expr)
#define RLOG_INFO(expr)  RELAY_LOG_AT(::relay::LogLevel::Info,  expr)
#define RLOG_DEBUG(expr) RELAY_LOG_AT(::relay::LogLevel::Debug, expr)

}  // namespace relay
