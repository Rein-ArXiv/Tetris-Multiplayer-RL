#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace net {
// Byte-stream adapter used only by the game client. The reactor continues to use
// real file descriptors. WSS framing/TLS belong below the Tetris frame parser.
class StreamTransport {
public:
    virtual ~StreamTransport() = default;
    virtual bool alive() const = 0;
    virtual bool send(const void* data, size_t size) = 0;
    virtual bool receive(std::vector<uint8_t>& out) = 0;
    virtual void close() = 0;
};
}
