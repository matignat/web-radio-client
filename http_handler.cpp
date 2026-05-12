/*
 * HTTP protocol utilities for the radio client.
 *
 * Provides helpers for parsing response headers, detecting redirects,
 * extracting cookies and ICY metadata, building HTTP requests, and
 * handling socket polling with timeout and quit support.
 */

#include "common.h"

// Finds the end position of HTTP headers in the given data string.
size_t find_header_end(const string &data) {
    size_t pos = data.find("\r\n\r\n");
    if (pos != string::npos) {
        return pos + 4;
    }

    pos = data.find("\n\n");
    if (pos != string::npos) {
        return pos + 2;
    }

    return string::npos;
}

// Delete leading and trailing whitespace from a string.
string trim(const string &s) {
    size_t begin = 0;
    while (begin < s.size() && isspace(static_cast<unsigned char>(s[begin]))) {
        ++begin;
    }

    size_t end = s.size();
    while (end > begin && isspace(static_cast<unsigned char>(s[end - 1]))) {
        --end;
    }

    return s.substr(begin, end - begin);
}


// Convert a string to lowercase.
string to_lower(const string &s) {
    string result = s;
    for (char &c : result) {
        c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    }
    return result;
}


// Parses the first line of the HTTP response (status line)
static void parse_status_line(const string &line, HttpResponse &response) {
    istringstream iss(line);

    if (!(iss >> response.protocol >> response.status_code)) {
        throw runtime_error("invalid status line");
    }

    string rest;
    getline(iss, rest);
    response.status_text = trim(rest);

    if (response.protocol != "HTTP/1.0" &&
        response.protocol != "HTTP/1.1" &&
        response.protocol != "ICY") {
        throw runtime_error("unsupported protocol in response: " + response.protocol);
    }
}

// Parses HTTP headers and maps them in the HttpResponse struct.
void parse_headers_block(const string &headers_text, HttpResponse &response) {
    istringstream stream(headers_text);
    string line;

    // Status line must exist.
    if (!getline(stream, line)) {
        throw runtime_error("empty response");
    }

    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    parse_status_line(line, response);

    // Read headers until an empty line is found.
    while (getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (line.empty()) {
            break;
        }

        size_t colon = line.find(':');
        if (colon == string::npos) {
            continue;
        }

        string name = to_lower(trim(line.substr(0, colon)));
        string value = trim(line.substr(colon + 1));
        response.headers.push_back({name, value});
    }
}

// Waits for either data to be ready on the socket or "quit" typed on stdin.
void poll_socket_or_quit(int sock, int timeout_ms) {
    pollfd fds[2];

    fds[0].fd = sock;
    fds[0].events = POLLIN;
    fds[0].revents = 0;

    fds[1].fd = STDIN_FILENO;
    fds[1].events = POLLIN;
    fds[1].revents = 0;

    int ready = poll(fds, 2, timeout_ms);

    if (ready < 0) {
        if (errno == EINTR) {
            return;
        }
        throw runtime_error(string("poll() failed: ") + strerror(errno));
    }

    if (ready == 0) {
        throw TimeoutException("data receiving timeout");
    }

    // Check for 'quit' on stdin.
    if (fds[1].revents & POLLIN) {
        string line;
        if (getline(cin, line) && line == "quit") {
            throw QuitException("quit requested");
        }
    }

    // Here the socket is ready for reading - return to caller to do recv().
}

// Reads HTTP resonse headers from the socket, with timeout and quit support.
HttpResponse read_response_headers(int sock, int timeout_ms) {
    HttpResponse response;
    string buffer;
    char temp[KB4];

    while (true) {
        size_t header_end = find_header_end(buffer);
        if (header_end != string::npos) {
            string headers_text = buffer.substr(0, header_end);

            // Body prefix is the part of the actual response.
            response.body_prefix = buffer.substr(header_end);

            // Parse headers and fill the HttpResponse struct.
            parse_headers_block(headers_text, response);
            return response;
        }

        // Wait for next chunk of data, quit request, or timeout.
        poll_socket_or_quit(sock, timeout_ms);

        ssize_t received = recv(sock, temp, sizeof(temp), 0);
        if (received < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw runtime_error(string("recv failed while reading headers: ") + strerror(errno));
        }

        if (received == 0) {
            throw runtime_error("connection closed before end of headers");
        }

        buffer.append(temp, static_cast<size_t>(received));
    }
}

// Retrieves the value of a header from the HttpResponse, case-insensitively.
string get_header(const HttpResponse &response, const string &name) {
    string wanted = to_lower(name);

    for (const auto &entry : response.headers) {
        if (entry.first == wanted) {
            return entry.second;
        }
    }

    return "";
}

// Checks if status_code is a redirect code.
bool is_redirect(int status_code) {
    return status_code == 301 ||
           status_code == 302 ||
           status_code == 303 ||
           status_code == 307 ||
           status_code == 308;
}

size_t get_ICY_metadata(const HttpResponse &response)
{
    string metaint_value = get_header(response, "icy-metaint");

    if (metaint_value.empty())
    {
        throw runtime_error("server did not send icy-metaint despite metadata request");
    }

    size_t pos = 0;
    int parsed = 0;

    try
    {
        parsed = stoi(metaint_value, &pos);
    }
    catch (...)
    {
        throw runtime_error("invalid icy-metaint value");
    }

    if (pos != metaint_value.size() || parsed <= 0)
    {
        throw runtime_error("invalid icy-metaint value");
    }

    return static_cast<size_t>(parsed);
}


// Extracts the first cookie pair from a Set-Cookie header value.
string extract_cookie_pair(const string &set_cookie_header) {
    size_t pos = set_cookie_header.find(';');
    if (pos == string::npos) {
        return trim(set_cookie_header);
    }
    return trim(set_cookie_header.substr(0, pos));
}

string build_request(const ParsedUrl &url, const Options &opt, const string &cookie) {
    ostringstream request;

    request << "GET " << url.target << " HTTP/1.1\r\n";
    request << "Host: " << url.host << "\r\n";
    request << "Connection: Keep-Alive\r\n";

    if (!cookie.empty()) {
        request << "Cookie: " << cookie << "\r\n";
    }

    if (opt.m) {
        request << "Icy-MetaData: 1\r\n";
    }

    request << "\r\n";
    return request.str();
}

