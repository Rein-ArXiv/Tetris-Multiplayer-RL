#ifndef STUDY_NET_EPOLL_REACTOR_H
#define STUDY_NET_EPOLL_REACTOR_H
#include "net/study_reactor.h"
#if defined(__linux__)
namespace study_net {
// Same ownership/identity contract as StudyReactor. Linux epoll + eventfd, LT.
std::unique_ptr<StudyReactor> make_epoll_reactor();
}
#endif
#endif
