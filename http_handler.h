#include "common.h"

#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

string trim(const string &s);
string to_lower(const string &s);
bool is_redirect(int status_code);

string get_header(const HttpResponse &response, const string &name);
void parse_headers_block(const string &headers_text, HttpResponse &response);
string extract_cookie_pair(const string &set_cookie_header);
HttpResponse read_response_headers(int sock, int timeout_ms);
HttpResponse read_response_headers_tls(SSL *ssl, int sock, int timeout_ms);
size_t get_ICY_metadata(const HttpResponse &response);
string build_request(const ParsedUrl &url, const Options &opt, const string &cookie);

void tls_handshake(int sock, const string &host, SSL_CTX **out_ctx, SSL **out_ssl);
void tls_teardown(SSL_CTX *ctx, SSL *ssl);