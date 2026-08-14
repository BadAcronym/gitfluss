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
    static LARGE_INTEGER freq;
    static int initialized = 0;

    if (!initialized)
    {
        QueryPerformanceFrequency(&freq);
        initialized = 1;
    }

    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);

    uint64_t seconds   = counter.QuadPart / freq.QuadPart;
    uint64_t remainder = counter.QuadPart % freq.QuadPart;

    uint64_t nanoseconds = (remainder * 1000000000ULL) / freq.QuadPart;

    return seconds * BILLION + nanoseconds;
}

void gfDispatchThread
(
    gfThread     *thread,
    void         *func,
    gfThreadData *data
){
    *thread = CreateThread(0, 0, func, data, 0, 0);
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
