#include "gitfluss.h"

#include <time.h>

#include <process.h>

int64_t gfQueryTime
(
    void
){
    return time(0);
}

uint64_t gfQueryMonotonic
(
    void
){
    // clock_gettime(CLOCK_MONOTONIC, &spec);

    // return (uint64_t)spec.tv_sec * BILLION + (uint64_t)spec.tv_nsec;
    return 0;
}

void gfDispatchThread
(
    gfThread     *thread,
    void         *func,
    gfThreadData *data
){
    uint32_t threadID;
    _beginthreadex(0, 0, func, data, 0, &threadID);
}

void gfWaitThread
(
    gfThread thread
){
    WaitForSingleObject(thread, INFINITE);
}

void gfLock
(
    gfDisplaySettings *set
){
    WaitForSingleObject(set->mutexSet, INFINITE);
}

void gfUnlock
(
    gfDisplaySettings *set
){
    ReleaseMutex(set->mutexSet);
}
