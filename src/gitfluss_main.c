#include "gitfluss.h"
#include "gf_print_macros.h"

#include "datasurf_main.h"

#include "pd_dyn_arr.h"

#include <stdio.h>

s_global const char *months[12] =
{
    "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

s_global const char *days[7] =
{
    "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"
};

s_global const char *colours[40] =
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
    "\033[38;2;75;0;107m",
    "\033[38;2;100;0;143m",
    "\033[38;2;132;0;189m",
    "\033[38;2;157;0;224m",
    "\033[38;2;206;92;255m",
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

f_internal u08 daysInMonth
(
    u08 month,
    u08 leapYear
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
    u32           commitCount
){
    if(!commitCount)
    {
        printf(" ");
        return;
    }

    if(config->flags & GF_FLAG_MONO)
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

        if(commitCount < percentiles->d20)
        {
            printf("%s", config->mono0);
        }
        else if(commitCount < percentiles->d50)
        {
            printf("%s", config->mono1);
        }
        else if(commitCount < percentiles->d70)
        {
            printf("%s", config->mono2);
        }
        else if(commitCount < percentiles->d90)
        {
            printf("%s", config->mono3);
        }
        else
        {
            printf("%s", config->mono4);
        }

        return;
    }

    u08        colour     = config->colour;
    const char *character = config->character;
    if(!character)
    {
        character = "\u25FC";
    }

    if(commitCount < percentiles->d20)
    {
        printf("%s", colours[colour * 5]);
    }
    else if(commitCount < percentiles->d50)
    {
        printf("%s", colours[1 + colour * 5]);
    }
    else if(commitCount < percentiles->d70)
    {
        printf("%s", colours[2 + colour * 5]);
    }
    else if(commitCount < percentiles->d90)
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
    gfHeatmapSettings *set,
    u08               yearsBack
){
    u08 currentWeekday = 0;
    printf("     ");

    for(u08 i = 0; i < 13; ++i)
    {
        u08 indexedMonth   = (set->currentMonth + i) % 12;
        u08 remainingDays  = daysInMonth(indexedMonth, set->leapYear);
        u08 remainingWeeks = (remainingDays + currentWeekday) / 7;

        if(i == 0)
        {
            remainingDays  -= set->day_of_month - 1;
            remainingWeeks = (remainingDays) / 7;
        }

        if(remainingWeeks > 3)
        {
            printf("%s", months[indexedMonth]);

            for(u08 j = 3; j < remainingWeeks; ++j)
            {
                printf(" ");
            }
        }
        else
        {
            for(u08 j = 0; j < remainingWeeks; ++j)
            {
                printf(" ");
            }
        }

        for(u08 j = 0; j < remainingDays; ++j)
        {
            if(++currentWeekday > 6)
            {
                currentWeekday = 0;
            }
        }
    }
    printf("\n");

    for(u08 i = 0; i < 7; ++i)
    {
        i64 offset = (yearsBack + 1) * 365;
        printf(" %s ", days[i]);
        for(int16_t j = i - set->weekday365; j < 366; j += 7)
        {
            printCorrespondingHeat(set->config, set->percentiles,
                                   set->heatmap[offset - j]);
        }
        printf("\n");
    }

    printf("\n");
}

u08 gfTwoCharsToByte
(
    char c1,
    char c2
){
    u08 v1 = 0;
    u08 v2 = 0;

    GF_ASSERT((c1 > 0x2F && c1 < 0x3A) || (c1 > 0x60 && c1 < 0x67),
              "c1 outside of valid range. passed char: '%c' (0x%X)", c1, c1);
    GF_ASSERT((c2 > 0x2F && c2 < 0x3A) || (c2 > 0x60 && c2 < 0x67),
              "c2 outside of valid range. passed char: '%c' (0x%X)", c2, c2);

    v1 = (u08)c1 - '0';
    if(c1 > 0x60)
    {
        v1 = (u08)c1 - 0x57;
    }

    v2 = (u08)c2 - '0';
    if(c2 > 0x60)
    {
        v2 = (u08)c2 - 0x57;
    }

    return (u08)((v1 << 4) | v2);
}

