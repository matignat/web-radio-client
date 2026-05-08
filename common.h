#ifndef COMMON_H
#define COMMON_H

#include <string>
#include <stdexcept>
#include <cctype>
#include <iostream>
#include <sstream>

using namespace std;

typedef struct {
    string url = "";
    bool m = false;
    int timeout = 5000;
    bool ip4 = false;
    bool ip6 = false;
    int verbosity = -1;
} Options;

typedef struct  {
    string host;
    string port;
    string target;
    bool use_tls;  
} ParsedUrl;

Options parse_args(int argc, char *argv[]);
ParsedUrl parse_url(const std::string &url);

string build_request(const ParsedUrl &url, const Options &opt);


#endif // COMMON_H
