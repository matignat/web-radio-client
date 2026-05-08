#include "common.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

// Helper function to convert sockaddr to a human-readable string
static std::string sockaddr_to_string(const sockaddr *addr, socklen_t addrlen) {
    char host[NI_MAXHOST];
    char service[NI_MAXSERV];

    int gai = getnameinfo(
        addr,
        addrlen,
        host,
        sizeof(host),
        service,
        sizeof(service),
        NI_NUMERICHOST | NI_NUMERICSERV
    );

    if (gai != 0) {
        return "<unknown>";
    }

    if (addr->sa_family == AF_INET6) {
        return "[" + string(host) + "]:" + service;
    }

    return string(host) + ":" + service;
}

int connect_to_server(const ParsedUrl &url, const Options &opt) {
    addrinfo hints{};
    addrinfo *result = nullptr;

    // Set up hints for getaddrinfo: we want TCP stream sockets
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    // Determine address family based on options
    if (opt.ip4 && !opt.ip6) {
        hints.ai_family = AF_INET;
    } else if (opt.ip6 && !opt.ip4) {
        hints.ai_family = AF_INET6;
    } else {
        hints.ai_family = AF_UNSPEC;
    }

    // Log: resolving name
    if (opt.verbosity >= 1) {
        std::cerr << "resolving name " << url.host << "\n";
    }

    int gai = getaddrinfo(url.host.c_str(), url.port.c_str(), &hints, &result);
    if (gai != 0) {
        throw std::runtime_error(
            std::string("getaddrinfo failed: ") + gai_strerror(gai)
        );
    }

    int sock = -1;

    for (addrinfo *rp = result; rp != nullptr; rp = rp->ai_next) {
        if (opt.verbosity >= 1) {
            std::cerr << "connecting to server "
                      << sockaddr_to_string(rp->ai_addr, rp->ai_addrlen)
                      << "\n";
        }

        sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sock < 0) {
            continue;
        }

        timeval tv{};
        tv.tv_sec = opt.timeout / 1000;
        tv.tv_usec = (opt.timeout % 1000) * 1000;

        if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0 ||
            setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
            close(sock);
            sock = -1;
            continue;
        }

        if (connect(sock, rp->ai_addr, rp->ai_addrlen) == 0) {
            freeaddrinfo(result);
            return sock;
        }

        close(sock);
        sock = -1;
    }

    freeaddrinfo(result);
    throw std::runtime_error("could not connect to any resolved address");
}

int main(int argc, char* argv[]) {
    try {
        Options opt = parse_args(argc, argv);
        ParsedUrl url = parse_url(opt.url);



        string request = build_request(url, opt);
        cout << request;
        
    } catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }

    return 0;
}