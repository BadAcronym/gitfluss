#include "gitfluss.h"

#define STRING_VIEW_IMPL
#include "string_view.h"

#include <git2.h>

#include <stdint.h>
#include <stdio.h>

#define f_internal static

#ifdef BUILD_LINUX
    #define CONF_PATH     ".conf"
    #define CONF_FALLBACK "~/.config/gitfluss/.conf"
#endif
#ifdef BUILD_WINDOWS
    #define CONF_PATH     "gitfluss.ini"
    #define CONF_FALLBACK "~\\.config\\gitfluss.ini"
#endif

#define bufsize     8192
#define PATH_MAX    4096
#define MAX_AUTHORS 1024

#define HEAT_0 " "

#define HEAT_1_RED   "\033[38;2;100;10;10m\u25FC\033[0m"
#define HEAT_2_RED   "\033[38;2;150;15;15m\u25FC\033[0m"
#define HEAT_3_RED   "\033[38;2;200;20;20m\u25FC\033[0m"
#define HEAT_4_RED   "\033[38;2;220;30;30m\u25FC\033[0m"
#define HEAT_5_RED   "\033[38;2;255;40;50m\u25FC\033[0m"

#define HEAT_1_GREEN "\033[38;2;50;100;50m\u25FC\033[0m"
#define HEAT_2_GREEN "\033[38;2;50;150;50m\u25FC\033[0m"
#define HEAT_3_GREEN "\033[38;2;50;200;50m\u25FC\033[0m"
#define HEAT_4_GREEN "\033[38;2;50;220;50m\u25FC\033[0m"
#define HEAT_5_GREEN "\033[38;2;50;255;50m\u25FC\033[0m"

f_internal void readConfig
(
    StringView *repositories,
    StringView *authors,
    uint8_t    *green
){
    char path_expanded[PATH_MAX];
    char fallback_expanded[PATH_MAX];
    gfExpandPath(CONF_PATH, path_expanded);
    gfExpandPath(CONF_FALLBACK, fallback_expanded);

    FILE *file = fopen(path_expanded, "r");
    if(!file)
    {
        file = fopen(fallback_expanded, "r");
        if(!file)
        {
            fprintf(stderr, "\033[33;3mWARNING: could not open configuration file."
                    "\033[0m\n");
            return;
        }
    }

    StringView sep = cstr_sv(";");

    char buf[bufsize];
    while(fgets(buf, bufsize, file))
    {
        StringView buffer;
        buffer.data = buf;
        buffer.size = bufsize;

        StringView  author_sv = cstr_sv("author: ");
        const char* authorloc = sv_find(author_sv, buffer);

        StringView  colour_sv = cstr_sv("colour: ");
        const char* colourloc = sv_find(colour_sv, buffer);

        if(authorloc)
        {
            StringView author = cstr_sv(buffer.data + author_sv.size);
            if(author.size)
            {
                author.size -= 1;
            }

            if(authors->data)
            {
                const char *authors_cstr = sv_concat(*authors, sep);
                free((void*)authors->data);
                *authors = cstr_sv(authors_cstr);

                authors_cstr = sv_concat(*authors, author);
                free((void*)authors->data);
                *authors = cstr_sv(authors_cstr);
            }
            else
            {
                *authors = cstr_sv_cpy(buffer.data + author_sv.size);
                if(authors->size)
                {
                    authors->size -= 1;
                }
            }

            #ifdef DEBUG
                fprintf(stderr, "detected author: "PRI_SV"\n", ARG_SV(author));
                fprintf(stderr, "author list: "PRI_SV"\n", ARG_SV(*authors));
            #endif

            continue;
        }
        else if(colourloc)
        {
            StringView colour   = cstr_sv(buffer.data + colour_sv.size);
            StringView green_sv = cstr_sv("green\n");

            if(sv_same(colour, green_sv))
            {
                *green = 1;
            }

            #ifdef DEBUG
                fprintf(stderr, "detected colour: "PRI_SV"\n", ARG_SV(colour));
            #endif

            continue;
        }

        if(repositories->data)
        {
            const char *repositories_cstr = sv_concat(*repositories, sep);
            free((void*)repositories->data);
            *repositories = cstr_sv_cpy(repositories_cstr);

            char resolved[PATH_MAX];
            gfExpandPath(buffer.data, resolved);

            StringView resolved_sv = cstr_sv(resolved);
            if(resolved_sv.size)
            {
                resolved_sv.size -= 1;
            }

            if(repositories_cstr)
            {
                free((void*)repositories_cstr);
            }
            repositories_cstr = sv_concat(*repositories, resolved_sv);
            free((void*)repositories->data);
            *repositories = cstr_sv_cpy(repositories_cstr);

            if(repositories_cstr)
            {
                free((void*)repositories_cstr);
            }
        }
        else
        {
            char resolved[PATH_MAX] = {0};
            gfExpandPath(buffer.data, resolved);
            *repositories = cstr_sv_cpy(resolved);

            if(repositories->size)
            {
                repositories->size -= 1;
            }
        }

        #ifdef DEBUG
            fprintf(stderr, "detected path: %s\n", buffer.data);
            fprintf(stderr, "path list: "PRI_SV"\n", ARG_SV(*repositories));
        #endif
    }

    const char *sorted_authors = sv_sort_by_delim(*authors, ';');
    if(authors->data)
    {
        free((void*)authors->data);
    }
    *authors = cstr_sv(sorted_authors);

    const char *sorted_repos = sv_sort_by_delim(*repositories, ';');
    if(repositories->data)
    {
        free((void*)repositories->data);
    }
    *repositories = cstr_sv(sorted_repos);

    #ifdef DEBUG
        fprintf(stderr, "\nfinal, sorted author list:\n"PRI_SV"\n", ARG_SV(*authors));
        fprintf(stderr, "\nfinal, sorted paths:\n"PRI_SV"\n", ARG_SV(*repositories));
    #endif

    fclose(file);
}

