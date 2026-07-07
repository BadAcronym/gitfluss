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
#define CYAN   3
#define PURPLE 4
#define YELLOW 5

typedef struct gfConf
{
    StringView repositories;
    StringView authors;
    char       *sorted_repos;
    char       *sorted_authors;
    const char *character;
    uint8_t    colour;
    uint8_t    info;
}
gfConf;

typedef struct gfPercentiles
{
    uint32_t d20;
    uint32_t d50;
    uint32_t d75;
    uint32_t d90;
}
gfPercentiles;

typedef struct gfHeatmapSettings
{
    uint32_t      *heatmap;
    gfPercentiles *percentiles;
    uint8_t       currentMonth;
    uint8_t       weekday_365;
    uint8_t       day_of_month;
    uint8_t       leapYear;
    const char    *character;
    uint8_t       colour;
}
gfHeatmapSettings;

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

#define ANSI_END "\033[0m"

const char *colours[30] =
{
    // reds
    "\033[38;2;80;10;10m",
    "\033[38;2;120;15;15m",
    "\033[38;2;180;20;20m",
    "\033[38;2;200;20;20m",
    "\033[38;2;255;30;30m",
    // greens
    "\033[38;2;50;100;50m",
    "\033[38;2;50;150;50m",
    "\033[38;2;50;200;50m",
    "\033[38;2;50;220;50m",
    "\033[38;2;50;255;50m",
    // blues
    "\033[38;2;20;20;100m",
    "\033[38;2;20;20;150m",
    "\033[38;2;20;20;200m",
    "\033[38;2;20;20;220m",
    "\033[38;2;20;20;255m",
    // cyans
    "\033[38;2;10;100;100m",
    "\033[38;2;20;150;150m",
    "\033[38;2;50;180;180m",
    "\033[38;2;80;220;220m",
    "\033[38;2;100;250;255m",
    // purples
    "\033[38;2;90;0;90m",
    "\033[38;2;120;0;120m",
    "\033[38;2;175;0;175m",
    "\033[38;2;200;0;200m",
    "\033[38;2;255;0;255m",
    // yellows
    "\033[38;2;90;90;0m",
    "\033[38;2;120;120;0m",
    "\033[38;2;175;175;0m",
    "\033[38;2;200;200;0m",
    "\033[38;2;255;255;0m",
};

f_internal void addAuthor
(
    gfConf     *config,
    StringView author
){
    StringView sep = cstr_sv(";");

    if(config->authors.data)
    {
        char *authors_cstr = malloc(config->authors.size + author.size + 2);
        sv_concat(config->authors, sep, authors_cstr);
        free((void*)config->authors.data);
        config->authors = cstr_sv(authors_cstr);

        sv_concat(config->authors, author, authors_cstr);
        config->authors = cstr_sv(authors_cstr);
    }
    else
    {
        char *new_buf = malloc(bufsize);
        char author_cstr[512];
        sv_cstr(author, author_cstr);
        config->authors = cstr_sv_cpy(author_cstr, new_buf);
    }

    #ifdef DEBUG
        fprintf(stderr, "detected author: "PRI_SV"\n", ARG_SV(author));
        fprintf(stderr, "author list: "PRI_SV"\n", ARG_SV(config->authors));
    #endif
}

f_internal void addPath
(
    gfConf     *config,
    StringView path
){
    StringView sep = cstr_sv(";");

    char path_cstr[4096];
    sv_cstr(path, path_cstr);

    if(config->repositories.data)
    {
        char *repositories_cstr = malloc(config->repositories.size + PATH_MAX + 2);
        sv_concat(config->repositories, sep, repositories_cstr);
        free((void*)config->repositories.data);
        config->repositories = cstr_sv(repositories_cstr);

        char resolved[PATH_MAX];
        gfExpandPath(path_cstr, resolved);
        StringView resolved_sv = cstr_sv(resolved);

        sv_concat(config->repositories, resolved_sv, repositories_cstr);
        config->repositories = cstr_sv(repositories_cstr);
    }
    else
    {
        char *resolved = calloc(PATH_MAX, 1);
        gfExpandPath(path_cstr, resolved);
        config->repositories = cstr_sv(resolved);
    }

    #ifdef DEBUG
        fprintf(stderr, "path list: "PRI_SV"\n", ARG_SV(config->repositories));
    #endif
}

