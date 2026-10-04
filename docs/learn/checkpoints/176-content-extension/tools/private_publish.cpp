#include "settings/private_file.h"
#include "platform/utf8_arguments.h"

#include <cstdio>
#include <string>
#include <vector>

namespace {

int guarded_run(int argc, char** argv) {
    try {
        if (argc != 3) {
            std::fputs("usage: private_publish <UTF8-destination> <write-old|write-new>\n", stderr);
            return 64;
        }
        const std::string destination = argv[1];
        const std::string mode = argv[2];
        // Synthetic exercise records, never real account credentials.
        std::string contents;
        if (mode == "write-old") {
            contents = std::string(4096, 'A') + '\n';
        } else if (mode == "write-new") {
            contents = std::string(4096, 'B') + '\n';
        } else {
            std::fputs("usage: private_publish <UTF8-destination> <write-old|write-new>\n", stderr);
            return 64;
        }
        if (study_files::write_private_file(destination, contents)) {
            std::fputs("write-confirmed\n", stdout);
            return 0;
        }
        std::fputs("write-not-confirmed\n", stdout);
        return 2;
    } catch (...) {
        std::fputs("write-exception\n", stderr);
        return 3;
    }
}

}  // namespace

#if defined(_WIN32)

int wmain(int argc, wchar_t** wargv) {
    try {
        auto utf8 = platform::utf8_arguments(argc, wargv);
        std::vector<char*> argv;
        argv.reserve(utf8.size() + 1);
        for (auto& arg : utf8) argv.push_back(arg.data());
        argv.push_back(nullptr);
        return guarded_run(argc, argv.data());
    } catch (...) {
        std::fputs("command-line-initialization-failed\n", stderr);
        return 3;
    }
}

#else

int main(int argc, char** argv) {
    return guarded_run(argc, argv);
}

#endif
