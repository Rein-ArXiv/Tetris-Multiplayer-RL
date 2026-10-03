#include "platform/platform.h"
#include <cassert>

int main(int argc, char**)
{
    const bool expect_failure = argc > 1;
    platform_shutdown();
    assert(platform_should_close());
    for (int attempt = 0; attempt < 2; ++attempt) {
        const bool ready = platform_init(64, 64, "lifetime probe");
        assert(ready != expect_failure);
        if (ready) {
            assert(!platform_should_close());
            // Rejected second init must not tear down the existing owner.
            assert(!platform_init(64, 64, "duplicate"));
            assert(!platform_should_close());
            platform_begin_frame();
        }
        platform_shutdown();
        platform_shutdown();
        assert(platform_should_close());
        const FrameInfo inactive = platform_begin_frame();
        assert(inactive.dt == 0.0 && inactive.events == 0);
        platform_end_frame();
    }
}
