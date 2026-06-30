#pragma once

#include "string_view.h"

#include <stdint.h>

uint64_t gfQueryTime
(
    void
);

uint8_t gfVerifyPath
(
    StringView path
);
