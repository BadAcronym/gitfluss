#ifndef GF_HEADER
#define GF_HEADER

#include "string_view.h"

#include <stdint.h>

#ifdef BUILD_LINUX
    #define CONF_PATH     ".gitflussconf"
    #define CONF_FALLBACK "~/.config/gitfluss/.conf"

    #define __USE_POSIX199309
    #define __USE_POSIX
    #define __USE_MISC
    #include <time.h>
    #include <pthread.h>
    typedef pthread_t gfThread;
#endif
#ifdef BUILD_WINDOWS
    #include <fcntl.h>
    #include <io.h>
    #include <windows.h>

    #ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
    #define ENABLE_VIRTUAL_TERMINAL_PROCESSING  0x0004
    #endif

    #define CONF_PATH     "gitfluss.ini"
    #define CONF_FALLBACK "~\\.config\\gitfluss\\gitfluss.ini"

    typedef HANDLE gfThread;
#endif

#define f_internal static
#define BILLION    1000000000L

#define ANSI_END "\033[0m"

#define bufsize  8192
#define MAX_DAYS 32768
#define MAX_PATH 4096

#define RED    0
#define GREEN  1
#define BLUE   2
#define CYAN   3
#define PURPLE 4
#define PINK   5
#define YELLOW 6
#define WHITE  7

#define GF_FLAG_INFO    0x01
#define GF_FLAG_MONO    0x02
#define GF_FLAG_PROFILE 0x04
#define GF_FLAG_NOMATCH 0x08
#define GF_FLAG_SUMMARY 0x10

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
    uint16_t   startYear;
    uint16_t   endYear;
    uint8_t    percentile0;
    uint8_t    percentile1;
    uint8_t    percentile2;
    uint8_t    percentile3;
    uint8_t    colour;
    uint8_t    years;
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
    uint16_t      startYear;
    uint16_t      endYear;
    int64_t       now;
    uint8_t       currentMonth;
    uint8_t       weekday365;
    uint8_t       day_of_month;
    uint8_t       leapYear;
    uint8_t       yearsEpoch;
}
gfHeatmapSettings;

typedef struct gfDisplaySettings
{
    StringView biggestRepo;
    uint32_t   repositoryCount;
    uint64_t   totalCommitCount;
    uint64_t   personalCommitCount;
    uint32_t   commitsToday;
    uint32_t   repoMax;
    uint32_t   *heatmap;
    uint32_t   *sorted;
    int64_t    now;
    int64_t    startYearTime;
    int64_t    endYearTime;
    int64_t    currDayEnd;
    int64_t    oldestCommitTime;
    char       *biggestRepoBuf;

    #ifdef BUILD_LINUX
    pthread_mutex_t mutexSet;
    #endif

    #ifdef BUILD_WINDOWS
    PSRWLOCK lockSet;
    #endif
}
gfDisplaySettings;

typedef struct gfThreadData
{
    uint8_t           flags;
    uint32_t          id;
    uint32_t          authorcount;
    StringView        repository;
    StringView        *authorlist;
    gfDisplaySettings *set;
}
gfThreadData;

extern int64_t gfQueryTime
(
    void
);

extern uint64_t gfQueryMonotonic
(
    void
);

extern int64_t gfQueryTimezoneOffset
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

extern void gfLock
(
    gfDisplaySettings *set
);

extern void gfUnlock
(
    gfDisplaySettings *set
);

extern void gfAddAuthor
(
    gfConf     *config,
    StringView author
);

extern void gfAddAuthorlist
(
    gfConf     *config,
    StringView path
);

extern void gfAddPath
(
    gfConf     *config,
    StringView path
);

extern void gfAddPathlist
(
    gfConf     *config,
    StringView path
);

extern void gfReadConfig
(
    gfConf *config
);

extern void gfReadArgs
(
    int    argc,
    char   **argv,
    gfConf *config
);

#endif
