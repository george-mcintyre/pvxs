
#include <openssl/opensslv.h>

#ifndef OPENSSL_VERSION_NUMBER
#  error Some antique OpenSSL version?
#endif
#if OPENSSL_VERSION_NUMBER < 0x30000000L
#  error Minimum OpenSSL 3.0
#endif

// Pull in runtime APIs to force link to libssl and libcrypto
#include <openssl/ssl.h>
#include <openssl/evp.h>

#include <event2/event-config.h>

#ifndef EVENT__HAVE_OPENSSL
#  error libevent not built with OpenSSL support
#endif

// Also include core event headers to construct minimal objects
#include <event2/event.h>
#include <event2/bufferevent_ssl.h>

// Provide a main() so the configure step performs a real link test.
// Reference symbols from libcrypto, libssl, and libevent_openssl.
int main(void)
{
    // libcrypto: reference a well-known digest
    const EVP_MD* md = EVP_sha256();
    (void)md;

    // libssl: initialize and construct a context
    if (OPENSSL_init_ssl(0, NULL) == 0) {
        return 1;
    }
    SSL_CTX* ctx = SSL_CTX_new(TLS_method());
    if (!ctx) return 1;

    SSL* ssl = SSL_new(ctx);
    if (!ssl) {
        SSL_CTX_free(ctx);
        return 1;
    }

    // libevent_openssl (+ core): create an event_base and a bufferevent using SSL*
    struct event_base* base = event_base_new();
    if (!base) {
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        return 1;
    }

    struct bufferevent* bev = bufferevent_openssl_filter_new(
        base,
        NULL, // no underlying bufferevent
        ssl,
        BUFFEREVENT_SSL_OPEN,
        BEV_OPT_CLOSE_ON_FREE);

    // We only need to reference the symbol; handle cleanup best-effort
    if (bev) {
        bufferevent_free(bev); // will free ssl if BEV_OPT_CLOSE_ON_FREE set
        ssl = NULL; // avoid double free
    }

    if (base) event_base_free(base);
    if (ssl) SSL_free(ssl);
    SSL_CTX_free(ctx);

    return 0;
}
