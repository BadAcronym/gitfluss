#include "gitfluss.h"

#include <sys/stat.h>

int64_t gfQueryTime
(
    void
){
    struct timespec spec;
    clock_gettime(CLOCK_REALTIME, &spec);

    return spec.tv_sec;
}

uint8_t gfVerifyPath
(
    StringView path
){
    struct stat pathInfo;
    const char *path_cstr = sv_cstr(path);

    if(stat(path_cstr, &pathInfo))
    {
        free((void*)path_cstr);
        return GF_TYPE_ERROR;
    }

    free((void*)path_cstr);

    if(S_ISDIR(pathInfo.st_mode))
    {
        return GF_TYPE_DIRECTORY;
    }

    if(S_ISREG(pathInfo.st_mode))
    {
        return GF_TYPE_FILE;
    }
    return GF_TYPE_OTHER;
}

const char *gfExpandPath
(
    const char *path
){
    StringView path_sv = cstr_sv(path);

    char *buf = calloc(4096, 1);

    StringView homevar = cstr_sv("$HOME");
    const char *home   = getenv("HOME");
    StringView home_sv = cstr_sv(home);

    if(home && path_sv.size > 0 && path[0] == '~')
    {
        uint32_t i = 0;
        for(; i < home_sv.size; ++i)
        {
            buf[i] = home[i];
        }

        for(uint32_t j = 0; j < path_sv.size && j < 4096 - i; ++j)
        {
            buf[i + j] = path[j + 1];
        }
    }
    else if(home && path_sv.size > 4 && sv_find(homevar, path_sv) == path_sv.data)
    {
        uint32_t i = 0;
        for(; i < home_sv.size; ++i)
        {
            buf[i] = home[i];
        }

        for(uint32_t j = 0; j < path_sv.size && j < 4096 - i - homevar.size; ++j)
        {
            buf[i + j] = path[j + homevar.size];
        }
    }

    return buf;
}
