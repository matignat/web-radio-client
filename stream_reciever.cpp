/*
 * Low-level I/O and streaming helpers for the internet radio client.
 *
 * Provides robust wrappers for writing to stdout/stderr and reading from
 * sockets with proper handling of partial writes, EINTR and timeouts.
 * Implements plain audio streaming and ICY metadata-aware streaming over TCP.
 */

#include "common.h"

// Write exactly len bytes to stdout, handling partial writes and EINTR.
void write_all_stdout(const char *data, size_t len) {
    size_t written = 0;

    while (written < len) {
        ssize_t result = write(STDOUT_FILENO, data + written, len - written);

        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw runtime_error(string("write() to stdout failed: ") + strerror(errno));
        }

        if (result == 0) {
            throw runtime_error("write() to stdout returned 0");
        }

        written += static_cast<size_t>(result);
    }
}

// Write exactly len bytes to stderr, handling partial writes and EINTR.
void write_all_stderr(const char *data, size_t len) {
    size_t written = 0;

    while (written < len) {
        ssize_t result = write(STDERR_FILENO, data + written, len - written);

        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw runtime_error(string("write() to stderr failed: ") + strerror(errno));
        }

        if (result == 0) {
            throw runtime_error("write() to stderr returned 0");
        }

        written += static_cast<size_t>(result);
    }
}

// Recieve exactly len bytes from sock, using poll to wait for data.
static void recv_exact_poll(int sock, char *buffer, size_t len, int timeout_ms) {
    size_t received_total = 0;

    while (received_total < len) {
        poll_socket_or_quit(sock, timeout_ms);

        ssize_t received = recv(sock, buffer + received_total, len - received_total, 0);

        if (received < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw runtime_error(string("recv() failed: ") + strerror(errno));
        }

        if (received == 0) {
            throw runtime_error("connection closed while reading exact amount of data");
        }

        received_total += static_cast<size_t>(received);
    }
}

// Plain TCP streaming without metadata.
void stream_audio_plain(int sock, const string &body_prefix, int timeout_ms) {
    if (!body_prefix.empty()) {
        write_all_stdout(body_prefix.data(), body_prefix.size());
    }

    char buffer[KB8];

    while (true) {
        poll_socket_or_quit(sock, timeout_ms);

        ssize_t received = recv(sock, buffer, sizeof(buffer), 0);

        if (received < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw runtime_error(string("recv() while streaming failed: ") + strerror(errno));
        }

        if (received == 0) {
            return;
        }

        write_all_stdout(buffer, static_cast<size_t>(received));
    }
}

// Plain TCP streaming with ICY metadata.
void stream_audio_with_metadata(int sock, const string &body_prefix, size_t metaint, int timeout_ms) {
    string pending = body_prefix;

    while (true) {
        // Read exactly metaint bytes of audio.
        size_t audio_needed = metaint;

        while (audio_needed > 0) {
            if (!pending.empty()) {
                size_t chunk = min(audio_needed, pending.size());
                write_all_stdout(pending.data(), chunk);
                pending.erase(0, chunk);
                audio_needed -= chunk;
                continue;
            }

            poll_socket_or_quit(sock, timeout_ms);

            char buffer[KB8];
            ssize_t received = recv(sock, buffer, sizeof(buffer), 0);

            if (received < 0) {
                if (errno == EINTR) {
                    continue;
                }
                throw runtime_error(string("recv() while reading audio block failed: ") + strerror(errno));
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
            recv_exact_poll(sock, reinterpret_cast<char *>(&meta_len_byte), 1, timeout_ms);
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

            recv_exact_poll(sock, buffer, want, timeout_ms);
            metadata.append(buffer, want);
        }

        write_all_stderr(metadata.data(), metadata.size());
    }
}
