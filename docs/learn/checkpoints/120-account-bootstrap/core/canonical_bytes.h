#ifndef STUDY_HASH_CANONICAL_BYTES_H
#define STUDY_HASH_CANONICAL_BYTES_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace study_hash {

// 고정 용량 바이트 버퍼. 오버플로 시 ok_=false로 고정되며 이후 쓰기는 무시된다.
template <std::size_t Capacity>
class Bytes {
public:
    // 1바이트 추가.
    void u8(std::uint8_t v) noexcept {
        if (!ok_) return;
        if (Capacity - size_ < 1) { ok_ = false; return; }
        data_[size_++] = v;
    }

    // 32비트 리틀엔디언 명시 기록. 전체 폭 capacity 검증 후 기록.
    void u32(std::uint32_t v) noexcept {
        if (!ok_) return;
        if (Capacity - size_ < 4) { ok_ = false; return; }
        data_[size_++] = static_cast<std::uint8_t>(v & 0xFFu);
        data_[size_++] = static_cast<std::uint8_t>((v >> 8) & 0xFFu);
        data_[size_++] = static_cast<std::uint8_t>((v >> 16) & 0xFFu);
        data_[size_++] = static_cast<std::uint8_t>((v >> 24) & 0xFFu);
    }

    // 64비트 리틀엔디언 명시 기록.
    void u64(std::uint64_t v) noexcept {
        if (!ok_) return;
        if (Capacity - size_ < 8) { ok_ = false; return; }
        for (int i = 0; i < 8; ++i) {
            data_[size_++] = static_cast<std::uint8_t>((v >> (8 * i)) & 0xFFu);
        }
    }

    // 음수는 uint32_t로 변환하여 modulo 2^32 표현.
    void i32(std::int32_t v) noexcept { u32(static_cast<std::uint32_t>(v)); }

    std::size_t size() const noexcept { return size_; }
    const std::uint8_t* data() const noexcept { return data_.data(); }
    bool ok() const noexcept { return ok_; }

    // !ok_면 nullopt. 성공 시 사용된 바이트만 FNV-1a 64로 해시.
    // 비교 보조용이며 암호 인증이 아니다.
    std::optional<std::uint64_t> digest() const noexcept {
        if (!ok_) return std::nullopt;
        std::uint64_t h = 14695981039346656037ull;
        for (std::size_t i = 0; i < size_; ++i) {
            h ^= static_cast<std::uint64_t>(data_[i]);
            h *= 1099511628211ull;
        }
        return h;
    }

private:
    std::array<std::uint8_t, Capacity> data_{};
    std::size_t size_ = 0;
    bool ok_ = true;
};

}  // namespace study_hash

#endif
