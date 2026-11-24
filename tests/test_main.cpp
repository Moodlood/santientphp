#include "common/test_framework.hpp"
#include <string>

int main(int argc, char* argv[]) {
    std::string filter;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--test" || arg == "-t" || arg == "--filter") {
            if (i + 1 < argc) filter = argv[++i];
        } else if (arg.rfind("--test=", 0) == 0) {
            filter = arg.substr(7);
        } else if (arg.rfind("--filter=", 0) == 0) {
            filter = arg.substr(9);
        } else if (arg[0] != '-') {
            filter = arg;
        }
    }
    return sph::testing::runAllTests(filter);
}

