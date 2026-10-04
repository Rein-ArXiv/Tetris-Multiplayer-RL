#pragma once
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
namespace study_meta {
inline bool credential_hex(const std::string& value, std::size_t size) {
    return value.size()==size && value.find_first_not_of("0123456789abcdef")==std::string::npos;
}
inline std::string hex_bytes(const unsigned char* bytes, std::size_t size) {
    static constexpr char digits[]="0123456789abcdef";
    std::string out;out.reserve(size*2);
    for(std::size_t i=0;i<size;++i){out+=digits[bytes[i]>>4];out+=digits[bytes[i]&15];}
    return out;
}
inline std::string account_hash(const std::string& token) {
    if(!credential_hex(token,32))throw std::invalid_argument("invalid account token");
    const std::string input="study-account-v1:"+token;
    unsigned char bytes[EVP_MAX_MD_SIZE];unsigned size=0;
    if(EVP_Digest(input.data(),input.size(),bytes,&size,EVP_sha256(),nullptr)!=1 || size!=32)
        throw std::runtime_error("digest unavailable");
    return hex_bytes(bytes,size);
}
struct IssuedAccount { std::uint64_t player_id=0; std::string token; };
using RandomBytes = int (*)(unsigned char*, int);
// Injection is for local failure tests. Production callers use RAND_bytes.
inline IssuedAccount new_identity(RandomBytes random=RAND_bytes) {
    std::array<unsigned char,24> bytes{};
    if(random(bytes.data(),static_cast<int>(bytes.size()))!=1)
        throw std::runtime_error("entropy unavailable");
    std::uint64_t id=0;
    for(unsigned i=0;i<8;++i)id=(id<<8)|bytes[i];
    return {id,hex_bytes(bytes.data()+8,16)};
}
} // namespace study_meta
