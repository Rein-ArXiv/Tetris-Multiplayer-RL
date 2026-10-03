#include "../meta/tls_identity.h"
#include <cstdio>
#include <stdexcept>
using Certificate = std::unique_ptr<X509, decltype(&X509_free)>;
void check(bool value) { if (!value) throw std::runtime_error("TLS identity contract"); }
Certificate certificate(const char* cn, const char* san) {
    Certificate cert(X509_new(), X509_free); check(bool(cert));
    check(X509_NAME_add_entry_by_txt(X509_get_subject_name(cert.get()), "CN", MBSTRING_ASC,
        reinterpret_cast<const unsigned char*>(cn), -1, -1, 0) == 1);
    if (san) {
        std::unique_ptr<X509_EXTENSION, decltype(&X509_EXTENSION_free)> extension(
            X509V3_EXT_conf_nid(nullptr, nullptr, NID_subject_alt_name, san), X509_EXTENSION_free);
        check(bool(extension)); check(X509_add_ext(cert.get(), extension.get(), -1) == 1);
    }
    return cert;
}
int main() {
    using meta::client::tls_identity::matches_host;
    try {
        auto dns = certificate("wrong.test", "DNS:api.example.test");
        check(matches_host(dns.get(), "api.example.test"));
        check(matches_host(dns.get(), "API.EXAMPLE.TEST"));
        check(!matches_host(dns.get(), "other.example.test"));
        check(!matches_host(dns.get(), std::string("api.example.test\0evil",21)));
        check(!matches_host(dns.get(), "")); check(!matches_host(nullptr,"api.example.test"));
        auto conflict = certificate("api.example.test", "DNS:other.example.test");
        check(!matches_host(conflict.get(),"api.example.test"));
        auto legacy = certificate("api.example.test", nullptr);
        check(!matches_host(legacy.get(),"api.example.test"));
        auto wildcard = certificate("irrelevant", "DNS:*.example.test");
        check(matches_host(wildcard.get(),"api.example.test"));
        check(!matches_host(wildcard.get(),"nested.api.example.test"));
        check(!matches_host(wildcard.get(),"example.test"));
        auto partial = certificate("irrelevant", "DNS:a*.example.test");
        check(!matches_host(partial.get(),"api.example.test"));
        auto address = certificate("irrelevant", "IP:127.0.0.1,IP:::1");
        check(matches_host(address.get(),"127.0.0.1")); check(matches_host(address.get(),"::1"));
        check(!matches_host(address.get(),"127.0.0.2"));check(!matches_host(address.get(),"localhost"));
        auto numericDns = certificate("127.0.0.1", "DNS:127.0.0.1");
        check(!matches_host(numericDns.get(),"127.0.0.1"));
        check(!meta::client::tls_identity::verified_peer(nullptr,"localhost"));
        std::puts("TLS identity: SAN required, DNS/IP distinction, wildcard boundary, absent peer and embedded NUL passed");
    } catch (const std::exception& e) { std::fprintf(stderr,"%s\n",e.what()); return 1; }
}
