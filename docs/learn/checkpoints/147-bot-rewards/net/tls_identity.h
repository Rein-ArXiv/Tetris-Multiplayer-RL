#pragma once
#include <openssl/ssl.h>
#include <openssl/x509v3.h>
#include <memory>
#include <string>

namespace study_net::tls_identity {
// Reference identity comes from the configured URL, never reverse DNS or CN.
// Require SAN; permit whole-label DNS wildcards, not partial-label wildcards.
inline bool matches_host(X509* certificate, const std::string& host) {
    if (!certificate || host.empty() || host.find('\0') != std::string::npos) return false;
    std::unique_ptr<ASN1_OCTET_STRING, decltype(&ASN1_OCTET_STRING_free)> address(
        a2i_IPADDRESS(host.c_str()), ASN1_OCTET_STRING_free);
    if (address) {
        return X509_check_ip(certificate, ASN1_STRING_get0_data(address.get()),
                             static_cast<std::size_t>(ASN1_STRING_length(address.get())), 0) == 1;
    }
    return X509_check_host(certificate, host.data(), host.size(),
        X509_CHECK_FLAG_NEVER_CHECK_SUBJECT | X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS,
        nullptr) == 1;
}

// cpp-httplib's custom verifier replaces its default chain AND name checks.
// Preserve the chain result and require a peer certificate before checking SAN.
inline bool verified_peer(SSL* ssl, const std::string& host) {
    if (!ssl || SSL_get_verify_result(ssl) != X509_V_OK) return false;
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
    X509* raw = SSL_get1_peer_certificate(ssl);
#else
    X509* raw = SSL_get_peer_certificate(ssl);
#endif
    std::unique_ptr<X509, decltype(&X509_free)> certificate(raw, X509_free);
    return matches_host(certificate.get(), host);
}
} // namespace study_net::tls_identity
