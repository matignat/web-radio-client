#include "common.h"

void stream_audio_plain_tls(SSL *ssl, int sock, const string &body_prefix, int timeout_ms);
void stream_audio_with_metadata_tls(SSL *ssl, int sock, const string &body_prefix, size_t metaint, int timeout_ms);
HttpResponse read_response_headers_tls(SSL *ssl, int sock, int timeout_ms);
void tls_handshake(int sock, const string &host, SSL_CTX **out_ctx, SSL **out_ssl);
void tls_teardown(SSL_CTX *ctx, SSL *ssl);