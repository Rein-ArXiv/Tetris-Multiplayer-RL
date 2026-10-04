#ifndef STUDY_NET_REACTOR_CHOICE_H
#define STUDY_NET_REACTOR_CHOICE_H
#include "net/epoll_reactor.h"
#include <string_view>
#include <stdexcept>
namespace study_net {
inline std::unique_ptr<StudyReactor> choose_reactor(bool epoll) {
    if (!epoll) return make_poll_reactor();
#if defined(__linux__)
    return make_epoll_reactor();
#else
    throw std::runtime_error("epoll requires Linux");
#endif
}
inline bool epoll_argument(int argc, char** argv) {
    if (argc == 1) return false;
    if (argc == 2 && std::string_view(argv[1]) == "--epoll") return true;
    throw std::invalid_argument("expected no arguments or --epoll");
}
}
#endif
