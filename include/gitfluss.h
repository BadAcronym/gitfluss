#pragma once

#include "string_view.h"

#ifdef BUILD_LINUX
    #define __USE_POSIX199309
    #include <time.h>
#endif
#include <stdint.h>

#define GF_TYPE_ERROR     0
#define GF_TYPE_FILE      1
#define GF_TYPE_DIRECTORY 2
#define GF_TYPE_OTHER     3

int64_t gfQueryTime
(
    void
);

uint8_t gfVerifyPath
(
    StringView path
);

const char *gfExpandPath
(
    const char *path
);
