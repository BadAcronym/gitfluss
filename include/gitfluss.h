#ifndef GF_HEADER
#define GF_HEADER

#include "pd_string_view.h"

#include <inttypes.h>

#ifdef BUILD_LINUX
    #define CONF_PATH     ".gitflussconf"
    #define CONF_FALLBACK "~/.config/gitfluss/.conf"

    #include <pthread.h>
    typedef pthread_t       gfThread;
    typedef pthread_mutex_t gfThreadLock;
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

    typedef HANDLE  gfThread;
    typedef SRWLOCK gfThreadLock;
#endif

#define f_internal static
#define s_global   static
#define BILLION    1000000000L

#define bool  _Bool
#define true  1
#define false 0

#define ANSI_END "\033[0m"

#define GF_BUFSIZE  8192
#define GF_MAX_DAYS 32768
#define GF_MAX_PATH 4096

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
#define GF_FLAG_STREAK  0x04
#define GF_FLAG_PROFILE 0x08
#define GF_FLAG_NOMATCH 0x10
#define GF_FLAG_SUMMARY 0x20

#define GF_OBJ_ID_SHA1   1
#define GF_OBJ_ID_SHA256 2

#define GF_OBJ_COMMIT    1
#define GF_OBJ_TREE      2
#define GF_OBJ_BLOB      3
#define GF_OBJ_TAG       4
#define GF_OBJ_RESERVED  5
#define GF_OBJ_OFS_DELTA 6
#define GF_OBJ_REF_DELTA 7

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
    u16        startYear;
    u16        endYear;
    u08        percentile0;
    u08        percentile1;
    u08        percentile2;
    u08        percentile3;
    u08        colour;
    u08        years;
    u08        flags;
}
gfConf;

typedef struct gfPercentiles
{
    u32 d20;
    u32 d50;
    u32 d70;
    u32 d90;
}
gfPercentiles;

typedef struct gfHeatmapSettings
{
    gfConf        *config;
    gfPercentiles *percentiles;
    u32           *heatmap;
    u16           startYear;
    u16           endYear;
    i64           now;
    u08           currentMonth;
    u08           weekday365;
    u08           day_of_month;
    u08           leapYear;
    u08           yearsEpoch;
}
gfHeatmapSettings;

typedef struct gfDisplaySettings
{
    gfHeatmapSettings *heatSet;
    StringView        biggestRepo;
    u64               totalCommitCount;
    u64               personalCommitCount;
    u32               repositoryCount;
    u32               commitsToday;
    u32               repoMax;
    u32               *sorted;
    i64               startYearTime;
    i64               endYearTime;
    i64               currDayEnd;
    i64               oldestCommitTime;
    char              *biggestRepoBuf;
    gfThreadLock      lock;
}
gfDisplaySettings;

typedef struct gfPackInfo
{
    u08 *data;
    u08 type;
    u64 size;
    u64 offset;
}
gfPackInfo;

typedef struct gfRecurseInfo
{
    StringView   repository;
    u08          *packFile;
    StringView   hash;
    gfPackInfo   **cache;
    u64          *index;
    u08          oidSize;
    u64          fileSize;
    gfPackInfo   *outInfo;
    bool         firstCall;
}
gfRecurseInfo;

typedef struct gfObjectOffset
{
    StringView hash;
    u32        offset;
}
gfObjectOffset;

typedef struct gfCommitInfo
{
    StringView hash;
    StringView summary;
    StringView *parentHashes;
    StringView authorName;
    StringView authorMail;
    StringView committerName;
    StringView committerMail;
    i64        authorTime;
    i64        committerTime;
}
gfCommitInfo;

typedef struct gfRepoThreadData
{
    u08               flags;
    u32               id;
    u32               authorcount;
    StringView        repository;
    StringView        *authorlist;
    gfDisplaySettings *set;
}
gfRepoThreadData;

typedef struct gfPackedThreadData
{
    u32          id;
    StringView   idxPath;
    gfCommitInfo **table;
    gfThreadLock *tableLocks;
    u08          oidSize;
}
gfPackedThreadData;

typedef struct gfRepository
{
    StringView   path;
    gfCommitInfo *packedCommits;
}
gfRepository;

typedef struct gfHashLocation
{
    StringView packFilePath;
    u64        index;
}
gfHashLocation;

extern u08 gfTwoCharsToByte
(
    char c1,
    char c2
);

extern i64 gfQueryTime
(
    void
);

extern u64 gfQueryMonotonic
(
    void
);

extern i64 gfQueryTimezoneOffset
(
    void
);

extern void gfDispatchThread
(
    gfThread *thread,
    void     *func,
    void     *data
);

extern void gfWaitThread
(
    gfThread thread
);

extern void gfLock
(
    gfThreadLock *lock
);

extern void gfUnlock
(
    gfThreadLock *lock
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

extern void gfInitRepository
(
    gfRepository *repo,
    gfCommitInfo *head,
    gfCommitInfo **commitTable
);

extern bool gfGetCommitInfo
(
    StringView   repository,
    StringView   hash,
    gfCommitInfo *commit,
    gfCommitInfo **table
);

extern void gfFreeCommit
(
    gfCommitInfo *commit
);

#endif