f_internal void setColour
(
    gfConf     *config,
    StringView colour
){
    StringView green_sv  = cstr_sv("green");
    StringView blue_sv   = cstr_sv("blue");
    StringView cyan_sv   = cstr_sv("cyan");
    StringView purple_sv = cstr_sv("purple");
    StringView yellow_sv = cstr_sv("yellow");

    if(sv_same(colour, green_sv))
    {
        config->colour = GREEN;
    }
    else if(sv_same(colour, blue_sv))
    {
        config->colour = BLUE;
    }
    else if(sv_same(colour, cyan_sv))
    {
        config->colour = CYAN;
    }
    else if(sv_same(colour, purple_sv))
    {
        config->colour = PURPLE;
    }
    else if(sv_same(colour, yellow_sv))
    {
        config->colour = YELLOW;
    }

    #ifdef DEBUG
        fprintf(stderr, "detected colour: "PRI_SV"\n", ARG_SV(colour));
    #endif
}

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
    gfConf *config
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

        StringView  char_sv = cstr_sv("character: ");
        const char* charloc = sv_find(char_sv, buffer);

        if(authorloc)
        {
            StringView author = cstr_sv(buffer.data + author_sv.size);
            if(author.size)
            {
                author.size -= 1;
            }

            addAuthor(config, author);
        }
        else if(colourloc)
        {
            StringView chosen_sv = cstr_sv(buffer.data + colour_sv.size);
            if(chosen_sv.size)
            {
                chosen_sv.size -= 1;
            }
            setColour(config, chosen_sv);
        }
        else if(infoloc)
        {
            StringView set_sv  = cstr_sv(buffer.data + info_sv.size);
            StringView true_sv = cstr_sv("true\n");

            if(sv_same(set_sv, true_sv))
            {
                config->info = 1;
            }
        }
        else if(charloc)
        {
            StringView chosen_sv = cstr_sv(buffer.data + char_sv.size);
            if(chosen_sv.size)
            {
                chosen_sv.size -= 1;
            }

            char *small_buf = malloc(8);
            sv_cstr(chosen_sv, small_buf);
            config->character = small_buf;
        }
        else
        {
            StringView path = cstr_sv(buffer.data);
            if(path.size)
            {
                path.size -= 1;
            }
            addPath(config, path);
        }
    }

    sv_sort_by_delim(config->authors, ';', config->sorted_authors);
    if(config->authors.data)
    {
        free((void*)config->authors.data);
    }
    config->authors = cstr_sv(config->sorted_authors);

    sv_sort_by_delim(config->repositories, ';', config->sorted_repos);
    if(config->repositories.data)
    {
        free((void*)config->repositories.data);
    }
    config->repositories = cstr_sv(config->sorted_repos);

    #ifdef DEBUG
        fprintf(stderr, "\nfinal, sorted author list:\n"PRI_SV"\n",
                ARG_SV(config->authors));
        fprintf(stderr, "\nfinal, sorted paths:\n"PRI_SV"\n",
                ARG_SV(config->repositories));
    #endif

    fclose(file);
}

f_internal void readArgs
(
    int    argc,
    char   **argv,
    gfConf *config
){
    for(uint16_t i = 1; i < argc; ++i)
    {
        StringView arg          = cstr_sv(argv[i]);
        StringView author_ident = cstr_sv("--author");
        StringView colour_ident = cstr_sv("--colour");
        StringView info_ident   = cstr_sv("--info");
        StringView char_ident   = cstr_sv("--char");

        if(sv_same(arg, author_ident) && i + 1 < argc)
        {
            StringView author = cstr_sv(argv[i + 1]);
            printf("author: "PRI_SV"\n", ARG_SV(author));
            addAuthor(config, author);

            ++i;
        }
        else if(sv_same(arg, colour_ident) && i + 1 < argc)
        {
            StringView colour = cstr_sv(argv[i + 1]);
            setColour(config, colour);

            ++i;
        }
        else if(sv_same(arg, info_ident))
        {
            config->info = 1;
        }
        else if(sv_same(arg, char_ident) && i + 1 < argc)
        {
            StringView chosen_sv = cstr_sv(argv[i + 1]);

            char *small_buf = malloc(8);
            sv_cstr(chosen_sv, small_buf);
            config->character = small_buf;

            ++i;
        }
        else
        {
            StringView path = cstr_sv(argv[i]);
            addPath(config, path);
        }
    }
}

