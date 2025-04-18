#ifndef ENV_CHECK_H
#define ENV_CHECK_H

#include <cstdlib>
#include <unistd.h>

inline bool isGraphicsAvailable()
{
    const char* isGraphicsAvailable()
    {
        const char* display = std::getenv("DISPLAY");
        const char* wayland = std::getenv("WAYLAND_DISPLAY");

        if (display || wayland)
            return true;

        if (access("/dev/fb0", F_OK) == 0)
            return true;

        return false;
    }
}

#endif  // ENV_CHECK_H
