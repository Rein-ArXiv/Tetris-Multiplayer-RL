#pragma once
// Behavioral API double, not Windows headers or an ABI emulation.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>
#include <algorithm>
using HRESULT=std::int32_t;using DWORD=std::uint32_t;using UINT32=std::uint32_t;
using UINT64=std::uint64_t;using WORD=std::uint16_t;using BYTE=std::uint8_t;
using ULONG=unsigned long;using LPCWSTR=const wchar_t*;
constexpr HRESULT S_OK=0,S_FALSE=1,E_FAIL=-1,RPC_E_CHANGED_MODE=static_cast<HRESULT>(0x80010106u);
#define FAILED(hr) ((hr)<0)
#define SUCCEEDED(hr) ((hr)>=0)
constexpr DWORD COINIT_MULTITHREADED=0;
constexpr UINT32 WAVE_FORMAT_PCM=1,XAUDIO2_DEFAULT_PROCESSOR=0,XAUDIO2_END_OF_STREAM=0x40,
 XAUDIO2_VOICE_NOSAMPLESPLAYED=0x100,XAUDIO2_MAX_BUFFER_BYTES=0x80000000u,XAUDIO2_LOOP_INFINITE=255;
struct WAVEFORMATEX{WORD wFormatTag=0,nChannels=0;DWORD nSamplesPerSec=0,nAvgBytesPerSec=0;WORD nBlockAlign=0,wBitsPerSample=0,cbSize=0;};
struct XAUDIO2_BUFFER{UINT32 Flags=0,AudioBytes=0;const BYTE*pAudioData=nullptr;UINT32 PlayBegin=0,PlayLength=0,LoopBegin=0,LoopLength=0,LoopCount=0;void*pContext=nullptr;};
struct XAUDIO2_VOICE_STATE{void*pCurrentBufferContext=nullptr;UINT32 BuffersQueued=0;UINT64 SamplesPlayed=0;};
struct IXAudio2SourceVoice;struct IXAudio2MasteringVoice;struct IXAudio2;
namespace fake_xaudio {
enum class Fail{none,com,engine,master,source,submit,start};
inline Fail fail=Fail::none;
inline HRESULT com_result=S_OK;
inline int com_refs=0,inits=0,uninits=0,engines=0,masters=0,sources=0,submits=0,starts=0,destroys=0;
inline unsigned touched=0;
inline std::vector<std::unique_ptr<IXAudio2SourceVoice>> source_objects;
inline std::vector<std::unique_ptr<IXAudio2MasteringVoice>> master_objects;
inline std::vector<std::unique_ptr<IXAudio2>> engine_objects;
inline void require(bool b,const char*message){if(!b){std::fprintf(stderr,"XAudio API double: %s\n",message);std::abort();}}
}
inline HRESULT CoInitializeEx(void*,DWORD){using namespace fake_xaudio;++inits;if(fail==Fail::com)return E_FAIL;if(SUCCEEDED(com_result))++com_refs;return com_result;}
inline void CoUninitialize(){using namespace fake_xaudio;require(com_refs>0,"unbalanced COM release");--com_refs;++uninits;}
struct IXAudio2SourceVoice {
 IXAudio2*engine=nullptr;
 bool alive=true,started=false;XAUDIO2_BUFFER buffer{};WAVEFORMATEX format{};UINT32 queued=0;
 void touch()const{if(alive&&queued&&buffer.pAudioData){fake_xaudio::touched^=buffer.pAudioData[0];fake_xaudio::touched^=buffer.pAudioData[buffer.AudioBytes-1];}}
 void DestroyVoice(){using namespace fake_xaudio;require(alive,"double source destroy");touch();alive=false;buffer={};queued=0;--sources;++destroys;}
 HRESULT Stop(UINT32=0,UINT32=0){return S_OK;}
 // A delayed stop/flush may leave the currently borrowed buffer pending.
 HRESULT FlushSourceBuffers(){return S_OK;}
 HRESULT SetVolume(float,UINT32=0){return S_OK;}
 void GetState(XAUDIO2_VOICE_STATE*out,UINT32=0)const{fake_xaudio::require(alive,"state of dead voice");out->BuffersQueued=queued;out->SamplesPlayed=0;}
 HRESULT SubmitSourceBuffer(const XAUDIO2_BUFFER*b,const void* =nullptr){
  using namespace fake_xaudio;require(alive&&b,"submit target");++submits;if(fail==Fail::submit)return E_FAIL;
  require(b->AudioBytes>0&&b->pAudioData,"empty PCM");require(b->AudioBytes%format.nBlockAlign==0,"partial PCM frame");
  buffer=*b;queued=1;touch();return S_OK;
 }
 HRESULT Start(UINT32=0,UINT32=0){using namespace fake_xaudio;++starts;if(fail==Fail::start)return E_FAIL;started=true;touch();return S_OK;}
};
struct IXAudio2MasteringVoice {
 IXAudio2*engine=nullptr;
 bool alive=true;
 void DestroyVoice(){using namespace fake_xaudio;require(alive&&std::none_of(source_objects.begin(),source_objects.end(),[this](const auto&v){return v->alive&&v->engine==engine;}),"master before sources");alive=false;--masters;}
};
struct IXAudio2 {
 bool alive=true;
 HRESULT CreateMasteringVoice(IXAudio2MasteringVoice**out,UINT32=0,UINT32=0,UINT32=0,LPCWSTR=nullptr,const void* =nullptr,int=0){
  using namespace fake_xaudio;require(alive,"master on dead engine");*out=nullptr;if(fail==Fail::master)return E_FAIL;
  master_objects.push_back(std::make_unique<IXAudio2MasteringVoice>());*out=master_objects.back().get();(*out)->engine=this;++masters;return S_OK;
 }
 HRESULT CreateSourceVoice(IXAudio2SourceVoice**out,const WAVEFORMATEX*f,UINT32=0,float=2.0f,void* =nullptr,const void* =nullptr,const void* =nullptr){
  using namespace fake_xaudio;require(alive&&std::any_of(master_objects.begin(),master_objects.end(),[this](const auto&m){return m->alive&&m->engine==this;}),"source without master");*out=nullptr;if(fail==Fail::source)return E_FAIL;
  require(f->wFormatTag==WAVE_FORMAT_PCM&&f->wBitsPerSample==16,"format tag/depth");
  require(f->nBlockAlign==f->nChannels*2&&f->nAvgBytesPerSec==f->nSamplesPerSec*f->nBlockAlign&&f->cbSize==0,"format units");
  source_objects.push_back(std::make_unique<IXAudio2SourceVoice>());*out=source_objects.back().get();(*out)->format=*f;(*out)->engine=this;++sources;return S_OK;
 }
 ULONG Release(){using namespace fake_xaudio;require(alive&&std::none_of(master_objects.begin(),master_objects.end(),[this](const auto&m){return m->alive&&m->engine==this;})&&std::none_of(source_objects.begin(),source_objects.end(),[this](const auto&v){return v->alive&&v->engine==this;}),"engine before voices");alive=false;--engines;return 0;}
};
inline HRESULT XAudio2Create(IXAudio2**out,UINT32=0,UINT32=XAUDIO2_DEFAULT_PROCESSOR){using namespace fake_xaudio;*out=nullptr;if(fail==Fail::engine)return E_FAIL;engine_objects.push_back(std::make_unique<IXAudio2>());*out=engine_objects.back().get();++engines;return S_OK;}
namespace fake_xaudio {
inline void reset(){require(engines==0&&masters==0&&sources==0,"reset while graph is alive");source_objects.clear();master_objects.clear();engine_objects.clear();fail=Fail::none;com_result=S_OK;com_refs=inits=uninits=submits=starts=destroys=0;touched=0;}
inline void read_all(){for(auto&v:source_objects)v->touch();}
inline void finish_all(){for(auto&v:source_objects)if(v->alive){v->touch();v->queued=0;v->buffer={};}}
inline int readers_of(std::int16_t value){int n=0;for(auto&v:source_objects)if(v->alive&&v->queued){std::int16_t sample;std::memcpy(&sample,v->buffer.pAudioData,sizeof sample);if(sample==value)++n;}return n;}
}
