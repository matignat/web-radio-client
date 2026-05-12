/*
 * TLS support for the internet radio client.
 *
 * Handles SSL/TLS session setup and cleanup, reads HTTP responses over secure
 * connections, and implements audio streaming with optional ICY metadata support.
 */

#include "common.h"
#include "http_handler.h"

// Performs TLS handshake on the given socket and returns the SSL context and SSL object.
void tls_handshake(int sock, const string &host, SSL_CTX **out_ctx, SSL **out_ssl) {
    SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) {
        throw runtime_error("SSL_CTX_new failed");
    }
    SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, nullptr);

    SSL *ssl = SSL_new(ctx);
    if (!ssl) {
        SSL_CTX_free(ctx);
        throw runtime_error("SSL_new failed");
    }

    if (SSL_set_tlsext_host_name(ssl, host.c_str()) != 1) {
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        throw runtime_error("SSL_set_tlsext_host_name failed");
    }

    if (SSL_set_fd(ssl, sock) != 1) {
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        throw runtime_error("SSL_set_fd failed");
    }

    if (SSL_connect(ssl) != 1) {
        char buf[BITS_256];
        ERR_error_string_n(ERR_get_error(), buf, sizeof(buf));
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        throw runtime_error(string("SSL_connect failed: ") + buf);
    }

    *out_ctx = ctx;
    *out_ssl = ssl;
}

// Clean up SSL context and SSL object.
void tls_teardown(SSL_CTX *ctx, SSL *ssl) {
    if (ssl) {
        SSL_shutdown(ssl);
        SSL_free(ssl);
    }
    if (ctx) {
        SSL_CTX_free(ctx);
    }
}

// Helper: SSL_read that handles WANT_READ and WANT_WRITE.
static ssize_t ssl_recv(SSL *ssl, char *buf, size_t len) {
    int n = SSL_read(ssl, buf, static_cast<int>(len));  
    if (n > 0) return n;

    int err = SSL_get_error(ssl, n);
    if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
        errno = EAGAIN;
        return -1;
    }
    if (err == SSL_ERROR_ZERO_RETURN) return 0;
    errno = EIO;
    return -1;
}

// TLS version of read_response_headers - reads headers over SSL connection.
HttpResponse read_response_headers_tls(SSL *ssl, int sock, int timeout_ms) {
    HttpResponse response;
    string buffer;
    char temp[KB4];

    while (true) {
        size_t header_end = find_header_end(buffer);
        if (header_end != string::npos) {
            string headers_text = buffer.substr(0, header_end);
            response.body_prefix = buffer.substr(header_end);
            parse_headers_block(headers_text, response);
            return response;
        }

        if (SSL_pending(ssl) == 0) {
            poll_socket_or_quit(sock, timeout_ms);
        }

        ssize_t received = ssl_recv(ssl, temp, sizeof(temp));
        if (received < 0) {
            if (errno == EINTR || errno == EAGAIN) continue;
            throw runtime_error(string("SSL_read failed: ") + strerror(errno));
        }
        if (received == 0) {
            throw runtime_error("connection closed before end of headers");
        }
        buffer.append(temp, static_cast<size_t>(received));
    }
}

// SSL_read that handles non-blocking behavior for streaming.
static ssize_t ssl_recv_stream(SSL *ssl, char *buf, size_t len) {
    int n = SSL_read(ssl, buf, static_cast<int>(len));
    if (n > 0) return n;

    int err = SSL_get_error(ssl, n);
    if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
        errno = EAGAIN;
        return -1;
    }
    if (err == SSL_ERROR_ZERO_RETURN) return 0;
    errno = EIO;
    return -1;
}

// TLS version of recv_exact - reads exactly len bytes over SSL.
static void recv_exact_poll_tls(SSL *ssl, int sock, char *buffer, size_t len, int timeout_ms) {
    size_t received_total = 0;

    while (received_total < len) {
        if (SSL_pending(ssl) == 0) {
            poll_socket_or_quit(sock, timeout_ms);
        }

        ssize_t received = ssl_recv_stream(ssl, buffer + received_total, len - received_total);

        if (received < 0) {
            if (errno == EINTR || errno == EAGAIN) continue;
            throw runtime_error(string("SSL_read failed: ") + strerror(errno));
        }

        if (received == 0) {
            throw runtime_error("connection closed while reading exact amount of data");
        }

        received_total += static_cast<size_t>(received);
    }
}

// TLS version of stream_audio_plain - streams audio over SSL connection.
void stream_audio_plain_tls(SSL *ssl, int sock, const string &body_prefix, int timeout_ms) {
    if (!body_prefix.empty()) {
        write_all_stdout(body_prefix.data(), body_prefix.size());
    }

    char buffer[KB8];

    while (true) {
        if (SSL_pending(ssl) == 0) {
            poll_socket_or_quit(sock, timeout_ms);
        }

        ssize_t received = ssl_recv_stream(ssl, buffer, sizeof(buffer));

        if (received < 0) {
            if (errno == EINTR || errno == EAGAIN) continue;
            throw runtime_error(string("SSL_read while streaming failed: ") + strerror(errno));
        }

        if (received == 0) {
            return;
        }

        write_all_stdout(buffer, static_cast<size_t>(received));
    }
}

// ICY metadata version of stream_audio using TLS connection.
void stream_audio_with_metadata_tls(SSL *ssl, int sock, const string &body_prefix, size_t metaint, int timeout_ms) {
    string pending = body_prefix;

    while (true) {
        size_t audio_needed = metaint;

        while (audio_needed > 0) {
            if (!pending.empty()) {
                size_t chunk = min(audio_needed, pending.size());
                write_all_stdout(pending.data(), chunk);
                pending.erase(0, chunk);
                audio_needed -= chunk;
                continue;
            }

            if (SSL_pending(ssl) == 0) {
                poll_socket_or_quit(sock, timeout_ms);
            }

            char buffer[KB8];
            ssize_t received = ssl_recv_stream(ssl, buffer, sizeof(buffer));

            if (received < 0) {
                if (errno == EINTR || errno == EAGAIN) continue;
                throw runtime_error(string("SSL_read while reading audio block failed: ") + strerror(errno));
            }

            if (received == 0) {
                return;
            }

            pending.append(buffer, static_cast<size_t>(received));
        }

        unsigned char meta_len_byte = 0;

        if (!pending.empty()) {
            meta_len_byte = static_cast<unsigned char>(pending[0]);
            pending.erase(0, 1);
        } else {
            recv_exact_poll_tls(ssl, sock, reinterpret_cast<char *>(&meta_len_byte), 1, timeout_ms);
        }

        size_t metadata_len = static_cast<size_t>(meta_len_byte) * 16;

        if (metadata_len == 0) {
            continue;
        }

        string metadata;
        metadata.reserve(metadata_len);

        if (!pending.empty()) {
            size_t chunk = min(metadata_len, pending.size());
            metadata.append(pending.data(), chunk);
            pending.erase(0, chunk);
        }

        while (metadata.size() < metadata_len) {
            char buffer[KB4];
            size_t still_needed = metadata_len - metadata.size();
            size_t want = min(still_needed, static_cast<size_t>(sizeof(buffer)));

            recv_exact_poll_tls(ssl, sock, buffer, want, timeout_ms);
            metadata.append(buffer, want);
        }

        write_all_stderr(metadata.data(), metadata.size());
    }
}