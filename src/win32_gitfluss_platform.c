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

    uint64_t nanoseconds = (remainder * BILLION) / freq.QuadPart;

    return seconds * BILLION + nanoseconds;
}

int64_t gfQueryTimezoneOffset
(
    void
){
    TIME_ZONE_INFORMATION zoneInfo = {0};

    DWORD   daylight = GetTimeZoneInformation(&zoneInfo);
    int64_t result   = zoneInfo.Bias * 60;

    if(daylight == TIME_ZONE_ID_STANDARD)
    {
        result += zoneInfo.StandardBias * 60;
    }
    else if(daylight == TIME_ZONE_ID_DAYLIGHT)
    {
        result += zoneInfo.DaylightBias * 60;
    }

    return result;
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
    AcquireSRWLockExclusive(set->lockSet);
}

void gfUnlock
(
    gfDisplaySettings *set
){
    ReleaseSRWLockExclusive(set->lockSet);
}
