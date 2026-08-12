#include "gitfluss.h"

#include <git2.h>

#include <stdint.h>
#include <stdio.h>

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

const char *colours[40] =
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
    "\033[38;2;0;0;90m",
    "\033[38;2;5;10;125m",
    "\033[38;2;10;20;150m",
    "\033[38;2;15;30;200m",
    "\033[38;2;20;40;255m",
    // cyans
    "\033[38;2;10;100;100m",
    "\033[38;2;20;150;150m",
    "\033[38;2;50;180;180m",
    "\033[38;2;80;220;220m",
    "\033[38;2;100;250;255m",
    // purples
    "\033[38;2;50;10;90m",
    "\033[38;2;75;20;120m",
    "\033[38;2;100;30;175m",
    "\033[38;2;125;40;200m",
    "\033[38;2;175;75;255m",
    // pinks
    "\033[38;2;135;0;55m",
    "\033[38;2;160;10;75m",
    "\033[38;2;185;25;95m",
    "\033[38;2;210;40;115m",
    "\033[38;2;255;100;160m",
    // yellows
    "\033[38;2;90;90;0m",
    "\033[38;2;120;120;0m",
    "\033[38;2;175;175;0m",
    "\033[38;2;200;200;0m",
    "\033[38;2;255;255;0m",
    // whites
    "\033[38;2;125;125;125m",
    "\033[38;2;150;150;150m",
    "\033[38;2;175;175;175m",
    "\033[38;2;220;220;220m",
    "\033[38;2;255;255;255m",
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

f_internal void printCorrespondingHeat
(
    gfConf        *config,
    gfPercentiles *percentiles,
    uint32_t      commit_count
){
    if(!commit_count)
    {
        printf(" ");
        return;
    }

    if(config->flags & FLAG_MONO)
    {
        if(!config->mono0)
        {
            config->mono0 = "░";
        }
        if(!config->mono1)
        {
            config->mono1 = "▒";
        }
        if(!config->mono2)
        {
            config->mono2 = "▒";
        }
        if(!config->mono3)
        {
            config->mono3 = "▓";
        }
        if(!config->mono4)
        {
            config->mono4 = "█";
        }

        if(commit_count < percentiles->d20)
        {
            printf("%s", config->mono0);
        }
        else if(commit_count < percentiles->d50)
        {
            printf("%s", config->mono1);
        }
        else if(commit_count < percentiles->d70)
        {
            printf("%s", config->mono2);
        }
        else if(commit_count < percentiles->d90)
        {
            printf("%s", config->mono3);
        }
        else
        {
            printf("%s", config->mono4);
        }

        return;
    }

    uint8_t    colour     = config->colour;
    const char *character = config->character;
    if(!character)
    {
        character = "\u25FC";
    }

    if(commit_count < percentiles->d20)
    {
        printf("%s", colours[colour * 5]);
    }
    else if(commit_count < percentiles->d50)
    {
        printf("%s", colours[1 + colour * 5]);
    }
    else if(commit_count < percentiles->d70)
    {
        printf("%s", colours[2 + colour * 5]);
    }
    else if(commit_count < percentiles->d90)
    {
        printf("%s", colours[3 + colour * 5]);
    }
    else
    {
        printf("%s", colours[4 + colour * 5]);
    }

    printf("%s%s", character, ANSI_END);
}

f_internal void printHeatMap
(
    gfHeatmapSettings *set
){
    uint8_t currentWeekday = 0;
    printf("     ");

    for(uint8_t i = 0; i < 13; ++i)
    {
        uint8_t indexedMonth   = (set->currentMonth + i) % 12;
        uint8_t remainingDays  = daysInMonth(indexedMonth, set->leapYear);
        uint8_t remainingWeeks = (remainingDays + currentWeekday) / 7;

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
            if(++currentWeekday > 6)
            {
                currentWeekday = 0;
            }
        }
    }
    printf("\n");

    for(uint8_t i = 0; i < 7; ++i)
    {
        printf(" %s ", days[i]);
        for(int16_t j = i - set->weekday365; j < 366; j += 7)
        {
            printCorrespondingHeat(set->config, set->percentiles,
                                   set->heatmap[365 - j]);
        }
        printf("\n");
    }

    printf("\n");
}

