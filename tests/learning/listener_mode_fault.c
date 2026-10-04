// Linux-only injection: refuse F_GETFL solely for listening sockets.
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <sys/socket.h>

int fcntl(int fd, int command, ...) {
    static int (*real_fcntl)(int, int, ...);
    if (!real_fcntl) real_fcntl = dlsym(RTLD_NEXT, "fcntl");
    if (command == F_GETFL) {
        int listening = 0;
        socklen_t size = sizeof(listening);
        if (getsockopt(fd, SOL_SOCKET, SO_ACCEPTCONN, &listening, &size) == 0 && listening) {
            errno = EIO;
            return -1;
        }
        return real_fcntl(fd, command);
    }
    if (command == F_GETFD || command == F_GETOWN) return real_fcntl(fd, command);
    if (command != F_SETFL && command != F_SETFD && command != F_DUPFD &&
        command != F_DUPFD_CLOEXEC) { errno = ENOSYS; return -1; }
    va_list args;
    va_start(args, command);
    // This fixture is used only by the relay's known F_SETFL/F_SETFD paths.
    int value = va_arg(args, int);
    va_end(args);
    return real_fcntl(fd, command, value);
}
