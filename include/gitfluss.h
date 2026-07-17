#pragma once

#include "string_view.h"

#ifdef BUILD_LINUX
    #define __USE_POSIX199309
    #include <time.h>
#endif

int64_t gfQueryTime
(
    void
);
