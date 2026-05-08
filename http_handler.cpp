#include "common.h"
#include <sstream>

string build_request(const ParsedUrl &url, const Options &opt) {
    ostringstream request;

    request << "GET " << url.target << " HTTP/1.1\r\n";
    request << "Host: " << url.host << "\r\n";
    request << "Connection: Keep-Alive\r\n";

    if (opt.m) {
        request << "Icy-MetaData: 1\r\n";
    }

    request << "\r\n";
    return request.str();
}