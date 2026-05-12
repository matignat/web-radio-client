#ifndef COMMON_H
#define COMMON_H

#include <atomic>
#include <cctype>
#include <cerrno>
#include <cstring>
#include <exception>
#include <iostream>
#include <vector>
#include <sstream>
#include <stdexcept>
#include <string>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <algorithm>

using namespace std;

#define HTTP_PORT "80"
#define HTTPS_PORT "443"
#define DEFAULT_TIMEOUT_MS 5000
#define KB8 8192
#define KB4 4096
#define BITS_256 256
#define TIME_FORMAT "%Y.%m.%d %H.%M.%S"
#define TIME_BUFF_SIZE 20
#define MIN_TIMEOUT_MS 100
#define MAX_TIMEOUT_MS 100000
#define MIN_VERBOSITY 0
#define MAX_VERBOSITY 4
#define MAX_PORT_NUMBER 65535
#define MIN_PORT_NUMBER 1

struct Options {
    string url = "";
    bool m = false;
    int timeout = DEFAULT_TIMEOUT_MS;
    bool ip4 = false;
    bool ip6 = false;
    int verbosity = 2;
};

struct ParsedUrl {
    string host;
    string port;
    string target;
    bool use_tls = false;
};

struct HttpResponse {
    string protocol;
    int status_code = 0;
    string status_text;
    vector<pair<string, string>> headers;
    string body_prefix;
};

// Custom exception for timeouts and quit requests.
class TimeoutException : public runtime_error {
public:
    explicit TimeoutException(const string &msg) : runtime_error(msg) {}
};

class QuitException : public runtime_error {
public:
    explicit QuitException(const string &msg) : runtime_error(msg) {}
};

extern atomic<bool> quit_requested;

// arg_parser.cpp
Options parse_args(int argc, char *argv[]);
ParsedUrl parse_url(const string &url);

// http_handler.cpp
void poll_socket_or_quit(int sock, int timeout_ms);

// http_handler.cpp
void write_all_stdout(const char *data, size_t len);
void write_all_stderr(const char *data, size_t len);

// TCP
void stream_audio_plain(int sock, const string &body_prefix, int timeout_ms);
void stream_audio_with_metadata(int sock, const string &body_prefix, size_t metaint, int timeout_ms);

size_t find_header_end(const string &data);

#endif // COMMON_H