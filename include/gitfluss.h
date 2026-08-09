#pragma once

#include <stdint.h>

#include "string_view.h"

#ifdef BUILD_LINUX
    #define __USE_POSIX199309
    #include <time.h>
    #include <pthread.h>
    typedef pthread_t gfThread;
#endif

typedef struct gfConf
{
    StringView repositories;
    StringView authors;
    char       *sortedRepos;
    char       *sortedAuthors;
    const char *character;
    const char *mono0;
    const char *mono1;
    const char *mono2;
    const char *mono3;
    const char *mono4;
    uint8_t    percentile0;
    uint8_t    percentile1;
    uint8_t    percentile2;
    uint8_t    percentile3;
    uint8_t    colour;
    uint8_t    flags;
}
gfConf;

typedef struct gfPercentiles
{
    uint32_t d20;
    uint32_t d50;
    uint32_t d70;
    uint32_t d90;
}
gfPercentiles;

typedef struct gfHeatmapSettings
{
    gfConf        *config;
    gfPercentiles *percentiles;
    uint32_t      *heatmap;
    uint8_t       currentMonth;
    uint8_t       weekday365;
    uint8_t       day_of_month;
    uint8_t       leapYear;
}
gfHeatmapSettings;

typedef struct gfDisplaySettings
{
    StringView biggestRepo;
    uint32_t   repositoryCount;
    uint64_t   totalCommitCount;
    uint64_t   personalCommitCount;
    uint32_t   repoMax;
    uint32_t   *heatmap;
    uint32_t   *sorted;
    int64_t    now;
    int64_t    currDayEnd;
    int64_t    oldestCommitTime;
    char       *biggestRepoBuf;
}
gfDisplaySettings;

typedef struct gfThreadData
{
    uint32_t          id;
    uint32_t          authorcount;
    StringView        repository;
    StringView        authorlist;
    gfDisplaySettings set;
}
gfThreadData;

#define BILLION 1000000000L

int64_t gfQueryTime
(
    void
);

uint64_t gfQueryMonotonic
(
    void
);

extern void gfDispatchThread
(
    gfThread     *thread,
    void         *func,
    gfThreadData *data
);

extern void gfWaitThread
(
    gfThread thread
);
