/*
 * Author: Mateusz Gnat
 * Project: Internet radio client (TCP, IPv4/IPv6, TLS)
 *
 * This file contains the main client logic responsible for connecting to an
 * HTTP/HTTPS server, performing TCP socket setup, and handling both IPv4 and
 * IPv6 connections. It builds and sends HTTP requests, negotiates TLS sessions
 * when required, and processes HTTP responses including redirects and cookies.
 *
 * The client streams the received audio data directly to standard output and,
 * optionally, processes ICY metadata multiplexed with the audio stream. It also
 * logs diagnostic information depending on the selected verbosity level and
 * safely retries the connection on timeouts.
 */

#include "common.h"
#include "http_handler.h"
#include "tls_handler.h"
#include <netdb.h>
#include <set>
#include <sys/time.h>

// Helper function for error logging based on verbosity level.
static void log_msg(const Options &opt, int level, const string &msg)
{
    if (opt.verbosity >= level)
    {
        cerr << msg << '\n';
    }
}

// Converts sockaddr to a human-readable string - used for logging.
static string sockaddr_to_string(const sockaddr *addr, socklen_t addrlen)
{
    char host[NI_MAXHOST];
    char service[NI_MAXSERV];

    int gni = getnameinfo(
        addr,
        addrlen,
        host,
        sizeof(host),
        service,
        sizeof(service),
        NI_NUMERICHOST | NI_NUMERICSERV);

    if (gni != 0)
    {
        return "";
    }

    if (addr->sa_family == AF_INET6)
    {
        return "[" + string(host) + "]:" + service;
    }

    return string(host) + ":" + service;
}

// Converts ParsedUrl back to string form, used for loop detection in redirects.
static string parsed_url_to_string(const ParsedUrl &url)
{
    string result = url.use_tls ? "https://" : "http://";

    if (url.host.find(':') != string::npos)
    {
        result += "[" + url.host + "]";
    }
    else
    {
        result += url.host;
    }

    bool is_default_http = (!url.use_tls && url.port == HTTP_PORT);
    bool is_default_https = (url.use_tls && url.port == HTTPS_PORT);

    // If port is non-deafult, include it in the string.
    if (!is_default_http && !is_default_https)
    {
        result += ":" + url.port;
    }

    result += url.target;
    return result;
}

static string make_timestamp()
{
    time_t now = time(nullptr);
    tm local_tm{};
    localtime_r(&now, &local_tm);

    char buf[TIME_BUFF_SIZE];
    strftime(buf, sizeof(buf), TIME_FORMAT, &local_tm);
    return string(buf);
}

// Returns socket descriptor or throws runtime_error on failure.
static int connect_to_server(const ParsedUrl &url, const Options &opt)
{
    addrinfo hints{};
    addrinfo *result = nullptr;

    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    if (opt.ip4 && !opt.ip6)
    {
        hints.ai_family = AF_INET;
    }
    else if (opt.ip6 && !opt.ip4)
    {
        hints.ai_family = AF_INET6;
    }
    else
    {
        hints.ai_family = AF_UNSPEC;
    }

    // Server communication log - level 1
    log_msg(opt, 1, make_timestamp());
    log_msg(opt, 1, "resolving name " + url.host);

    int gai = getaddrinfo(url.host.c_str(), url.port.c_str(), &hints, &result);
    if (gai != 0)
    {
        throw runtime_error(string("getaddrinfo failed: ") + gai_strerror(gai));
    }

    int sock = -1;

    for (addrinfo *rp = result; rp != nullptr; rp = rp->ai_next)
    {
        // Server communication log - level 1
        log_msg(opt, 1, "connecting to server " + sockaddr_to_string(rp->ai_addr, rp->ai_addrlen));

        sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sock < 0)
        {
            continue;
        }

        if (connect(sock, rp->ai_addr, rp->ai_addrlen) == 0)
        {
            freeaddrinfo(result);
            return sock;
        }

        close(sock);
        sock = -1;
    }

    freeaddrinfo(result);
    throw runtime_error("could not connect to any resolved address");
}

// Send all data as the string.
static void send_all(int sock, const string &data)
{
    size_t sent_total = 0;

    while (sent_total < data.size())
    {
        ssize_t sent = send(sock, data.data() + sent_total, data.size() - sent_total, 0);

        if (sent < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            throw runtime_error(string("send() failed: ") + strerror(errno));
        }

        if (sent == 0)
        {
            throw runtime_error("send() returned 0");
        }

        sent_total += static_cast<size_t>(sent);
    }
}

// Wraps SSL_write to handle non-blocking behavior and errors.
static ssize_t ssl_send(SSL *ssl, const char *buf, size_t len) {
    int n = SSL_write(ssl, buf, static_cast<int>(len));
    if (n > 0) return n;
    int err = SSL_get_error(ssl, n);
    // For WANT_READ/WRITE we set errno to EAGAIN to allow retrying the operation
    if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
        errno = EAGAIN;
        return -1;
    }
    errno = EIO;
    return -1;
}

