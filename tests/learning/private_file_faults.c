// Linux-only test interposer, injected into the synthetic publication probe.
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int mode_is(const char *name) {
    const char *mode = getenv("STUDY_FILE_FAULT");
    return mode && strcmp(mode, name) == 0;
}

ssize_t write(int fd, const void *bytes, size_t length) {
    static ssize_t (*real_write)(int, const void *, size_t);
    static int interrupted, short_written;
    if (!real_write) real_write = dlsym(RTLD_NEXT, "write");
    struct stat info;
    if (fstat(fd, &info) == 0 && S_ISREG(info.st_mode)) {
        if (mode_is("interrupted") && !interrupted++) { errno = EINTR; return -1; }
        if (mode_is("short") && length > 17) length = 17;
        if (mode_is("short-failure")) {
            if (short_written++) { errno = ENOSPC; return -1; }
            if (length > 17) length = 17;
        }
    }
    return real_write(fd, bytes, length);
}

int fsync(int fd) {
    static int (*real_sync)(int);
    if (!real_sync) real_sync = dlsym(RTLD_NEXT, "fsync");
    struct stat info;
    if (fstat(fd, &info) == 0 &&
        ((mode_is("file-sync") && S_ISREG(info.st_mode)) ||
         (mode_is("directory-sync") && S_ISDIR(info.st_mode)))) {
        errno = EIO; return -1;
    }
    return real_sync(fd);
}

int rename(const char *from, const char *to) {
    static int (*real_rename)(const char *, const char *);
    if (!real_rename) real_rename = dlsym(RTLD_NEXT, "rename");
    if (mode_is("rename")) { errno = EACCES; return -1; }
    return real_rename(from, to);
}
