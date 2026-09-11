#pragma once
struct ssl_ctx_st;
namespace net {
// Supplement OpenSSL's file-based CA lookup with the OS trust store on desktops.
void load_native_trust(ssl_ctx_st* context);
}
