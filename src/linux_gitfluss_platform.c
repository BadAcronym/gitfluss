#include "gitfluss.h"
#include "gf_print_macros.h"

i64 gfQueryTime
(
    void
){
    struct timespec spec;
    clock_gettime(CLOCK_REALTIME, &spec);

    return spec.tv_sec;
}

u64 gfQueryMonotonic
(
    void
){
    struct timespec spec;
    clock_gettime(CLOCK_MONOTONIC, &spec);

    return (u64)spec.tv_sec * BILLION + (u64)spec.tv_nsec;
}

i64 gfQueryTimezoneOffset
(
    void
){
    time_t    timeStruct = time(0);
    struct tm timeBroken = {0};

    localtime_r(&timeStruct, &timeBroken);

    return timeBroken.tm_gmtoff;
}

void gfDispatchThread
(
    gfThread         *thread,
    void             *func,
    gfRepoThreadData *data
){
    pthread_create(thread, 0, (void *(*)(void *))func, data);
}

void gfWaitThread
(
    gfThread thread
){
    pthread_join(thread, 0);
}

void gfLock
(
    gfThreadLock *lock
){
    GF_ASSERT(lock, "cannot lock a NULL lock.");
    pthread_mutex_lock(lock);
}

void gfUnlock
(
    gfThreadLock *lock
){
    GF_ASSERT(lock, "cannot unlock a NULL lock.");
    pthread_mutex_unlock(lock);
}