f_internal void printCorrespondingHeat
(
    gfPercentiles *percentiles,
    uint32_t      commit_count,
    const char    *character,
    uint8_t       colour
){
    if(!character)
    {
        character = "\u25FC";
    }

    if(!commit_count)
    {
        printf(" ");
        return;
    }
    else if(commit_count > percentiles->d90)
    {
        printf("%s", colours[4 + colour * 5]);
    }
    else if(commit_count > percentiles->d75)
    {
        printf("%s", colours[3 + colour * 5]);
    }
    else if(commit_count > percentiles->d50)
    {
        printf("%s", colours[2 + colour * 5]);
    }
    else if(commit_count > percentiles->d20)
    {
        printf("%s", colours[1 + colour * 5]);
    }
    else
    {
        printf("%s", colours[colour * 5]);
    }

    printf("%s%s", character, ANSI_END);
}

f_internal void printHeatMap
(
    gfHeatmapSettings *set
){
    uint8_t current_weekday = 0;

    printf("     ");
    for(uint8_t i = 0; i < 13; ++i)
    {
        uint8_t indexedMonth   = (set->currentMonth + i) % 12;
        uint8_t remainingDays  = daysInMonth(indexedMonth, set->leapYear);
        uint8_t remainingWeeks = (remainingDays + current_weekday) / 7;

        if(i == 0)
        {
            remainingDays  -= set->day_of_month - 1;
            remainingWeeks = (remainingDays) / 7;
        }

        if(remainingWeeks > 3)
        {
            printf("%s", months[indexedMonth]);

            for(uint8_t j = 3; j < remainingWeeks; ++j)
            {
                printf(" ");
            }
        }
        else
        {
            for(uint8_t j = 0; j < remainingWeeks; ++j)
            {
                printf(" ");
            }
        }

        for(uint8_t j = 0; j < remainingDays; ++j)
        {
            if(++current_weekday > 6)
            {
                current_weekday = 0;
            }
        }
    }
    printf("\n");

    for(uint8_t i = 0; i < 7; ++i)
    {
        printf(" %s ", days[i]);
        for(int16_t j = i - set->weekday_365; j < 366; j += 7)
        {
            printCorrespondingHeat(set->percentiles, set->heatmap[365 - j],
                                   set->character, set->colour);
        }
        printf("\n");
    }

    printf("\n");
}

