#include "core/hash.h"
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
int main(){
    CHECK((uint64_t(11)^11)==(uint64_t(29)^29)); // XOR blind spot.
    CHECK(hash_player_pair(11,11)!=hash_player_pair(29,29));
    CHECK(hash_player_pair(11,29)!=hash_player_pair(29,11));
    const uint64_t hostLocal=11,hostRemote=29,peerLocal=29,peerRemote=11;
    CHECK(hash_player_pair(hostLocal,hostRemote)==hash_player_pair(peerRemote,peerLocal));
    const uint8_t bytes[]={'D','U','E','L',1,11,0,0,0,0,0,0,0,29,0,0,0,0,0,0,0};
    CHECK(hash_player_pair(11,29)==fnv1a64(bytes,sizeof(bytes)));
    std::printf("PAIR_VECTOR %llu\n",static_cast<unsigned long long>(hash_player_pair(11,29)));
}
