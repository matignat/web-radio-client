#ifndef COMMON_H
#define COMMON_H

#include <string>
#include <stdexcept>
#include <cstdlib>

using namespace std;

typedef struct {
    string url = "";
    bool m = false;
    int timeout = 5000;
    bool ip4 = false;
    bool ip6 = false;
    int verbosity = -1;
} Options;

#endif // COMMON_H
