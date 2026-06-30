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
