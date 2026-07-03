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
#define YELLOW 4

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

const char *colours[25] =
{
    // reds
    "\033[38;2;100;10;10m\u25FC\033[0m",
    "\033[38;2;150;15;15m\u25FC\033[0m",
    "\033[38;2;200;20;20m\u25FC\033[0m",
    "\033[38;2;220;30;30m\u25FC\033[0m",
    "\033[38;2;255;40;50m\u25FC\033[0m",
    // greens
    "\033[38;2;50;100;50m\u25FC\033[0m",
    "\033[38;2;50;150;50m\u25FC\033[0m",
    "\033[38;2;50;200;50m\u25FC\033[0m",
    "\033[38;2;50;220;50m\u25FC\033[0m",
    "\033[38;2;50;255;50m\u25FC\033[0m",
    // blues
    "\033[38;2;20;20;100m\u25FC\033[0m",
    "\033[38;2;20;20;150m\u25FC\033[0m",
    "\033[38;2;20;20;200m\u25FC\033[0m",
    "\033[38;2;20;20;220m\u25FC\033[0m",
    "\033[38;2;20;20;255m\u25FC\033[0m",
    // purples
    "\033[38;2;100;0;100m\u25FC\033[0m",
    "\033[38;2;150;0;150m\u25FC\033[0m",
    "\033[38;2;200;0;200m\u25FC\033[0m",
    "\033[38;2;220;0;220m\u25FC\033[0m",
    "\033[38;2;255;0;255m\u25FC\033[0m",
    // yellows
    "\033[38;2;90;90;0m\u25FC\033[0m",
    "\033[38;2;120;120;0m\u25FC\033[0m",
    "\033[38;2;175;175;0m\u25FC\033[0m",
    "\033[38;2;200;200;0m\u25FC\033[0m",
    "\033[38;2;255;255;0m\u25FC\033[0m",
};

f_internal uint8_t daysInMonth
(
    uint8_t month,
    uint8_t leapYear
){
    if(month == 1 && leapYear)
    {
        return 29;
    }
    else if(month == 1)
    {
        return 28;
    }
    else if(month == 3 || month == 5 || month == 8 || month == 10)
    {
        return 30;
    }
    else
    {
        return 31;
    }
}

f_internal void readConfig
(
    StringView *repositories,
    StringView *authors,
    char       *sorted_repos,
    char       *sorted_authors,
    uint8_t    *colour,
    uint8_t    *info
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

        StringView  info_sv = cstr_sv("info: ");
        const char* infoloc = sv_find(info_sv, buffer);

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
            StringView blue_sv   = cstr_sv("blue\n");
            StringView purple_sv = cstr_sv("purple\n");
            StringView yellow_sv = cstr_sv("yellow\n");

            if(sv_same(chosen_sv, green_sv))
            {
                *colour = GREEN;
            }
            else if(sv_same(chosen_sv, blue_sv))
            {
                *colour = BLUE;
            }
            else if(sv_same(chosen_sv, purple_sv))
            {
                *colour = PURPLE;
            }
            else if(sv_same(chosen_sv, yellow_sv))
            {
                *colour = YELLOW;
            }

            #ifdef DEBUG
                fprintf(stderr, "detected colour: "PRI_SV"\n", ARG_SV(chosen_sv));
            #endif

            continue;
        }
        else if(infoloc)
        {
            StringView set_sv  = cstr_sv(buffer.data + info_sv.size);
            StringView true_sv = cstr_sv("true\n");

            if(sv_same(set_sv, true_sv))
            {
                *info = 1;
            }
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
            char *resolved = calloc(PATH_MAX, 1);
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
        printf(" ");
    }
    else if(commit_count > 9)
    {
        printf("%s", colours[4 + colour * 5]);
    }
    else if(commit_count > 7)
    {
        printf("%s", colours[3 + colour * 5]);
    }
    else if(commit_count > 5)
    {
        printf("%s", colours[2 + colour * 5]);
    }
    else if(commit_count > 3)
    {
        printf("%s", colours[1 + colour * 5]);
    }
    else
    {
        printf("%s", colours[0 + colour * 5]);
    }
}

