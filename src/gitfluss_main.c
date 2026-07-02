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

#define RED    0
#define GREEN  1
#define BLUE   2
#define PURPLE 3
#define GRAY   4

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
    char       *sorted_repos,
    char       *sorted_authors,
    uint8_t    *colour
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
                char *authors_cstr = malloc(authors->size + author.size + 2);
                sv_concat(*authors, sep, authors_cstr);
                free((void*)authors->data);
                *authors = cstr_sv(authors_cstr);

                sv_concat(*authors, author, authors_cstr);
                *authors = cstr_sv(authors_cstr);
            }
            else
            {
                char *new_buf = malloc(bufsize);
                *authors = cstr_sv_cpy(buffer.data + author_sv.size, new_buf);
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
            StringView chosen_sv = cstr_sv(buffer.data + colour_sv.size);
            StringView green_sv  = cstr_sv("green\n");

            if(sv_same(chosen_sv, green_sv))
            {
                *colour = 1;
            }

            #ifdef DEBUG
                fprintf(stderr, "detected colour: "PRI_SV"\n", ARG_SV(chosen_sv));
            #endif

            continue;
        }

        if(repositories->data)
        {
            char *repositories_cstr = malloc(repositories->size + PATH_MAX + 2);
            sv_concat(*repositories, sep, repositories_cstr);
            free((void*)repositories->data);
            *repositories = cstr_sv(repositories_cstr);

            char resolved[PATH_MAX];
            gfExpandPath(buffer.data, resolved);

            StringView resolved_sv = cstr_sv(resolved);
            if(resolved_sv.size)
            {
                resolved_sv.size -= 1;
            }

            sv_concat(*repositories, resolved_sv, repositories_cstr);
            *repositories = cstr_sv(repositories_cstr);
        }
        else
        {
            char *resolved = malloc(PATH_MAX);
            gfExpandPath(buffer.data, resolved);
            free((void*)repositories->data);
            *repositories = cstr_sv(resolved);

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

    sv_sort_by_delim(*authors, ';', sorted_authors);
    if(authors->data)
    {
        free((void*)authors->data);
    }
    *authors = cstr_sv(sorted_authors);

    sv_sort_by_delim(*repositories, ';', sorted_repos);
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
    uint8_t  colour
){
    if(!commit_count)
    {
        printf(HEAT_0);
    }
    else if(commit_count > 9 && colour == GREEN)
    {
        printf(HEAT_5_GREEN);
    }
    else if(commit_count > 9)
    {
        printf(HEAT_5_RED);
    }
    else if(commit_count > 7 && colour == GREEN)
    {
        printf(HEAT_4_GREEN);
    }
    else if(commit_count > 7)
    {
        printf(HEAT_4_RED);
    }
    else if(commit_count > 5 && colour == GREEN)
    {
        printf(HEAT_3_GREEN);
    }
    else if(commit_count > 5)
    {
        printf(HEAT_3_RED);
    }
    else if(commit_count > 3 && colour == GREEN)
    {
        printf(HEAT_2_GREEN);
    }
    else if(commit_count > 3)
    {
        printf(HEAT_2_RED);
    }
    else if(colour == GREEN)
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
    uint8_t  weekday_365,
    uint8_t  colour
){
    const char *months[12] =
    {
        "Jan",
        "Feb",
        "Mar",
        "Apr",
        "May",
        "Jun",
        "Jul",
        "Aug",
        "Sep",
        "Oct",
        "Nov",
        "Dec"
    };

    const char *days[7] =
    {
        "Mon",
        "Tue",
        "Wed",
        "Thu",
        "Fri",
        "Sat",
        "Sun",
    };

    printf("       ");
    for(uint8_t i = 0; i < 13; ++i)
    {
        printf("%s ", months[(currentMonth + i) % 12]);
    }
    printf("\n");

    for(uint8_t i = 0; i < 7; ++i)
    {
        printf(" %s ", days[i]);
        for(int16_t j = i - weekday_365; j < 366; j += 7)
        {
            if(j < 0)
            {
                printf(" ");
                continue;
            }
            printCorrespondingHeat(heatmap[365 - j], colour);
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
    uint8_t    colour       = RED;

    char sorted_repos[PATH_MAX * 100];
    char sorted_authors[PATH_MAX * 2];
    char biggestRepo_buf[PATH_MAX + 1];

    readConfig(&repositories, &authors, sorted_repos, sorted_authors, &colour);
    uint32_t repository_count = sv_count_by_delim(repositories, ';');

    git_libgit2_init();

    uint32_t   heatmap[16384]   = {0};
    StringView biggestRepo      = {0};
    uint32_t   repo_max         = 0;
    int64_t    oldestCommitTime = INT64_MAX;
    int64_t    now              = gfQueryTime();
    int64_t    currDayEnd       = now - now % (24 * 3600) + 24 * 3600;

    for(uint32_t i = 0; i < repository_count; ++i)
    {
        git_repository *repo    = 0;
        git_revwalk    *revwalk = 0;
        git_oid        oid      = {0};

        StringView repository      = sv_find_by_delim(repositories, ';', i);
        uint32_t   repoCommitCount = 0;

        #ifdef DEBUG
            fprintf(stderr, "\nAnalyzing repository %u: "PRI_SV"\n", i,
                    ARG_SV(repository));
        #endif

        char current_repo_cstr[repository.size + 1];
        sv_cstr(repository, current_repo_cstr);

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
                    int64_t days = (currDayEnd - commit_time.time) / (24 * 3600);
                    if(!days)
                    {
                        printf("%s\n", git_commit_message(commit));
                    }
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
            biggestRepo = cstr_sv_cpy(current_repo_cstr, biggestRepo_buf);
            repo_max    = repoCommitCount;
        }

        git_revwalk_free(revwalk);
        git_repository_free(repo);
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
    printf("\ncommits today: %u ", heatmap[0]);
    printCorrespondingHeat(heatmap[0], colour);
    printf("\n");

    int64_t  days_epoch    = now / (24 * 3600);
    uint32_t years_epoch   = 0;
    int64_t  currYearStart = 0;
    for(uint32_t i = 0; i < days_epoch;)
    {
        ++years_epoch;

        if(years_epoch % 4 == 2)
        {
            i += 366;
            if(currYearStart + 366 * 24 * 3600 < now)
            {
                currYearStart += 366 * 24 * 3600;
            }

            continue;
        }

        i += 365;
        if(currYearStart + 365 * 24 * 3600 < now)
        {
            currYearStart += 365 * 24 * 3600;
        }
    }

    uint8_t currentMonth   = 0;
    int64_t currMonthStart = currYearStart;
    for(uint16_t i = 0; i < 365; ++i)
    {
        if(currYearStart + i * 24 * 3600 > now)
        {
            break;
        }

        ++currentMonth;

        if(i == 1 && years_epoch % 4 == 2)
        {
            i += 29;
            currMonthStart += 29 * 24 * 3600;
        }
        else if(i == 1)
        {
            i += 28;
            currMonthStart += 28 * 24 * 3600;
        }
        else if(i == 0 || i == 2 || i == 4 || i == 6 || i == 7 || i == 9 || i == 11)
        {
            i += 31;
            currMonthStart += 31 * 24 * 3600;
        }
        else
        {
            i += 30;
            currMonthStart += 30 * 24 * 3600;
        }
    }

    int64_t days_commit = (now - oldestCommitTime) / (24 * 3600);

    #ifdef DEBUG
        printf("\nfull days since epoch: %lu\n", days_epoch);
        printf("full years since epoch: %u\n", years_epoch);
        printf("now, unix time: %lu\n", now);
        printf("current year start: %lu\n", currYearStart);
        printf("current month start: %lu\n", currMonthStart);
        printf("current month: %u\n", currentMonth);
        printf("current day start: %lu\n", currDayStart);
    #endif

    printf("days since first commit: %lu\n", days_commit);

    printf("\nheatmap (last 365 days):\n\n");
    uint8_t weekday_365 = 2;
    printHeatMap(heatmap, currentMonth, weekday_365, colour);

    return git_libgit2_shutdown();
}