// Send all in loop using SSL.
static void send_all_tls(SSL *ssl, const string &data) {
    size_t sent_total = 0;
    while (sent_total < data.size()) {
        ssize_t sent = ssl_send(ssl, data.data() + sent_total, data.size() - sent_total);
        if (sent < 0) {
            if (errno == EINTR || errno == EAGAIN) continue;
            // For other errors, such as EIO.
            throw runtime_error(string("SSL_write failed: ") + strerror(errno));
        }
        sent_total += static_cast<size_t>(sent);
    }
}

static void print_response(const Options &opt, const HttpResponse &response)
{
    if (opt.verbosity < 1)
    {
        return;
    }

    cerr << response.protocol << " "
         << response.status_code << " "
         << response.status_text << "\n";

    for (const auto &entry : response.headers)
    {
        cerr << entry.first << ": " << entry.second << "\n";
    }

    cerr << "\n";
}

static ParsedUrl resolve_redirect_url(const ParsedUrl &current, const string &location)
{
    if (location.empty())
    {
        throw runtime_error("redirect without Location header");
    }

    if (location.rfind("http://", 0) == 0 || location.rfind("https://", 0) == 0)
    {
        return parse_url(location);
    }

    if (location[0] == '/')
    {
        ParsedUrl next = current;
        next.target = location;
        return next;
    }

    throw runtime_error("unsupported redirect Location format");
}

static bool run_single_attempt(
    const Options &opt,
    ParsedUrl &current_url,
    string &cookie,
    set<string> &visited_urls)
{
    int sock = -1;
    SSL_CTX *ctx = nullptr;
    SSL *ssl = nullptr;

    try {
        sock = connect_to_server(current_url, opt);

        if (current_url.use_tls) {
            tls_handshake(sock, current_url.host, &ctx, &ssl);
        }

        string request = build_request(current_url, opt, cookie);
        if (opt.verbosity >= 1) cerr << request;

        // Read response headers
        HttpResponse response;
        if (current_url.use_tls) {
            send_all_tls(ssl, request);
            response = read_response_headers_tls(ssl, sock, opt.timeout);
        } else {
            send_all(sock, request);
            response = read_response_headers(sock, opt.timeout);
        }

        print_response(opt, response);

        // Get cookie from response if present, to use in redirects.
        string set_cookie = get_header(response, "set-cookie");
        if (!set_cookie.empty()) {
            cookie = extract_cookie_pair(set_cookie);
        }

        if (is_redirect(response.status_code)) {
            ParsedUrl next_url = resolve_redirect_url(current_url, get_header(response, "location"));
            string next_text = parsed_url_to_string(next_url);

            if (visited_urls.count(next_text) > 0) {
                throw runtime_error("redirect loop detected for URL: " + next_text);
            }
            visited_urls.insert(next_text);

            // Clean up before next attempt.
            tls_teardown(ctx, ssl);
            close(sock);

            current_url = next_url;
            return true;
        }

        if (response.status_code != 200) {
            throw runtime_error("server returned unexpected status code " +
                                to_string(response.status_code));
        }

        if (opt.m) {
            size_t metaint = get_ICY_metadata(response);
            if (current_url.use_tls) {
                stream_audio_with_metadata_tls(ssl, sock, response.body_prefix, metaint, opt.timeout);
            } else {
                stream_audio_with_metadata(sock, response.body_prefix, metaint, opt.timeout);
            }
        } else {
            if (current_url.use_tls) {
                stream_audio_plain_tls(ssl, sock, response.body_prefix, opt.timeout);
            } else {
                stream_audio_plain(sock, response.body_prefix, opt.timeout);
            }
        }

        tls_teardown(ctx, ssl);
        close(sock);

        // End of streaming - normal exit.
        return false;
    }
    catch (...) {
        tls_teardown(ctx, ssl);
        if (sock >= 0) close(sock);
        throw;
    }
}

// Main client logic: handle redirects and retries on timeout.
static void run_client(const Options &opt)
{
    ParsedUrl start_url = parse_url(opt.url);

    while (true) {
        ParsedUrl current_url = start_url;
        string cookie;
        set<string> visited_urls;
        visited_urls.insert(parsed_url_to_string(current_url));

        try {

            while (run_single_attempt(opt, current_url, cookie, visited_urls)) {
                // Loop to handle redirects.
            }
            return;
        }
        catch (const QuitException &) {
            // Quit requested - exit gracefully.
            return;
        }
        catch (const TimeoutException &) {
            log_msg(opt, 1, "\ndata receiving timeout");
            continue;
        }
    }
}

int main(int argc, char *argv[])
{
    Options opt;

    try
    {
        opt = parse_args(argc, argv);
    }
    catch (const exception &e)
    {
        cerr << "Error: " << e.what() << '\n';
        return 1;
    }

    try
    {
        run_client(opt);
        return 0;
    }
    catch (const exception &e)
    {
        // Crital errors log - level 2
        if (opt.verbosity >= 2)
        {
            cerr << "Error: " << e.what() << '\n';
        }
        return 1;
    }
}