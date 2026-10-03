#include "image_decode.h"
#include <fstream>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <utility>
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#define STBI_MAX_DIMENSIONS 8192
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
namespace study_image {
namespace {
Result failure(Error error) noexcept { return {std::nullopt,error}; }
bool allowed(int width,int height) noexcept {
    const auto bytes=study_texture::rgba_byte_count(width,height);
    return bytes && width<=max_side && height<=max_side && *bytes<=max_decoded_bytes;
}
using StbiPixels = std::unique_ptr<stbi_uc,decltype(&stbi_image_free)>;
}
const char* error_text(Error error) noexcept {
    switch(error){
    case Error::none:return "success";
    case Error::invalid_input:return "empty/null encoded input";
    case Error::encoded_limit:return "encoded file exceeds 8 MiB";
    case Error::file_io:return "file open/seek/read failed";
    case Error::decode_failed:return "PNG/JPEG decode failed (format, data or decoder resources)";
    case Error::dimensions:return "decoded dimensions exceed policy";
    case Error::allocation:return "CPU pixel allocation failed";
    }
    return "unknown image error";
}
Result decode_memory(const std::uint8_t* bytes,std::size_t size) noexcept {
    if(!bytes || !size)return failure(Error::invalid_input);
    if(size>max_encoded_bytes)return failure(Error::encoded_limit);
    static_assert(max_encoded_bytes<=static_cast<std::size_t>((std::numeric_limits<int>::max)()));
    const auto length=static_cast<int>(size);
    int width=0,height=0,source_channels=0;
    if(!stbi_info_from_memory(bytes,length,&width,&height,&source_channels))
        return failure(Error::decode_failed);
    if(!allowed(width,height))return failure(Error::dimensions);
    try {
        StbiPixels decoded(stbi_load_from_memory(bytes,length,&width,&height,&source_channels,4),stbi_image_free);
        if(!decoded)return failure(Error::decode_failed);
        // Header inspection is not proof that decompression succeeded.
        if(!allowed(width,height))return failure(Error::dimensions);
        const auto count=*study_texture::rgba_byte_count(width,height);
        Image result;
        result.width=width;result.height=height;result.source_channels=source_channels;
        result.pixels.assign(decoded.get(),decoded.get()+count);
        return {std::move(result),Error::none};
    } catch(const std::bad_alloc&) {return failure(Error::allocation);}
      catch(const std::length_error&) {return failure(Error::allocation);}
}
Result decode_file(const std::filesystem::path& path) noexcept {
    try {
        std::ifstream file(path,std::ios::binary|std::ios::ate);
        if(!file)return failure(Error::file_io);
        const auto end=file.tellg();
        if(end<0)return failure(Error::file_io);
        if(end==0)return failure(Error::invalid_input);
        if(end>static_cast<std::streamoff>(max_encoded_bytes))return failure(Error::encoded_limit);
        const auto count=static_cast<std::size_t>(end);
        std::vector<std::uint8_t> bytes(count);
        file.seekg(0,std::ios::beg);
        if(!file || !file.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(count)))
            return failure(Error::file_io);
        // Detect growth after tellg instead of silently decoding a prefix.
        if(file.peek()!=std::char_traits<char>::eof() || file.bad())return failure(Error::file_io);
        return decode_memory(bytes.data(),bytes.size());
    } catch(const std::bad_alloc&) {return failure(Error::allocation);}
      catch(const std::length_error&) {return failure(Error::allocation);}
      catch(const std::ios_base::failure&) {return failure(Error::file_io);}
}
} // namespace study_image
