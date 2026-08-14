#include "log.h"

#include <charconv>
#include <chrono>
#include <cstdio>
#include <ctime>

#if defined(_WIN32)
#  include <io.h>
#else
#  include <unistd.h>
#endif

namespace relay {

std::atomic<int> g_log_threshold{static_cast<int>(LogLevel::Info)};

void set_log_level(LogLevel lv)
{
    g_log_threshold.store(static_cast<int>(lv), std::memory_order_relaxed);
}

const char* log_level_name(LogLevel lv)
{
    switch (lv) {
        case LogLevel::Error: return "error";
        case LogLevel::Warn:  return "warn";
        case LogLevel::Info:  return "info";
        case LogLevel::Debug: return "debug";
    }
    return "info";
}

bool parse_log_level(const std::string& s, LogLevel& out)
{
    if (s == "error" || s == "err")   { out = LogLevel::Error; return true; }
    if (s == "warn"  || s == "warning") { out = LogLevel::Warn; return true; }
    if (s == "info")                  { out = LogLevel::Info;  return true; }
    if (s == "debug")                 { out = LogLevel::Debug; return true; }
    return false;
}

namespace {

// 정수는 to_chars 로 찍는다 — 로케일도, 스트림 상태도 거치지 않는다.
template <typename T>
void append_int(std::string& buf, T v)
{
    char        tmp[24];
    const auto  r = std::to_chars(tmp, tmp + sizeof(tmp), v);
    if (r.ec == std::errc{}) buf.append(tmp, static_cast<size_t>(r.ptr - tmp));
}

void append_2(std::string& buf, int v)
{
    buf.push_back(static_cast<char>('0' + (v / 10) % 10));
    buf.push_back(static_cast<char>('0' + v % 10));
}

// UTC 타임스탬프. 문의 대응은 "그 시각" 에서 출발하므로 초만으로는 부족해
// 밀리초까지 남긴다. UTC 로 고정하는 이유는 메타 서버의 경기 기록이 UTC 라
// 두 로그를 같은 축에 놓고 맞춰 볼 수 있어야 하기 때문이다.
void append_timestamp(std::string& buf)
{
    using namespace std::chrono;
    const auto now  = system_clock::now();
    const auto secs = time_point_cast<seconds>(now);
    const auto ms   = duration_cast<milliseconds>(now - secs).count();
    const std::time_t t = system_clock::to_time_t(secs);

    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    append_int(buf, tm.tm_year + 1900);
    buf.push_back('-');
    append_2(buf, tm.tm_mon + 1);
    buf.push_back('-');
    append_2(buf, tm.tm_mday);
    buf.push_back('T');
    append_2(buf, tm.tm_hour);
    buf.push_back(':');
    append_2(buf, tm.tm_min);
    buf.push_back(':');
    append_2(buf, tm.tm_sec);
    buf.push_back('.');
    const int msi = static_cast<int>(ms);
    buf.push_back(static_cast<char>('0' + (msi / 100) % 10));
    append_2(buf, msi % 100);
    buf.push_back('Z');
}

const char* level_tag(LogLevel lv)
{
    switch (lv) {
        case LogLevel::Error: return " E ";
        case LogLevel::Warn:  return " W ";
        case LogLevel::Info:  return " I ";
        case LogLevel::Debug: return " D ";
    }
    return " I ";
}

}  // namespace

LogLine& LogLine::operator<<(const char* s)        { if (s) buf_.append(s); return *this; }
LogLine& LogLine::operator<<(const std::string& s) { buf_.append(s); return *this; }
LogLine& LogLine::operator<<(char c)               { buf_.push_back(c); return *this; }
LogLine& LogLine::operator<<(bool b)               { buf_.append(b ? "true" : "false"); return *this; }
LogLine& LogLine::operator<<(int v)                { append_int(buf_, v); return *this; }
LogLine& LogLine::operator<<(unsigned v)           { append_int(buf_, v); return *this; }
LogLine& LogLine::operator<<(long v)               { append_int(buf_, v); return *this; }
LogLine& LogLine::operator<<(unsigned long v)      { append_int(buf_, v); return *this; }
LogLine& LogLine::operator<<(long long v)          { append_int(buf_, v); return *this; }
LogLine& LogLine::operator<<(unsigned long long v) { append_int(buf_, v); return *this; }

std::string log_hex(unsigned long long v)
{
    char       tmp[16];
    const auto r = std::to_chars(tmp, tmp + sizeof(tmp), v, 16);
    std::string out = "0x";
    if (r.ec == std::errc{}) out.append(tmp, static_cast<size_t>(r.ptr - tmp));
    return out;
}

void log_emit(const LogLine& line)
{
    // 최종 줄을 하나의 버퍼로 만든 뒤에야 write 를 부른다. 여기서 두 번 쓰면
    // 이 모듈의 존재 이유가 사라진다.
    std::string out;
    out.reserve(line.text().size() + 32);
    append_timestamp(out);
    out.append(level_tag(line.level()));
    out.append(line.text());
    out.push_back('\n');

#if defined(_WIN32)
    const int fd = _fileno(stderr);
#else
    const int fd = 2;
#endif
    // 줄이 PIPE_BUF 를 넘지 않는 한 커널이 쪼개지 않으므로 한 번으로 끝난다.
    // 그래도 부분 write 는 가능한 반환값이라 남은 만큼 이어 쓴다 — 그 경우에만
    // 원자성이 깨지고, 그건 이미 비정상 상황이다.
    const char* p    = out.data();
    size_t      left = out.size();
    while (left > 0) {
#if defined(_WIN32)
        const int n = _write(fd, p, static_cast<unsigned int>(left));
#else
        const auto n = ::write(fd, p, left);
#endif
        if (n <= 0) break;   // 더 쓸 수 없다 — 로그 때문에 서버를 세우지는 않는다
        p    += n;
        left -= static_cast<size_t>(n);
    }
}

}  // namespace relay