f_internal void *gatherRepoData
(
    void *arguments
){
    gfThreadData *data = (gfThreadData*)arguments;
    gfDisplaySettings *set        = data->set;
    StringView        repository  = data->repository;
    StringView        *authorlist = data->authorlist;
    uint32_t          authorcount = data->authorcount;

    git_repository *repo    = 0;
    git_revwalk    *revwalk = 0;
    git_oid        oid      = {0};

    uint32_t repoCommitCount = 0;

    char current_repo_cstr[repository.size + 1];
    sv_cstr(repository, current_repo_cstr);

    git_repository_open(&repo, current_repo_cstr);
    git_revwalk_new(&revwalk, repo);
    git_revwalk_push_head(revwalk);

    uint8_t    any_author = 0;
    git_commit *commit    = 0;

    while(!git_revwalk_next(&oid, revwalk))
    {
        gfLock(set);
        ++set->totalCommitCount;
        git_commit_lookup(&commit, repo, &oid);

        const git_signature *sign = git_commit_author(commit);

        StringView author_mail = cstr_sv(sign->email);
        git_time   commit_time = sign->when;
        uint8_t    counts      = any_author;

        for(uint16_t j = 0; !counts && j < authorcount; ++j)
        {
            if(sv_same(authorlist[j], author_mail))
            {
                counts = 1;
            }
            else if(sv_same(authorlist[j], cstr_sv("any")))
            {
                counts     = 1;
                any_author = 1;
            }
        }

        if(counts)
        {
            int64_t daysSince = (set->currDayEnd - commit_time.time) / (24 * 3600);
            if(commit_time.time < set->oldestCommitTime)
            {
                set->oldestCommitTime = commit_time.time;
            }

            ++set->heatmap[daysSince];
            ++repoCommitCount;
            ++set->personalCommitCount;
            if(daysSince < 366)
            {
                ++set->sorted[daysSince];
            }
        }
        gfUnlock(set);

        git_commit_free(commit);
    }

    if(repoCommitCount > set->repoMax)
    {
        gfLock(set);
        set->biggestRepo = cstr_sv_cpy(current_repo_cstr, set->biggestRepoBuf);
        set->repoMax     = repoCommitCount;
        gfUnlock(set);
    }

    git_revwalk_free(revwalk);
    git_repository_free(repo);

    return 0;
}

f_internal void gatherData
(
    gfConf            *config,
    gfDisplaySettings *set
){
    uint32_t   authorcount = sv_count_by_delim(config->authors, ';');
    StringView authorlist[authorcount];

    sv_separate_by_delim(config->authors, authorlist, ';');

    gfThread     threads[set->repositoryCount];
    gfThreadData threadData[set->repositoryCount];

    for(uint32_t i = 0; i < set->repositoryCount; ++i)
    {
        threadData[i].id          = i;
        threadData[i].repository  = sv_find_by_delim(config->repositories, ';', i);
        threadData[i].authorcount = authorcount;
        threadData[i].authorlist  = authorlist;
        threadData[i].set         = set;

        #ifdef DEBUG
            fprintf(stderr, "Thread %u: analyzing repository: '"PRI_SV"'\n", i,
                    ARG_SV(threadData[i].repository));
        #endif

        gfDispatchThread(&threads[i], (void*)gatherRepoData, &threadData[i]);
    }

    for(uint32_t i = 0; i < set->repositoryCount; ++i)
    {
        gfWaitThread(threads[i]);
    }
}