f_internal void printHeatMap
(
    uint32_t *heatmap,
    uint8_t  currentMonth,
    uint8_t  weekday_365,
    uint8_t  weekday,
    uint8_t  day_month,
    uint8_t  leapYear,
    uint8_t  colour
){
    uint8_t column = 0;
    uint8_t printedColumns = 0;
    printf("     ");
    for(uint8_t i = 0; i < 13; ++i)
    {
        uint8_t indexedMonth   = (currentMonth + i) % 12;
        uint8_t remainingWeeks = (daysInMonth(indexedMonth, leapYear) + weekday + 6) / 7;
        if(i == 0)
        {
            uint8_t remainingDays = daysInMonth(indexedMonth, leapYear) - day_month + 1;
            remainingWeeks = (remainingDays + weekday + 6) / 7;
        }

        if(remainingWeeks > 3)
        {
            while(printedColumns < column)
            {
                printf(" ");
                ++printedColumns;
            }

            printf("%s", months[indexedMonth]);
            printedColumns += 3;
        }

        for(uint8_t d = 0; d < daysInMonth(indexedMonth, leapYear); ++d)
        {
            if (++weekday == 7)
            {
                weekday = 0;
                ++column;
            }
        }

        if(i == 0)
        {
            day_month = 1;
        }
    }
    printf("\n");

    for(uint8_t i = 0; i < 7; ++i)
    {
        printf(" %s ", days[i]);
        for(int16_t j = i - weekday_365; j < 366; j += 7)
        {
            if(j < 0 && j >= weekday_365)
            {
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
    uint8_t    info         = 0;

    char sorted_repos[PATH_MAX * 100];
    char sorted_authors[PATH_MAX * 2];
    char biggestRepo_buf[PATH_MAX + 1];

    readConfig(&repositories, &authors, sorted_repos, sorted_authors, &colour, &info);
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
                    int64_t daysSince = (currDayEnd - commit_time.time) / (24 * 3600);
                    if(commit_time.time < oldestCommitTime)
                    {
                        oldestCommitTime = commit_time.time;
                    }

                    ++heatmap[daysSince];
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

    if(info)
    {
        printf("most commits in the last 365 days (%u) made %u days ago.\n",
               singleday_max, maxday);
        printf("most commits in single repository (%u) in '"PRI_SV"'.\n", repo_max,
               ARG_SV(biggestRepo));
        printf("\ncommits today: %u ", heatmap[0]);
        printCorrespondingHeat(heatmap[0], colour);
        printf("\n");
    }

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

        uint8_t daysThisMonth = daysInMonth(currentMonth, years_epoch % 4 == 2);
        i += daysThisMonth;
        ++currentMonth;
        currMonthStart += daysThisMonth * 24 * 3600;
    }

    int64_t days_commit = (now - oldestCommitTime) / (24 * 3600);
    uint8_t weekday_365 = 3 + (uint8_t)((days_epoch - 365) % 7);
    if(years_epoch % 4 == 2)
    {
        ++weekday_365;
        weekday_365 %= 7;
    }
    uint8_t weekday_today = 3 + (uint8_t)(days_epoch % 7);
    uint8_t day_of_month  = (uint8_t)((currDayEnd - currMonthStart) / (24 * 3600));

    #ifdef DEBUG
        printf("\nfull days since epoch: %lu\n", days_epoch);
        printf("full years since epoch: %u\n", years_epoch);
        printf("now, unix time: %lu\n", now);
        printf("current year start: %lu\n", currYearStart);
        printf("current month: %u\n", currentMonth);
        // printf("current day start: %lu\n", currDayStart);
        printf("weekday 365 days ago: %u\n", days[weekday_365]);
        printf("day of the month, today: %u\n", day_of_month);
        printf("pallette: ");
        printCorrespondingHeat(2, colour);
        printCorrespondingHeat(4, colour);
        printCorrespondingHeat(6, colour);
        printCorrespondingHeat(8, colour);
        printCorrespondingHeat(10, colour);
        printf("\n");
    #endif

    if(info)
    {
        printf("days since first commit: %lu\n", days_commit);
        printf("\nheatmap (last 365 days):\n");
    }

    printf("\n");
    printHeatMap(heatmap, currentMonth, weekday_365, weekday_today, day_of_month,
                 years_epoch % 4 == 2, colour);

    return git_libgit2_shutdown();
}
