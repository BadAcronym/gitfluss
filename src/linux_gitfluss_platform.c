#include "gitfluss.h"

int64_t gfQueryTime
(
    void
){
    struct timespec spec;
    clock_gettime(CLOCK_REALTIME, &spec);

    return spec.tv_sec;
}