f_internal void printCorrespondingHeat
(
    uint32_t commit_count,
    uint8_t  green
){
    if(!commit_count)
    {
        printf(HEAT_0);
    }
    else if(commit_count > 9 && green)
    {
        printf(HEAT_5_GREEN);
    }
    else if(commit_count > 9)
    {
        printf(HEAT_5_RED);
    }
    else if(commit_count > 7 && green)
    {
        printf(HEAT_4_GREEN);
    }
    else if(commit_count > 7)
    {
        printf(HEAT_4_RED);
    }
    else if(commit_count > 5 && green)
    {
        printf(HEAT_3_GREEN);
    }
    else if(commit_count > 5)
    {
        printf(HEAT_3_RED);
    }
    else if(commit_count > 3 && green)
    {
        printf(HEAT_2_GREEN);
    }
    else if(commit_count > 3)
    {
        printf(HEAT_2_RED);
    }
    else if(green)
    {
        printf(HEAT_1_GREEN);
    }
    else
    {
        printf(HEAT_1_RED);
    }
}

f_internal void printHeatMap
(
    uint32_t *heatmap,
    uint8_t  currentMonth,
    uint8_t  green
){
    const char *months[12] =
    {
        " Jan ",
        " Feb ",
        " Mar ",
        " Apr ",
        " May ",
        " Jun ",
        " Jul ",
        " Aug ",
        " Sep ",
        " Oct ",
        " Nov ",
        " Dec "
    };

    const char *days[7] =
    {
        " Mon ",
        " Tue ",
        " Wed ",
        " Thu ",
        " Fri ",
        " Sat ",
        " Sun ",
    };

    printf("      ");
    for(uint8_t i = 0; i < 12; ++i)
    {
        printf("%s", months[(currentMonth + i) % 12]);
    }
    printf("\n");

    for(uint8_t i = 0; i < 7; ++i)
    {
        printf(" %s ", days[i]);
        for(uint8_t j = 0; j < 48; ++j)
        {
            uint32_t test = j + i + j * 2;

            printCorrespondingHeat(heatmap[test], green);
            if(j % 4 == 3)
            {
                printf(" ");
            }
        }
        printf("\n");
    }

    printf("\n");
}

