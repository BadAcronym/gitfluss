#include "gitfluss.h"

#include <time.h>

#include <process.h>

i64 gfQueryTime
(
    void
){
    return time(0);
}

u64 gfQueryMonotonic
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

    u64 seconds   = counter.QuadPart / freq.QuadPart;
    u64 remainder = counter.QuadPart % freq.QuadPart;

    u64 nanoseconds = (remainder * BILLION) / freq.QuadPart;

    return seconds * BILLION + nanoseconds;
}

i64 gfQueryTimezoneOffset
(
    void
){
    TIME_ZONE_INFORMATION zoneInfo = {0};

    DWORD daylight = GetTimeZoneInformation(&zoneInfo);
    i64   result   = zoneInfo.Bias * 60;

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
    gfThread         *thread,
    void             *func,
    gfRepoThreadData *data
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
