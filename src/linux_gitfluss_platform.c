#include "gitfluss.h"

#define __USE_POSIX199309
#include <time.h>
#include <pthread.h>

int64_t gfQueryTime
(
    void
){
    struct timespec spec;
    clock_gettime(CLOCK_REALTIME, &spec);

    return spec.tv_sec;
}

uint64_t gfQueryMonotonic
(
    void
){
    struct timespec spec;
    clock_gettime(CLOCK_MONOTONIC, &spec);

    return (uint64_t)spec.tv_sec * BILLION + (uint64_t)spec.tv_nsec;
}

void gfDispatchThread
(
){

}