int main
(
    void
){
    StringView repositories = {0};
    StringView authors      = {0};
    uint8_t    green        = 0;

    readConfig(&repositories, &authors, &green);
    uint32_t repository_count = sv_count_by_delim(repositories, ';');

    git_libgit2_init();

    uint32_t   heatmap[16384]   = {0};
    StringView biggestRepo      = {0};
    uint32_t   repo_max         = 0;
    int64_t    oldestCommitTime = INT64_MAX;
    int64_t    now              = gfQueryTime();

    for(uint32_t i = 0; i < repository_count; ++i)
    {
        git_repository *repo    = 0;
        git_revwalk    *revwalk = 0;
        git_oid        oid      = {0};

        StringView repository        = sv_find_by_delim(repositories, ';', i);
        uint32_t   repoCommitCount = 0;

        #ifdef DEBUG
            fprintf(stderr, "\nAnalyzing repository %u: "PRI_SV"\n", i,
                    ARG_SV(repository));
        #endif

        const char *current_repo_cstr = sv_cstr(repository);

        git_repository_open(&repo, current_repo_cstr);
        git_revwalk_new(&revwalk, repo);
        git_revwalk_push_head(revwalk);

        while(!git_revwalk_next(&oid, revwalk))
        {
            git_commit *commit = 0;
            git_commit_lookup(&commit, repo, &oid);

            const git_signature *sign = git_commit_author(commit);

            StringView author_mail = cstr_sv(sign->email);
            git_time   commit_time = sign->when;

            for(uint16_t j = 0; j < MAX_AUTHORS; ++j)
            {
                StringView author = sv_find_by_delim(authors, ';', j);
                if(sv_same(author, author_mail))
                {
                    int64_t days = (now - commit_time.time) / (24 * 3600);
                    if(commit_time.time < oldestCommitTime)
                    {
                        oldestCommitTime = commit_time.time;
                    }

                    ++heatmap[days];
                    ++repoCommitCount;
                    break;
                }
            }

            git_commit_free(commit);
        }

        if(repoCommitCount > repo_max)
        {
            if(biggestRepo.data)
            {
                free((void*)biggestRepo.data);
            }
            biggestRepo = cstr_sv_cpy(current_repo_cstr);
            repo_max    = repoCommitCount;
        }

        git_revwalk_free(revwalk);
        git_repository_free(repo);
        free((void*)current_repo_cstr);
    }

    uint32_t singleday_max = 0;
    uint16_t maxday        = 0;

    for(uint16_t i = 0; i < 365; ++i)
    {
        if(heatmap[i] > singleday_max)
        {
            singleday_max = heatmap[i];
            maxday        = i;
        }
    }

    printf("most commits in the last 365 days (%u) made %u days ago.\n", singleday_max,
           maxday);
    printf("most commits in single repository (%u) in '"PRI_SV"'.\n", repo_max,
           ARG_SV(biggestRepo));
    printf("\ncommits in the last 24h: %u ", heatmap[0]);
    printCorrespondingHeat(heatmap[0], green);
    printf("\n");

    uint32_t days_epoch  = now / (24 * 3600);
    uint32_t days_commit = (now - oldestCommitTime) / (24 * 3600);
    // printf("\ndays since epoch: %u\n", days_epoch);
    printf("days since first commit: %u\n", days_commit);

    printf("\nheatmap (last 365 days):\n\n");
    uint8_t currentMonth = 6;
    printHeatMap(heatmap, currentMonth, green);

    if(authors.data)
    {
        free((void*)authors.data);
    }
    if(repositories.data)
    {
        free((void*)repositories.data);
    }
    if(biggestRepo.data)
    {
        free((void*)biggestRepo.data);
    }
    return git_libgit2_shutdown();
}