f_internal void displayData
(
    gfConf            *config,
    gfDisplaySettings *set
){
    uint32_t max       = 0;
    uint32_t maxday    = 0;
    uint32_t zerocount = 0;

    for(uint32_t i = 0; i < 365; ++i)
    {
        for(uint32_t j = 0; j < 365 - i; ++j)
        {
            if(set->sorted[j] > max)
            {
                max    = set->sorted[j];
                maxday = j;
            }

            if(set->sorted[j] > set->sorted[j + 1])
            {
                uint32_t tmp = set->sorted[j];
                set->sorted[j]     = set->sorted[j + 1];
                set->sorted[j + 1] = tmp;
            }
        }
    }

    for(uint16_t i = 0; i < 365 && !set->sorted[i]; ++i)
    {
        ++zerocount;
    }

    uint16_t d20_sep = (uint16_t)((float)(365 - zerocount) * 0.20f);
    uint16_t d50_sep = (uint16_t)((float)(365 - zerocount) * 0.50f);
    uint16_t d70_sep = (uint16_t)((float)(365 - zerocount) * 0.70f);
    uint16_t d90_sep = (uint16_t)((float)(365 - zerocount) * 0.90f);

    gfPercentiles percentiles = {0};
    percentiles.d20 = set->sorted[zerocount + d20_sep];
    percentiles.d50 = set->sorted[zerocount + d50_sep];
    percentiles.d70 = set->sorted[zerocount + d70_sep];
    percentiles.d90 = set->sorted[zerocount + d90_sep];

    #ifdef DEBUG
        printf("found 0-days: %u\n", zerocount);
        printf("found d20_sep: %u\n", d20_sep);
        printf("found d50_sep: %u\n", d50_sep);
        printf("found d70_sep: %u\n", d70_sep);
        printf("found d90_sep: %u\n", d90_sep);
        printf("found d20: %u\n", percentiles.d20);
        printf("found d50: %u\n", percentiles.d50);
        printf("found d70: %u\n", percentiles.d70);
        printf("found d90: %u\n", percentiles.d90);
    #endif

    int64_t  days_epoch    = set->now / (24 * 3600);
    uint32_t years_epoch   = 0;
    int64_t  currYearStart = 0;
    for(uint32_t i = 0; i < days_epoch;)
    {
        ++years_epoch;

        if(years_epoch % 4 == 2)
        {
            i += 366;
            if(currYearStart + 366 * 24 * 3600 < set->now)
            {
                currYearStart += 366 * 24 * 3600;
            }

            continue;
        }

        i += 365;
        if(currYearStart + 365 * 24 * 3600 < set->now)
        {
            currYearStart += 365 * 24 * 3600;
        }
    }

    uint8_t currentMonth   = 0;
    int64_t currMonthStart = currYearStart;
    for(uint16_t i = 0; i < 365; ++i)
    {
        if(currYearStart + i * 24 * 3600 > set->now)
        {
            break;
        }

        uint8_t daysThisMonth = daysInMonth(currentMonth, years_epoch % 4 == 2);

        i += daysThisMonth;
        if(currMonthStart + daysThisMonth * 24 * 3600 > set->now)
        {
            continue;
        }
        ++currentMonth;
        currMonthStart += daysThisMonth * 24 * 3600;
    }

    int64_t daysCommit = (set->now - set->oldestCommitTime) / (24 * 3600);
    uint8_t weekday365 = (3 + (uint8_t)(days_epoch - 364)) % 7;
    if(years_epoch % 4 == 2)
    {
        ++weekday365;
        weekday365 %= 7;
    }
    uint8_t day_of_month = (uint8_t)((set->currDayEnd - currMonthStart) / (24 * 3600));

    #ifdef DEBUG
        printf("\nfull days since epoch: %lu\n", days_epoch);
        printf("full years since epoch: %u\n", years_epoch);
        printf("now, unix time: %lu\n", set->now);
        printf("current year start: %lu\n", currYearStart);
        printf("current month: %u\n", currentMonth);
        printf("current month start: %lu\n", currMonthStart);
        printf("weekday 365 days ago: %s\n", days[weekday365]);
        printf("day of the month, today: %u\n", day_of_month);
        printf("palette: ");
        printCorrespondingHeat(config, &percentiles, 1);
        printCorrespondingHeat(config, &percentiles, percentiles.d20);
        printCorrespondingHeat(config, &percentiles, percentiles.d50);
        printCorrespondingHeat(config, &percentiles, percentiles.d70);
        printCorrespondingHeat(config, &percentiles, percentiles.d90);
        printf("\n");
    #endif

    uint32_t currentStreak      = 0;
    uint8_t  brokeCurrentStreak = 0;

    uint32_t streak = 0;
    uint32_t longestStreak = 0;

    for(uint32_t i = 0; i < MAX_DAYS; ++i)
    {
        if(!brokeCurrentStreak && !set->heatmap[i])
        {
            brokeCurrentStreak = 1;
            if(!i)
            {
                brokeCurrentStreak = 0;
                streak = 0;
            }
        }
        else if(!brokeCurrentStreak)
        {
            ++currentStreak;
        }

        if(!set->heatmap[MAX_DAYS - i - 1])
        {
            if(streak > longestStreak)
            {
                longestStreak = streak;
            }
            streak = 0;
        }
        else
        {
            ++streak;
        }
    }

    if(config->flags & FLAG_INFO)
    {
        float percentage = 100.0f * (float)set->personalCommitCount /
                           (float)set->totalCommitCount;

        printf("\n%lu commits analyzed across %u repositories.\n",
               set->totalCommitCount, set->repositoryCount);
        printf("%lu were matched with a provided author (%.2f%%).\n",
               set->personalCommitCount, percentage);

        printf("most commits in the last 365 days (%u) made %u days ago.\n",
               max, maxday);
        printf("most commits in single repository (%u) in '"PRI_SV"'.\n\n",
               set->repoMax, ARG_SV(set->biggestRepo));

        printf("time since first commit: %lu days\n\n", daysCommit);

        printf("longest streak: %u days\n", longestStreak);
        printf("current streak: %u days\n", currentStreak);
        printf("commits today:  %u ", set->heatmap[0]);
        printCorrespondingHeat(config, &percentiles, set->heatmap[0]);
        printf("\n");

        printf("\nheatmap (last 365 days):\n");
    }
    printf("\n");

    gfHeatmapSettings heatSet = {0};
    heatSet.config       = config;
    heatSet.percentiles  = &percentiles;
    heatSet.heatmap      = set->heatmap;
    heatSet.currentMonth = currentMonth;
    heatSet.weekday365   = weekday365;
    heatSet.day_of_month = day_of_month;
    heatSet.leapYear     = years_epoch % 4 == 2;

    printHeatMap(&heatSet);
}

