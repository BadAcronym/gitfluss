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
    char path_cstr[4096];
    sv_cstr(path, path_cstr);

    if(stat(path_cstr, &pathInfo))
    {
        return GF_TYPE_ERROR;
    }

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

void gfExpandPath
(
    const char *path,
    char*      buf
){
    StringView path_sv = cstr_sv(path);
    StringView dot_sv  = cstr_sv(".");

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

        uint32_t j = 0;
        for(; j < path_sv.size && j < 4096 - i; ++j)
        {
            buf[i + j] = path[j + 1];
        }

        buf[i + j] = '\0';
    }
    else if(home && path_sv.size > 4 && sv_find(homevar, path_sv) == path_sv.data)
    {
        uint32_t i = 0;
        for(; i < home_sv.size; ++i)
        {
            buf[i] = home[i];
        }

        uint32_t j = 0;
        for(; j < path_sv.size && j < 4096 - i - homevar.size; ++j)
        {
            buf[i + j] = path[j + homevar.size];
        }

        buf[i + j] = '\0';
    }
    else if(sv_same(path_sv, dot_sv))
    {
        const char *pwd   = getenv("PWD");
        StringView pwd_sv = cstr_sv(pwd);

        uint32_t i = 0;
        for(; i < pwd_sv.size; ++i)
        {
            buf[i] = pwd[i];
        }
        buf[i] = '\0';
    }
    else
    {
        uint32_t i = 0;
        for(; i < path_sv.size && i < 4096; ++i)
        {
            buf[i] = path[i];
        }
        buf[i] = '\0';
    }
}