char gfValueToHexChar
(
    u08 value
){
    GF_ASSERT(value < 0x10, "cannot express values bigger than 15 in 4 bits.");

    if(value > 9)
    {
        return value + 0x57;
    }

    return value + 0x30;
}

f_internal void *threadProcessRepository
(
    void *arguments
){
    gfRepoThreadData  *data       = (gfRepoThreadData*)arguments;
    gfDisplaySettings *set        = data->set;
    StringView        *authorlist = data->authorlist;
    u32               authorcount = data->authorcount;
    u08               flags       = data->flags;

    i64 currDayStart    = set->currDayEnd - (24 * 3600);
    u32 repoCommitCount = 0;

    bool anyAuthor = false;

    gfCommitInfo *table[256] = {0};

    gfCommitInfo commit  = {0};
    gfRepository repo    = {0};
    repo.path            = data->repository;
    gfInitRepository(&repo, &commit, table);

    StringView *missedQueue     = 0;
    StringView *seenHashes[256] = {0};

    pdArrReserve(missedQueue, 8);

    if(!commit.hash.data || !commit.hash.size)
    {
        return 0;
    }

    for(;;)
    {
        GF_TRACE("analyzing commit: '"PRI_SV"'", ARG_SV(commit.hash));

        u08 firstTwo = gfTwoCharsToByte(commit.hash.data[0], commit.hash.data[1]);

        bool duplicate = false;
        if(seenHashes[firstTwo] && pdArrSize(seenHashes[firstTwo]))
        {
            for(u64 i = 0; i < pdArrSize(seenHashes[firstTwo]); ++i)
            {
                if(pdSVSame(commit.hash, seenHashes[firstTwo][i]))
                {
                    duplicate = true;
                    break;
                }
            }
        }

        if(duplicate)
        {
            goto nextCommit;
        }

        bool counts = anyAuthor;

        if(commit.authorTime < set->startYearTime ||
           commit.authorTime > set->endYearTime
        ){
            GF_TRACE("skipped out-of-time commit: '"PRI_SV"'", ARG_SV(commit.hash));
            goto nextCommit;
        }

        for(u16 j = 0; !counts && j < authorcount; ++j)
        {
            if(pdSVSame(authorlist[j], commit.authorMail))
            {
                counts = true;
            }
            else if(pdSVSame(authorlist[j], pdCstrSV("any")))
            {
                counts    = true;
                anyAuthor = true;
            }
        }

        gfLock(&set->lock);
        ++set->totalCommitCount;
        gfUnlock(&set->lock);

        if(counts)
        {
            ++repoCommitCount;
            i64 daysSince = (set->currDayEnd - commit.authorTime) / (24 * 3600);

            gfLock(&set->lock);
            if(commit.authorTime < set->oldestCommitTime)
            {
                set->oldestCommitTime = commit.authorTime;
            }

            ++set->heatSet->heatmap[daysSince];
            ++set->personalCommitCount;
            if(daysSince < 366)
            {
                ++set->sorted[daysSince];
            }
            if(commit.authorTime >= currDayStart)
            {
                ++set->commitsToday;
            }
            gfUnlock(&set->lock);

            if(flags & GF_FLAG_SUMMARY && commit.authorTime >= currDayStart)
            {
                // TODO: instead of printing (because it's in reverse), add the
                // summary, timestamp & everything to a list and print at the
                // end.
                printf("("PRI_SV")\n[%02u:%02u]: "PRI_SV"\n\n",
                       ARG_SV(repo.path),
                       (u32)(commit.authorTime - currDayStart) / 3600,
                       (u32)(commit.authorTime % 3600) / 60,
                       ARG_SV(commit.summary));
            }
        }
        else if(flags & GF_FLAG_NOMATCH)
        {
            printf("unmatched author: "PRI_SV"\n", ARG_SV(commit.authorMail));
        }

        pdArrPush(seenHashes[firstTwo], pdSVCpy(commit.hash));

    nextCommit:
        if(!commit.parentHashes || !pdArrSize(commit.parentHashes))
        {
            GF_TRACE("dead end for this branch.");

            if(!missedQueue || !pdArrSize(missedQueue))
            {
                break;
            }

            GF_TRACE("new branch with commit: '"PRI_SV"'", ARG_SV(missedQueue[0]));

            pdArrPush(commit.parentHashes, pdSVCpy(missedQueue[0]));
            pdSVFree(&missedQueue[0]);
            pdArrRemove(missedQueue, 0, (StringView){0});
        }

        while(pdArrSize(commit.parentHashes) > 1)
        {
            u64 last = pdArrSize(commit.parentHashes) - 1;

            firstTwo  = gfTwoCharsToByte(commit.parentHashes[last].data[0],
                                         commit.parentHashes[last].data[1]);
            bool seen = false;
            if(seenHashes[firstTwo] && pdArrSize(seenHashes[firstTwo]))
            {
                for(u64 i = 0; i < pdArrSize(seenHashes[firstTwo]); ++i)
                {
                    if(pdSVSame(commit.parentHashes[last], seenHashes[firstTwo][i]))
                    {
                        seen = true;
                        break;
                    }
                }
            }

            if(!seen)
            {
                GF_TRACE("saving extra parent for later: '"PRI_SV"'",
                         ARG_SV(commit.parentHashes[last]));
                pdArrPush(missedQueue, pdSVCpy(commit.parentHashes[last]));
            }

            pdSVFree(&commit.parentHashes[last]);
            pdArrRemove(commit.parentHashes, last, (StringView){0});
        }

        StringView parentHash = pdSVCpy(commit.parentHashes[0]);

        if(!gfGetCommitInfo(repo.path, parentHash, &commit, table))
        {
            GF_TRACE("couldn't get commit info from parent: '"PRI_SV"'",
                     ARG_SV(parentHash));
            pdSVFree(&parentHash);
            break;
        }
        pdSVFree(&parentHash);
    }

    for(u16 i = 0; i < 256; ++i)
    {
        if(seenHashes[i])
        {
            for(u16 j = 0; j < pdArrSize(seenHashes[i]); ++j)
            {
                pdSVFree(&seenHashes[i][j]);
            }
            pdArrFree(seenHashes[i]);
        }
    }

    if(missedQueue)
    {
        for(u16 i = 0; i < pdArrSize(missedQueue); ++i)
        {
            pdSVFree(&missedQueue[i]);
        }
        pdArrFree(missedQueue);
    }

    if(repoCommitCount > set->repoMax)
    {
        gfLock(&set->lock);
        char currentRepo[repo.path.size + 1];
        pdSVCstr(repo.path, currentRepo);
        set->biggestRepo = pdCstrSVCpy(currentRepo, set->biggestRepoBuf);
        set->repoMax     = repoCommitCount;
        gfUnlock(&set->lock);
    }

    for(u16 i = 0; i < 256; ++i)
    {
        if(!table[i])
        {
            continue;
        }

        u64 arraySize = pdArrSize(table[i]);
        for(u64 j = 0; j < arraySize; ++j)
        {
            gfFreeCommit(&table[i][j]);
        }
        pdArrFree(table[i]);
    }

    gfFreeCommit(&commit);
    return 0;
}