void sortStrings
(
    gfConf *config
){

    sv_sort_by_delim(config->authors, ';', config->sortedAuthors);
    if(config->authors.data)
    {
        free((void*)config->authors.data);
    }
    config->authors = cstr_sv(config->sortedAuthors);

    sv_sort_by_delim(config->repositories, ';', config->sortedRepos);
    if(config->repositories.data)
    {
        free((void*)config->repositories.data);
    }
    config->repositories = cstr_sv(config->sortedRepos);

    #ifdef DEBUG
        fprintf(stderr, "\nfinal, sorted author list:\n"PRI_SV"\n",
                ARG_SV(config->authors));
        fprintf(stderr, "\nfinal, sorted paths:\n"PRI_SV"\n",
                ARG_SV(config->repositories));
    #endif
}

int main
(
    int  argc,
    char **argv
){
    #ifdef BUILD_WINDOWS
    _setmode(_fileno(stdout), _O_BINARY);
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    HANDLE hOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleMode(hOutput, ENABLE_PROCESSED_OUTPUT |
                            ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    #endif

    uint64_t now = gfQueryMonotonic();

    char sortedRepos[MAX_PATH * 128];
    char sortedAuthors[MAX_PATH * 4];
    char biggestRepoBuf[MAX_PATH + 1];

    uint32_t heatmap[MAX_DAYS] = {0};
    uint32_t sorted[366]       = {0};

    gfConf config = {0};
    config.sortedRepos   = sortedRepos;
    config.sortedAuthors = sortedAuthors;

    gfReadConfig(&config);
    if(argc > 1)
    {
        gfReadArgs(argc, argv, &config);
    }

    sortStrings(&config);

    if(!config.authors.data)
    {
        StringView any_author = cstr_sv("any");
        gfAddAuthor(&config, any_author);
    }

    if(!config.repositories.data)
    {
        StringView fallback = cstr_sv(".");
        gfAddPath(&config, fallback);
    }

    gfDisplaySettings set = {0};
    set.heatmap          = heatmap;
    set.sorted           = sorted;
    set.biggestRepoBuf   = biggestRepoBuf;
    set.oldestCommitTime = INT64_MAX;
    set.repositoryCount  = sv_count_by_delim(config.repositories, ';');
    set.now              = gfQueryTime();
    set.currDayEnd       = set.now - set.now % (24 * 3600) + 24 * 3600;

    #ifdef BUILD_WINDOWS
        set.mutexSet = CreateMutexA(0, 0, 0);
    #endif

    git_libgit2_init();

    uint64_t initialization = gfQueryMonotonic() - now;
    now = gfQueryMonotonic();

    gatherData(&config, &set);

    uint64_t gathering = gfQueryMonotonic() - now;

    displayData(&config, &set);

    if(config.flags & FLAG_PROFILE)
    {
        float initMS    = (float)initialization / 1e6f;
        float gatherMS  = (float)gathering / 1e6f;
        float perCommit = gatherMS / (float)set.totalCommitCount;

        printf("libgit2 init: %10.5f ms\n", initMS);
        printf("gather time:  %10.5f ms\n", gatherMS);
        printf("per commit:   %10.5f ms\n", perCommit);
        printf("effective:    %10.0f commits/s\n\n",
               (float)set.totalCommitCount / gatherMS * 1e3);
    }

    if(config.character)
    {
        free((void*)config.character);
    }
    git_libgit2_shutdown();
}
