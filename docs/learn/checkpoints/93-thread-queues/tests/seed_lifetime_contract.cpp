#include "net/seed_handshake.h"
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
int main() {
 using namespace study_net;Runtime runtime;CHECK(runtime.ready());
 Connection c;Socket empty;CHECK(!c.adopt(empty)&&!empty.valid()&&!c.active());
 Config invalid{77,601,2};CHECK(negotiate_host(c,invalid,invalid)==HandshakeResult::invalid_local&&invalid.countdown_ticks==601);
 Config sentinel{9,10,11},agreed=sentinel;
 CHECK(negotiate_host(c,Config{},agreed)==HandshakeResult::receive_failed&&equal_config(agreed,sentinel));
 CHECK(negotiate_peer(c,agreed)==HandshakeResult::send_failed&&equal_config(agreed,sentinel));
 int error=0;uint16_t port=0;auto listener=listen_loopback(0,port,error);CHECK(listener.valid());
 auto peer=connect_loopback(port,error);CHECK(peer.valid());auto accepted=accept_one(listener,error);CHECK(accepted.valid());
 CHECK(c.adopt(accepted)&&!accepted.valid()&&c.active());
 CHECK(!c.adopt(peer)&&peer.valid()&&c.active());
 CHECK(negotiate_host(c,invalid,invalid)==HandshakeResult::invalid_local&&c.active()&&invalid.seed==77);
 c.close();CHECK(c.adopt(peer)&&!peer.valid());c.close();
 std::puts("Handshake lifetime: failed adoption preserves owner, success transfers, busy preserves, invalid local aliases preserved");
}