f_internal void gatherData
(
    gfConf            *config,
    gfDisplaySettings *set
){
    u32 authorcount = pdSVCountByDelim(config->authors, ';');
    StringView authorlist[authorcount];

    pdSVSeparateByDelim(config->authors, authorlist, ';', authorcount);

    gfThread         threads[set->repositoryCount];
    gfRepoThreadData threadData[set->repositoryCount];

    GF_TRACE("dispatching %u threads.", set->repositoryCount);

    for(u32 i = 0; i < set->repositoryCount; ++i)
    {
        threadData[i].id          = i;
        threadData[i].repository  = pdSVFindByDelim(config->repositories, ';', i);
        threadData[i].authorcount = authorcount;
        threadData[i].authorlist  = authorlist;
        threadData[i].set         = set;
        threadData[i].flags       = config->flags;

        GF_TRACE("thread %u: analyzing repository: '"PRI_SV"'",
                 i, ARG_SV(threadData[i].repository));

        gfDispatchThread(&threads[i], (void*)threadProcessRepository, &threadData[i]);
    }

    for(u32 i = 0; i < set->repositoryCount; ++i)
    {
        GF_TRACE("waiting on thread #%u", i);
        gfWaitThread(threads[i]);
    }
}

f_internal void calculateHeatmap
(
    gfHeatmapSettings *set,
    u08               silent
){
    set->yearsEpoch = 0;

    i64 daysEpoch    = set->now / (24 * 3600);
    i64 currYearStart = 0;
    for(u32 i = 0; i < daysEpoch;)
    {
        ++set->yearsEpoch;

        if(set->yearsEpoch % 4 == 2)
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

    u08 currentMonth   = 0;
    i64 currMonthStart = currYearStart;
    for(u16 i = 0; i < 365; ++i)
    {
        if(currYearStart + i * 24 * 3600 > set->now)
        {
            break;
        }

        u08 daysThisMonth = daysInMonth(currentMonth, set->yearsEpoch % 4 == 2);

        i += daysThisMonth;
        if(currMonthStart + daysThisMonth * 24 * 3600 > set->now)
        {
            continue;
        }
        ++currentMonth;
        currMonthStart += daysThisMonth * 24 * 3600;
    }

    u08 weekday365 = (3 + (u08)(daysEpoch - 364)) % 7;
    if(set->yearsEpoch % 4 == 2)
    {
        ++weekday365;
        weekday365 %= 7;
    }
    i64 dayEnd       = set->now - set->now % (24 * 3600) + 24 * 3600;
    u08 dayOfMonth = (u08)((dayEnd - currMonthStart) / (24 * 3600));

    set->currentMonth = currentMonth;
    set->weekday365   = weekday365;
    set->day_of_month = dayOfMonth;
    set->leapYear     = set->yearsEpoch % 4 == 2;
    set->now          = set->now + 365 * 24 * 3600;
    if(set->leapYear)
    {
        set->now += 24 * 3600;
    }

    if(silent)
    {
        return;
    }

    GF_DEBUG("\n");
    GF_DEBUG("full days since epoch:   %"PRIu64, daysEpoch);
    GF_DEBUG("full years since epoch:  %"PRIu32, set->yearsEpoch);
    GF_DEBUG("now, unix time:          %"PRIu64, set->now);
    GF_DEBUG("current year start:      %"PRIu64, currYearStart);
    GF_DEBUG("current month:           %"PRIu32, currentMonth);
    GF_DEBUG("current month start:     %"PRIu64, currMonthStart);
    GF_DEBUG("weekday 365 days ago:    %s",      days[weekday365]);
    GF_DEBUG("day of the month, today: %"PRIu8,  dayOfMonth);
    GF_DEBUG("\n");
}

f_internal void displayData
(
    gfConf            *config,
    gfDisplaySettings *set
){
    i64 daysCommit = (set->heatSet->now - set->oldestCommitTime) / (24 * 3600);

    u32 max       = 0;
    u32 maxday    = 0;
    u32 zerocount = 0;

    for(u32 i = 0; i < 365; ++i)
    {
        for(u32 j = 0; j < 365 - i; ++j)
        {
            if(set->sorted[j] > max)
            {
                max    = set->sorted[j];
                maxday = j;
            }

            if(set->sorted[j] > set->sorted[j + 1])
            {
                u32 tmp = set->sorted[j];
                set->sorted[j]     = set->sorted[j + 1];
                set->sorted[j + 1] = tmp;
            }
        }
    }

    for(u16 i = 0; i < 365 && !set->sorted[i]; ++i)
    {
        ++zerocount;
    }

    u16 d20Sep = (u16)((f32)(365 - zerocount) * 0.20f);
    u16 d50Sep = (u16)((f32)(365 - zerocount) * 0.50f);
    u16 d70Sep = (u16)((f32)(365 - zerocount) * 0.70f);
    u16 d90Sep = (u16)((f32)(365 - zerocount) * 0.90f);

    gfPercentiles percentiles = {0};
    percentiles.d20 = set->sorted[zerocount + d20Sep];
    percentiles.d50 = set->sorted[zerocount + d50Sep];
    percentiles.d70 = set->sorted[zerocount + d70Sep];
    percentiles.d90 = set->sorted[zerocount + d90Sep];

    GF_DEBUG("\n");
    GF_DEBUG("found 0-days:  %"PRIu32, zerocount);
    GF_DEBUG("found d20_sep: %"PRIu32, d20Sep);
    GF_DEBUG("found d50_sep: %"PRIu32, d50Sep);
    GF_DEBUG("found d70_sep: %"PRIu32, d70Sep);
    GF_DEBUG("found d90_sep: %"PRIu32, d90Sep);
    GF_DEBUG("found d20: %"PRIu32, percentiles.d20);
    GF_DEBUG("found d50: %"PRIu32, percentiles.d50);
    GF_DEBUG("found d70: %"PRIu32, percentiles.d70);
    GF_DEBUG("found d90: %"PRIu32, percentiles.d90);
    GF_DEBUG("\n");

    i64 yearStart   = set->heatSet->now;
    i64 currentYear = 0;
    for(u08 i = 0; i < config->years; ++i)
    {
        currentYear = 1970 + (yearStart / (365 * 24 * 3600));
        yearStart -= 365 * 24 * 3600;
        GF_DEBUG("calculating year frame [%"PRIi64"-%"PRIi64"] @ start %"PRIi64"",
                 currentYear - 1, currentYear, yearStart);

        if((currentYear + 1) % 4 == 2)
        {
            yearStart -= 24 * 3600;
        }
        --currentYear;
    }

    gfHeatmapSettings heatSet = *set->heatSet;
    heatSet.config      = config;
    heatSet.percentiles = &percentiles;
    heatSet.now         = yearStart;
    heatSet.startYear   = config->startYear;
    heatSet.endYear     = config->endYear;
    calculateHeatmap(&heatSet, 1);

    #ifdef DEBUG
        printf("\npalette: ");
        printCorrespondingHeat(config, &percentiles, 1);
        printCorrespondingHeat(config, &percentiles, percentiles.d20);
        printCorrespondingHeat(config, &percentiles, percentiles.d50);
        printCorrespondingHeat(config, &percentiles, percentiles.d70);
        printCorrespondingHeat(config, &percentiles, percentiles.d90);
        printf("\n");
    #endif

    u32 currentStreak      = 0;
    u08 brokeCurrentStreak = 0;

    u32 streak        = 0;
    u32 longestStreak = 0;

    for(u32 i = 0; i < MAX_DAYS; ++i)
    {
        if(!brokeCurrentStreak && !set->heatSet->heatmap[i])
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

        if(!set->heatSet->heatmap[MAX_DAYS - i - 1])
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

    if(config->flags & GF_FLAG_INFO)
    {
        f32 percentage = 100.0f * (f32)set->personalCommitCount /
                         (f32)set->totalCommitCount;

        printf("\n%"PRIu64" commits analysed across %"PRIu32" repositories.\n",
               set->totalCommitCount, set->repositoryCount);
        printf("%"PRIu64" were matched with a provided author (%.2f%%).\n",
               set->personalCommitCount, percentage);

        printf("most commits in the last 365 days (%"PRIu32") made %"PRIu32
               " days ago.\n", max, maxday);
        if(set->repositoryCount > 1)
        {
            printf("most commits in single repository (%"PRIu32") in '"PRI_SV"'.\n\n",
                   set->repoMax, ARG_SV(set->biggestRepo));
        }

        printf("time since first commit: %lu days\n\n", daysCommit);
    }

    if(config->flags & GF_FLAG_STREAK)
    {
        printf("longest streak: %"PRIu32" days\n", longestStreak);
        printf("current streak: %"PRIu32" days\n", currentStreak);
        printf("commits today:  %"PRIu32" ", set->commitsToday);
        printCorrespondingHeat(config, &percentiles, set->commitsToday);
        printf("\n\n");
    }

    if(!config->years)
    {
        printHeatMap(&heatSet, 0);
        return;
    }

    for(u08 i = 0; i < config->years; ++i)
    {
        u32 currPrintYear = 1970 + heatSet.yearsEpoch;
        if(currPrintYear < 1971)
        {
            GF_WARN("skipping year frame [%u] because you could not possibly have git "
                    "commits older than the epoch.", i);
            calculateHeatmap(&heatSet, 1);
            continue;
        }
        else if(currPrintYear < heatSet.startYear ||
                currPrintYear > heatSet.endYear + 1
        ){
            calculateHeatmap(&heatSet, 1);
            continue;
        }

        printf("[%u - %u]\n", currPrintYear - 1, currPrintYear);

        calculateHeatmap(&heatSet, 0);
        printHeatMap(&heatSet, config->years - i - 1);
    }
}

void sortStrings
(
    gfConf *config
){
    pdSVSortByDelim(config->authors, ';', config->sortedAuthors);
    if(config->authors.data)
    {
        free((void*)config->authors.data);
    }
    config->authors = pdCstrSV(config->sortedAuthors);

    pdSVSortByDelim(config->repositories, ';', config->sortedRepos);
    if(config->repositories.data)
    {
        free((void*)config->repositories.data);
    }
    config->repositories = pdCstrSV(config->sortedRepos);

    GF_DEBUG("\nfinal, sorted author list:\n"PRI_SV"", ARG_SV(config->authors));
    GF_DEBUG("\nfinal, sorted paths:\n"PRI_SV"\n", ARG_SV(config->repositories));
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

    u64 now = gfQueryMonotonic();

    char sortedRepos[MAX_PATH * 128];
    char sortedAuthors[MAX_PATH * 4];
    char biggestRepoBuf[MAX_PATH + 1];

    u32 heatmap[MAX_DAYS] = {0};
    u32 sorted[366]       = {0};

    gfConf config = {0};
    config.sortedRepos   = sortedRepos;
    config.sortedAuthors = sortedAuthors;

    gfReadConfig(&config);
    if(argc > 1)
    {
        gfReadArgs(argc, argv, &config);
    }
    // TESTING: manual set
    config.startYear = 1979;
    config.endYear   = 2026;

    sortStrings(&config);

    if(!config.authors.data || pdSVSame(config.authors, pdCstrSV("")))
    {
        StringView anyIdent = pdCstrSV("any");
        gfAddAuthor(&config, anyIdent);
    }

    if(!config.repositories.data || pdSVSame(config.repositories, pdCstrSV("")))
    {
        StringView fallback = pdCstrSV(".");
        gfAddPath(&config, fallback);
    }

    i64 timezoneOffset = gfQueryTimezoneOffset();
    GF_DEBUG("queried timezoneOffset: %"PRIi64, timezoneOffset);

    gfHeatmapSettings heatSet = {0};
    heatSet.heatmap = heatmap;
    heatSet.now     = gfQueryTime();

    i64 nowAdjusted = heatSet.now + timezoneOffset;

    gfDisplaySettings set = {0};
    set.heatSet           = &heatSet;
    set.sorted            = sorted;
    set.biggestRepoBuf    = biggestRepoBuf;
    set.oldestCommitTime  = I64MAX;
    set.repositoryCount   = pdSVCountByDelim(config.repositories, ';');
    set.currDayEnd        = (heatSet.now - nowAdjusted % (24 * 3600) + 24 * 3600);

    GF_DEBUG("currDayEnd: %"PRIi64, set.currDayEnd);

    #ifdef BUILD_WINDOWS
        InitializeSRWLock(set.lock);
    #endif

    u64 gfInitTime = gfQueryMonotonic() - now;
    now = gfQueryMonotonic();

    // TESTING: set start & end time manually
    // set.endYearTime = 430224334;
    set.endYearTime = heatSet.now;

    gatherData(&config, &set);

    u64 gathering = gfQueryMonotonic() - now;
    now = gfQueryMonotonic();

    displayData(&config, &set);

    u64 printing = gfQueryMonotonic() - now;

    if(config.flags & GF_FLAG_PROFILE)
    {
        f32 printMS   = (f32)printing      / 1e6f;
        f32 gfinitMS  = (f32)gfInitTime    / 1e6f;
        f32 gatherMS  = (f32)gathering     / 1e6f;
        f32 perCommit = (gatherMS + printMS) / (f32)set.totalCommitCount;

        printf("gitfluss init: %10.5f ms\n", gfinitMS);
        printf("gather time:   %10.5f ms\n", gatherMS);
        printf("print time:    %10.5f ms\n", printMS);
        printf("per commit:    %10.5f ms\n", perCommit);
        printf("effective:     %10.0f commits/s\n\n",
               (f32)set.totalCommitCount / (gatherMS + printMS) * 1e3);
    }

    if(config.character)
    {
        free((void*)config.character);
    }
}
