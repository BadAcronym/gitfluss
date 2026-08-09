#include "gitfluss.h"

    #include <time.h>
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
    gfThread     *thread,
    void         *func,
    gfThreadData *data
){
    pthread_create(thread, 0, func, data);
}

void gfWaitThread
(
    gfThread thread
){
    pthread_join(thread, 0);
}
