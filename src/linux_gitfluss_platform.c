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

    if(path_sv.size > 1 && path[0] == '~')
    {
        const char *home = getenv("HOME");
        if(!home)
        {
            return 0;
        }

        StringView home_sv = cstr_sv(home);

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

    return buf;
}