int main
(
    int  argc,
    char **argv
){
    char sorted_repos[PATH_MAX * 100];
    char sorted_authors[PATH_MAX * 2];
    char biggestRepo_buf[PATH_MAX + 1];

    gfConf config = {0};
    config.sorted_repos   = sorted_repos;
    config.sorted_authors = sorted_authors;

    if(argc < 2)
    {
        readConfig(&config);
    }
    else
    {
        readArgs(argc, argv, &config);
    }

    if(!config.authors.data)
    {
        StringView any_author = cstr_sv("any");
        addAuthor(&config, any_author);
    }

    uint32_t repository_count = sv_count_by_delim(config.repositories, ';');

    git_libgit2_init();

    uint32_t   heatmap[16384]   = {0};
    uint32_t   sorted[366]      = {0};
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

        StringView repository      = sv_find_by_delim(config.repositories, ';', i);
        uint32_t   repoCommitCount = 0;

        #ifdef DEBUG
            fprintf(stderr, "\nAnalyzing repository %u: '"PRI_SV"'\n", i,
                    ARG_SV(repository));
        #endif

        char current_repo_cstr[repository.size + 1];
        sv_cstr(repository, current_repo_cstr);

        git_repository_open(&repo, current_repo_cstr);
        git_revwalk_new(&revwalk, repo);
        git_revwalk_push_head(revwalk);

        StringView any_sv     = cstr_sv("any");
        uint8_t    any_author = 0;

        while(!git_revwalk_next(&oid, revwalk))
        {
            git_commit *commit = 0;
            git_commit_lookup(&commit, repo, &oid);

            const git_signature *sign = git_commit_author(commit);

            StringView author_mail = cstr_sv(sign->email);
            git_time   commit_time = sign->when;
            uint8_t    counts      = any_author;

            for(uint16_t j = 0; !counts && j < MAX_AUTHORS; ++j)
            {
                StringView author = sv_find_by_delim(config.authors, ';', j);
                if(sv_same(author, author_mail))
                {
                    counts = 1;
                }
                else if(sv_same(author, any_sv))
                {
                    counts     = 1;
                    any_author = 1;
                }
            }

            if(counts)
            {
                int64_t daysSince = (currDayEnd - commit_time.time) / (24 * 3600);
                if(commit_time.time < oldestCommitTime)
                {
                    oldestCommitTime = commit_time.time;
                }

                ++heatmap[daysSince];
                ++repoCommitCount;
                if(daysSince < 366)
                {
                    ++sorted[daysSince];
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

    uint32_t max       = 0;
    uint32_t maxday    = 0;
    uint32_t zerocount = 0;

    for(uint32_t i = 0; i < 365; ++i)
    {
        for(uint32_t j = 0; j < 365 - i; ++j)
        {
            if(sorted[j] > max)
            {
                max    = sorted[j];
                maxday = j;
            }

            if(sorted[j] > sorted[j + 1])
            {
                uint32_t tmp  = sorted[j];
                sorted[j]     = sorted[j + 1];
                sorted[j + 1] = tmp;
            }
        }
    }

    for(uint16_t i = 0; i < 365 && !sorted[i]; ++i)
    {
        ++zerocount;
    }

    uint16_t d20_sep = (uint16_t)((float)(365 - zerocount) * 0.20f);
    uint16_t d50_sep = (uint16_t)((float)(365 - zerocount) * 0.50f);
    uint16_t d75_sep = (uint16_t)((float)(365 - zerocount) * 0.75f);
    uint16_t d90_sep = (uint16_t)((float)(365 - zerocount) * 0.90f);

    gfPercentiles percentiles = {0};
    percentiles.d20 = sorted[zerocount + d20_sep];
    percentiles.d50 = sorted[zerocount + d50_sep];
    percentiles.d75 = sorted[zerocount + d75_sep];
    percentiles.d90 = sorted[zerocount + d90_sep];

    #ifdef DEBUG
        printf("found 0-days: %u\n", zerocount);
        printf("found d20_sep: %u\n", d20_sep);
        printf("found d50_sep: %u\n", d50_sep);
        printf("found d75_sep: %u\n", d75_sep);
        printf("found d90_sep: %u\n", d90_sep);
        printf("found d20: %u\n", percentiles.d20);
        printf("found d50: %u\n", percentiles.d50);
        printf("found d75: %u\n", percentiles.d75);
        printf("found d90: %u\n", percentiles.d90);
    #endif

    if(config.info)
    {
        printf("most commits in the last 365 days (%u) made %u days ago.\n",
               max, maxday);
        printf("most commits in single repository (%u) in '"PRI_SV"'.\n", repo_max,
               ARG_SV(biggestRepo));
        printf("\ncommits today: %u ", heatmap[0]);
        printCorrespondingHeat(&percentiles, heatmap[0], config.character,
                               config.colour);
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
        if(currMonthStart + daysThisMonth * 24 * 3600 > now)
        {
            continue;
        }
        ++currentMonth;
        currMonthStart += daysThisMonth * 24 * 3600;
    }

    int64_t days_commit = (now - oldestCommitTime) / (24 * 3600);
    uint8_t weekday_365 = (3 + (uint8_t)(days_epoch - 364)) % 7;
    if(years_epoch % 4 == 2)
    {
        ++weekday_365;
        weekday_365 %= 7;
    }
    uint8_t day_of_month = (uint8_t)((currDayEnd - currMonthStart) / (24 * 3600));

    #ifdef DEBUG
        printf("\nfull days since epoch: %lu\n", days_epoch);
        printf("full years since epoch: %u\n", years_epoch);
        printf("now, unix time: %lu\n", now);
        printf("current year start: %lu\n", currYearStart);
        printf("current month: %u\n", currentMonth);
        printf("current month start: %lu\n", currMonthStart);
        printf("weekday 365 days ago: %s\n", days[weekday_365]);
        printf("day of the month, today: %u\n", day_of_month);
        printf("palette: ");
        const char *debugChar = "\u25FC";
        printCorrespondingHeat(&percentiles, 1, debugChar, config.colour);
        printCorrespondingHeat(&percentiles, percentiles.d50, debugChar, config.colour);
        printCorrespondingHeat(&percentiles, percentiles.d75, debugChar, config.colour);
        printCorrespondingHeat(&percentiles, percentiles.d90, debugChar, config.colour);
        printCorrespondingHeat(&percentiles, max, debugChar, config.colour);
        printf("\n");
    #endif

    if(config.info)
    {
        printf("days since first commit: %lu\n", days_commit);
        printf("\nheatmap (last 365 days):\n");
    }
    printf("\n");

    gfHeatmapSettings set = {0};
    set.percentiles  = &percentiles;
    set.heatmap      = heatmap;
    set.currentMonth = currentMonth;
    set.weekday_365  = weekday_365;
    set.day_of_month = day_of_month;
    set.leapYear     = years_epoch % 4 == 2;
    set.character    = config.character;
    set.colour       = config.colour;

    printHeatMap(&set);

    if(config.character)
    {
        free((void*)config.character);
    }

    return git_libgit2_shutdown();
}
