#include "audio/load_clip.h"
#include "third_party/dr_mp3.h"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <algorithm>
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"MP3 line %d: %s\n",__LINE__,#e);std::exit(1);}}while(false)
using audio_mp3::Error;
static std::vector<uint8_t> read(const char* p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
static void put(const std::filesystem::path& p,const std::vector<uint8_t>& v){std::ofstream f(p,std::ios::binary);f.write(reinterpret_cast<const char*>(v.data()),static_cast<std::streamsize>(v.size()));CHECK(f);}
int main(int argc,char**argv){
 CHECK(argc==3);const auto bytes=read(argv[1]);CHECK(!bytes.empty());
 const auto decoded=audio_mp3::decode(bytes);CHECK(decoded&&decoded.channels==2&&decoded.rate==48000&&decoded.samples.size()==32256);
 drmp3_config cfg{};drmp3_uint64 frames=0;
 auto* reference=drmp3_open_memory_and_read_pcm_frames_s16(bytes.data(),bytes.size(),&cfg,&frames,nullptr);CHECK(reference);
 CHECK(cfg.channels==decoded.channels&&cfg.sampleRate==decoded.rate&&frames*cfg.channels==decoded.samples.size());
 CHECK(std::equal(decoded.samples.begin(),decoded.samples.end(),reference));drmp3_free(reference,nullptr);
 CHECK(audio_mp3::decode(bytes,{bytes.size(),64512}));
 auto failed=audio_mp3::decode(bytes,{bytes.size(),64511});CHECK(!failed&&failed.error==Error::pcm_too_large&&failed.samples.empty()&&failed.samples.capacity()==0);
 CHECK(audio_mp3::decode(bytes,{bytes.size()-1,64512}).error==Error::file_too_large);
 CHECK(audio_mp3::decode(bytes,{bytes.size(),0}).error==Error::invalid_argument);
 CHECK(audio_mp3::decode(bytes,{bytes.size(),1}).error==Error::pcm_too_large);
 CHECK(audio_mp3::decode({}).error==Error::empty);
 CHECK(audio_mp3::decode({'n','o','t',' ','m','p','3'}).error==Error::decode_failed);
 CHECK(audio_mp3::load(nullptr).error==Error::invalid_argument&&audio_mp3::load("").error==Error::invalid_argument);
 const auto dir=std::filesystem::path(argv[2]);std::filesystem::remove_all(dir);std::filesystem::create_directories(dir);
 CHECK(audio_mp3::load((dir/"missing.mp3").string().c_str()).error==Error::io);
 put(dir/"empty.mp3",{});CHECK(audio_mp3::load((dir/"empty.mp3").string().c_str()).error==Error::empty);
 put(dir/"not.mp3",{'x','y','z'});CHECK(audio_mp3::load((dir/"not.mp3").string().c_str()).error==Error::decode_failed);
 // The format is read from bytes, not inferred from the filename suffix.
 put(dir/"sound.data",bytes);auto clip=study_audio::load_clip((dir/"sound.data").string().c_str());CHECK(clip&&clip.pcm->layout().frames==16128&&clip.pcm->layout().bytes==64512);
 CHECK(clip.pcm->samples()==decoded.samples);
 CHECK(audio_mp3::load(argv[1],{bytes.size()-1,64512}).error==Error::file_too_large);
 CHECK(audio_mp3::load(argv[1],{bytes.size(),64512}));
 // Truncation may return a decodable prefix: never claim strict file validation.
 std::size_t accepted=0;
 for(std::size_t n=1;n<bytes.size();n+=97){auto prefix=bytes;prefix.resize(n);auto part=audio_mp3::decode(prefix);if(part){++accepted;CHECK(part.samples.size()%part.channels==0&&part.samples.size()<=decoded.samples.size());}else CHECK(part.samples.empty());}
 CHECK(accepted>0);
 uint32_t state=12345;
 for(int i=0;i<200;++i){std::vector<uint8_t> junk(513);for(auto& b:junk){state^=state<<13;state^=state>>17;state^=state<<5;b=static_cast<uint8_t>(state);}auto result=audio_mp3::decode(junk,{1024,4096});if(result)CHECK(result.samples.size()*2<=4096&&result.samples.size()%result.channels==0);else CHECK(result.samples.empty());}
 std::printf("MP3 real sample equality, exact byte budgets, file failures and %zu decodable prefixes passed.\n",accepted);
}
