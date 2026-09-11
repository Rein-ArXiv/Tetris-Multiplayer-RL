#include "system_trust.h"
#include <openssl/ssl.h>
#if defined(_WIN32) || defined(__APPLE__)
// Reuse the platform certificate-store adapter already shipped with cpp-httplib.
// Keep this dependency in one TU, outside Beast's templates and the game loop.
#ifndef CPPHTTPLIB_OPENSSL_SUPPORT
#define CPPHTTPLIB_OPENSSL_SUPPORT
#endif
#if defined(__APPLE__)
#define CPPHTTPLIB_USE_CERTS_FROM_MACOSX_KEYCHAIN
#endif
#include "../third_party/httplib.h"
#endif

namespace net {
void load_native_trust(ssl_ctx_st* context) {
#if defined(_WIN32)
    httplib::detail::load_system_certs_on_windows(SSL_CTX_get_cert_store(context));
#elif defined(__APPLE__)
    httplib::detail::load_system_certs_on_macos(SSL_CTX_get_cert_store(context));
#else
    (void)context; // OpenSSL's default CA paths are the Linux system trust store.
#endif
}
}
