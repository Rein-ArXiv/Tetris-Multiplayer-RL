#ifndef STUDY_NET_BYTE_CODEC_H
#define STUDY_NET_BYTE_CODEC_H

// Educational little-endian byte codec for the study_net namespace.
//
// ByteReader and ByteWriter only borrow caller-provided storage. They never
// allocate, copy whole structs, throw, or recover on their own. Every read or
// write validates first and, on failure, leaves the cursor, the caller output,
// and the destination storage untouched. Callers must short-circuit on false.

#include <climits>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace study_net {

static_assert(CHAR_BIT == 8, "study_net byte codec requires 8-bit bytes");

// The codec reads and writes fields with explicit little-endian shifts and
// masks. It cannot prove that a non-null borrowed pointer refers to a live
// object, nor that [data, data + size) lies inside one allocation; that
// contract belongs to the caller. A null pointer with a zero extent is valid
// and is never dereferenced or used in pointer arithmetic.

class ByteReader {
public:
  ByteReader(const uint8_t* data, size_t size) noexcept
      : data_(data), size_(size), position_(0), valid_(data != nullptr || size == 0) {}

  bool valid() const noexcept { return valid_; }
  size_t position() const noexcept { return position_; }
  size_t remaining() const noexcept { return valid_ ? size_ - position_ : 0; }
  bool at_end() const noexcept { return valid_ && position_ == size_; }

  bool u8(uint8_t& out) noexcept { return read_unsigned<uint8_t>(out); }
  bool u16(uint16_t& out) noexcept { return read_unsigned<uint16_t>(out); }
  bool u32(uint32_t& out) noexcept { return read_unsigned<uint32_t>(out); }
  bool u64(uint64_t& out) noexcept { return read_unsigned<uint64_t>(out); }

private:
  template <typename T>
  bool read_unsigned(T& out) noexcept {
    static_assert(std::is_integral<T>::value && std::is_unsigned<T>::value,
                  "read_unsigned<T> requires an unsigned integral T");
    static_assert(!std::is_same<T, bool>::value,
                  "read_unsigned<bool> is not supported");
    static_assert(sizeof(T) <= sizeof(uint64_t),
                  "read_unsigned<T> supports widths up to 64 bits");

    // Complete-field precheck: consume nothing unless the whole field fits.
    if (!valid_ || remaining() < sizeof(T)) {
      return false;
    }

    // Accumulate into a local candidate so a failure cannot disturb `out`.
    T candidate = 0;
    for (size_t i = 0; i < sizeof(T); ++i) {
      // Widen each byte to T before shifting; the largest shift here is 56.
      const T byte = static_cast<T>(data_[position_ + i]);
      candidate |= static_cast<T>(byte << (i * 8));
    }

    position_ += sizeof(T);
    out = candidate;
    return true;
  }

  const uint8_t* data_;
  size_t size_;
  size_t position_;
  bool valid_;
};

class ByteWriter {
public:
  ByteWriter(uint8_t* data, size_t capacity) noexcept
      : data_(data), capacity_(capacity), position_(0), valid_(data != nullptr || capacity == 0) {}

  bool valid() const noexcept { return valid_; }
  size_t position() const noexcept { return position_; }
  size_t remaining() const noexcept { return valid_ ? capacity_ - position_ : 0; }

  bool u8(uint8_t value) noexcept { return write_unsigned<uint8_t>(value); }
  bool u16(uint16_t value) noexcept { return write_unsigned<uint16_t>(value); }
  bool u32(uint32_t value) noexcept { return write_unsigned<uint32_t>(value); }
  bool u64(uint64_t value) noexcept { return write_unsigned<uint64_t>(value); }

private:
  template <typename T>
  bool write_unsigned(T value) noexcept {
    static_assert(std::is_integral<T>::value && std::is_unsigned<T>::value,
                  "write_unsigned<T> requires an unsigned integral T");
    static_assert(!std::is_same<T, bool>::value,
                  "write_unsigned<bool> is not supported");
    static_assert(sizeof(T) <= sizeof(uint64_t),
                  "write_unsigned<T> supports widths up to 64 bits");

    // Full-field precheck so a partial field is never written.
    if (!valid_ || remaining() < sizeof(T)) {
      return false;
    }

    for (size_t i = 0; i < sizeof(T); ++i) {
      data_[position_ + i] = static_cast<uint8_t>((value >> (i * 8)) & 0xFFu);
    }

    position_ += sizeof(T);
    return true;
  }

  uint8_t* data_;
  size_t capacity_;
  size_t position_;
  bool valid_;
};

}  // namespace study_net

#endif  // STUDY_NET_BYTE_CODEC_H
