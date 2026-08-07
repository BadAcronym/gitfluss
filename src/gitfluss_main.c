#include "gitfluss.h"
#include "pd_path.h"

#include <git2.h>

#include <stdint.h>
#include <stdio.h>

#define f_internal static

#ifdef BUILD_LINUX
    #define CONF_PATH     ".gitflussconf"
    #define CONF_FALLBACK "~/.config/gitfluss/.conf"
#endif
#ifdef BUILD_WINDOWS
    #include <fcntl.h>
    #include <io.h>
    #include <windows.h>

    #ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
    #define ENABLE_VIRTUAL_TERMINAL_PROCESSING  0x0004
    #endif

    #define CONF_PATH     "gitfluss.ini"
    #define CONF_FALLBACK "~\\.config\\gitfluss\\gitfluss.ini"
#endif

#define bufsize  8192
#define MAX_DAYS 16384
#define PATH_MAX 4096

#define RED    0
#define GREEN  1
#define BLUE   2
#define CYAN   3
#define PURPLE 4
#define PINK   5
#define YELLOW 6
#define WHITE  7

#define FLAG_INFO 0x01
#define FLAG_MONO 0x02

typedef struct gfConf
{
    StringView repositories;
    StringView authors;
    char       *sortedRepos;
    char       *sortedAuthors;
    const char *character;
    const char *mono0;
    const char *mono1;
    const char *mono2;
    const char *mono3;
    const char *mono4;
    uint8_t    percentile0;
    uint8_t    percentile1;
    uint8_t    percentile2;
    uint8_t    percentile3;
    uint8_t    colour;
    uint8_t    flags;
}
gfConf;

typedef struct gfPercentiles
{
    uint32_t d20;
    uint32_t d50;
    uint32_t d70;
    uint32_t d90;
}
gfPercentiles;

typedef struct gfHeatmapSettings
{
    gfConf        *config;
    gfPercentiles *percentiles;
    uint32_t      *heatmap;
    uint8_t       currentMonth;
    uint8_t       weekday365;
    uint8_t       day_of_month;
    uint8_t       leapYear;
}
gfHeatmapSettings;

typedef struct gfDisplaySettings
{
    StringView biggestRepo;
    uint32_t   repositoryCount;
    uint32_t   personalCommitCount;
    uint32_t   repoMax;
    uint32_t   *heatmap;
    uint32_t   *sorted;
    int64_t    now;
    int64_t    currDayEnd;
    int64_t    oldestCommitTime;
    char       *biggestRepoBuf;
}
gfDisplaySettings;

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

f_internal void addAuthor
(
    gfConf     *config,
    StringView author
){
    if(!author.size)
    {
        return;
    }

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

f_internal void addAuthorlist
(
    gfConf     *config,
    StringView path
){
    char path_cstr[4096];
    sv_cstr(path, path_cstr);

    char path_expanded_cstr[4096];
    pdExpandPath(path_cstr, path_expanded_cstr);

    StringView path_expanded = cstr_sv(path_expanded_cstr);

    if(pdVerifyPath(path_expanded) != PD_TYPE_FILE)
    {
        #ifdef DEBUG
        fprintf(stderr, "\033[33;1mWARNING: tasked with opening author list file: '"
                PRI_SV"', no such file exists.\033[0m\n", ARG_SV(path_expanded));
        #endif
        return;
    }

    FILE *file = fopen(path_expanded_cstr, "r");
    if(!file)
    {
        #ifdef DEBUG
        fprintf(stderr, "\033[33;1mWARNING: tasked with opening author list file: '"
                        PRI_SV"', failed to open.\033[0m\n", ARG_SV(path_expanded));
        #endif
        return;
    }

    char buf[bufsize];
    while(fgets(buf, bufsize, file))
    {
        StringView buffer = cstr_sv(buf);

        if(buf[buffer.size - 1] == '\n')
        {
            buffer.size -= 1;
        }

        StringView comment_sv  = cstr_sv("//");
        const char *commentloc = sv_find(comment_sv, buffer);
        if(commentloc == buf)
        {
            continue;
        }

        addAuthor(config, buffer);
    }

    fclose(file);
}

f_internal void addPath
(
    gfConf     *config,
    StringView path
){
    if(!path.size)
    {
        return;
    }

    char path_cstr[path.size + 1];
    sv_cstr(path, path_cstr);

    StringView sep = cstr_sv(";");

    if(config->repositories.data)
    {
        char *repositories_cstr = malloc(config->repositories.size + PATH_MAX + 2);
        sv_concat(config->repositories, sep, repositories_cstr);
        free((void*)config->repositories.data);
        config->repositories = cstr_sv(repositories_cstr);

        char resolved[PATH_MAX];
        pdExpandPath(path_cstr, resolved);
        StringView resolved_sv = cstr_sv(resolved);

        if(pdVerifyPath(resolved_sv) != PD_TYPE_DIRECTORY)
        {
            #ifdef DEBUG
            fprintf(stderr, "\033[33;3mWARNING: path '"PRI_SV"' is not a directory."
                            "\033[0m\n", ARG_SV(resolved_sv));
            #endif
            return;
        }

        sv_concat(config->repositories, resolved_sv, repositories_cstr);
        config->repositories = cstr_sv(repositories_cstr);
    }
    else
    {
        char *resolved = calloc(PATH_MAX, 1);
        pdExpandPath(path_cstr, resolved);
        StringView resolved_sv = cstr_sv(resolved);
        config->repositories = resolved_sv;

        if(pdVerifyPath(resolved_sv) != PD_TYPE_DIRECTORY)
        {
            #ifdef DEBUG
            fprintf(stderr, "\033[33;3mWARNING: path '"PRI_SV"' is not a directory."
                            "\033[0m\n", ARG_SV(resolved_sv));
            #endif
            return;
        }
    }

    #ifdef DEBUG
        fprintf(stderr, "path list: "PRI_SV"\n", ARG_SV(config->repositories));
    #endif
}

f_internal void addPathlist
(
    gfConf     *config,
    StringView path
){
    char path_cstr[4096];
    sv_cstr(path, path_cstr);

    char path_expanded_cstr[4096];
    pdExpandPath(path_cstr, path_expanded_cstr);

    StringView path_expanded = cstr_sv(path_expanded_cstr);

    if(pdVerifyPath(path_expanded) != PD_TYPE_FILE)
    {
        #ifdef DEBUG
        fprintf(stderr, "\033[33;1mWARNING: tasked with opening path list file: '"
                PRI_SV"', no such file exists.\033[0m\n", ARG_SV(path_expanded));
        #endif
        return;
    }

    FILE *file = fopen(path_expanded_cstr, "r");
    if(!file)
    {
        #ifdef DEBUG
        fprintf(stderr, "\033[33;1mWARNING: tasked with opening path list file: '"
                        PRI_SV"', failed to open.\033[0m\n", ARG_SV(path_expanded));
        #endif
        return;
    }

    char buf[bufsize];
    while(fgets(buf, bufsize, file))
    {
        StringView buffer = cstr_sv(buf);

        if(buf[buffer.size - 1] == '\n')
        {
            buffer.size -= 1;
        }

        StringView comment_sv  = cstr_sv("//");
        const char *commentloc = sv_find(comment_sv, buffer);
        if(commentloc == buf)
        {
            continue;
        }

        addPath(config, buffer);
    }

    fclose(file);
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
    StringView pink_sv   = cstr_sv("pink");
    StringView yellow_sv = cstr_sv("yellow");
    StringView white_sv  = cstr_sv("white");

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
    else if(sv_same(colour, pink_sv))
    {
        config->colour = PINK;
    }
    else if(sv_same(colour, yellow_sv))
    {
        config->colour = YELLOW;
    }
    else if(sv_same(colour, white_sv))
    {
        config->colour = WHITE;
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
    pdExpandPath(CONF_PATH, path_expanded);
    pdExpandPath(CONF_FALLBACK, fallback_expanded);

    StringView authorlist_sv = cstr_sv("authorlist:");
    StringView repolist_sv   = cstr_sv("repolist:");

    StringView author_sv = cstr_sv("author:");
    StringView colour_sv = cstr_sv("colour:");
    StringView info_sv   = cstr_sv("info:");
    StringView mono_sv   = cstr_sv("mono:");
    StringView heat0_sv  = cstr_sv("heat0:");
    StringView heat1_sv  = cstr_sv("heat1:");
    StringView heat2_sv  = cstr_sv("heat2:");
    StringView heat3_sv  = cstr_sv("heat3:");
    StringView heat4_sv  = cstr_sv("heat4:");
    StringView char_sv   = cstr_sv("character:");
    StringView true_sv   = cstr_sv("true");

    FILE *file = fopen(path_expanded, "r");
    if(!file)
    {
        file = fopen(fallback_expanded, "r");
        if(!file)
        {
            fprintf(stderr, "\033[33;3mWARNING: could not open configuration file.\n"
                    "Please pass your arguments via the cmdline.\033[0m\n");
            return;
        }
    }

    char buf[bufsize];
    while(fgets(buf, bufsize, file))
    {
        StringView buffer;
        buffer.data = buf;
        buffer.size = bufsize;

        StringView comment_sv  = cstr_sv("//");
        const char *commentloc = sv_find(comment_sv, buffer);
        if(commentloc == buf)
        {
            continue;
        }

        const char* authorlistloc = sv_find(authorlist_sv, buffer);
        if(authorlistloc)
        {
            StringView authorlist = cstr_sv(buffer.data + authorlist_sv.size + 1);
            if(authorlist.size)
            {
                authorlist.size -= 1;
            }

            addAuthorlist(config, authorlist);
            continue;
        }

        const char* repolistloc = sv_find(repolist_sv, buffer);
        if(repolistloc)
        {
            StringView repolist = cstr_sv(buffer.data + repolist_sv.size + 1);
            if(repolist.size)
            {
                repolist.size -= 1;
            }

            addPathlist(config, repolist);
            continue;
        }

        const char* authorloc = sv_find(author_sv, buffer);
        if(authorloc)
        {
            StringView author = cstr_sv(buffer.data + author_sv.size + 1);
            if(author.size)
            {
                author.size -= 1;
            }

            addAuthor(config, author);
            continue;
        }

        const char* colourloc = sv_find(colour_sv, buffer);
        if(colourloc)
        {
            StringView chosen_sv = cstr_sv(buffer.data + colour_sv.size + 1);
            if(chosen_sv.size)
            {
                chosen_sv.size -= 1;
            }
            setColour(config, chosen_sv);
            continue;
        }

        const char* infoloc = sv_find(info_sv, buffer);
        if(infoloc)
        {
            StringView set_sv = cstr_sv(buffer.data + info_sv.size + 1);
            if(set_sv.size)
            {
                set_sv.size -= 1;
            }

            if(sv_same(set_sv, true_sv))
            {
                config->flags |= FLAG_INFO;
            }
            continue;
        }

        const char* monoloc = sv_find(mono_sv, buffer);
        if(monoloc)
        {
            StringView set_sv = cstr_sv(buffer.data + mono_sv.size + 1);
            if(set_sv.size)
            {
                set_sv.size -= 1;
            }

            if(sv_same(set_sv, true_sv))
            {
                config->flags |= FLAG_MONO;
            }
            continue;
        }

        const char* heat0loc = sv_find(heat0_sv, buffer);
        if(heat0loc)
        {
            StringView set_sv = cstr_sv(buffer.data + heat0_sv.size + 1);
            if(set_sv.size)
            {
                set_sv.size -= 1;
            }

            char *small_buf = malloc(8);
            sv_cstr(set_sv, small_buf);
            config->mono0 = small_buf;
            continue;
        }

        const char* heat1loc = sv_find(heat1_sv, buffer);
        if(heat1loc)
        {
            StringView set_sv = cstr_sv(buffer.data + heat1_sv.size + 1);
            if(set_sv.size)
            {
                set_sv.size -= 1;
            }

            char *small_buf = malloc(8);
            sv_cstr(set_sv, small_buf);
            config->mono1 = small_buf;
            continue;
        }

        const char* heat2loc = sv_find(heat2_sv, buffer);
        if(heat2loc)
        {
            StringView set_sv = cstr_sv(buffer.data + heat2_sv.size + 1);
            if(set_sv.size)
            {
                set_sv.size -= 1;
            }

            char *small_buf = malloc(8);
            sv_cstr(set_sv, small_buf);
            config->mono2 = small_buf;
            continue;
        }

        const char* heat3loc = sv_find(heat3_sv, buffer);
        if(heat3loc)
        {
            StringView set_sv = cstr_sv(buffer.data + heat3_sv.size + 1);
            if(set_sv.size)
            {
                set_sv.size -= 1;
            }

            char *small_buf = malloc(8);
            sv_cstr(set_sv, small_buf);
            config->mono3 = small_buf;
            continue;
        }

        const char* heat4loc = sv_find(heat4_sv, buffer);
        if(heat4loc)
        {
            StringView set_sv = cstr_sv(buffer.data + heat4_sv.size + 1);
            if(set_sv.size)
            {
                set_sv.size -= 1;
            }

            char *small_buf = malloc(8);
            sv_cstr(set_sv, small_buf);
            config->mono4 = small_buf;
            continue;
        }

        const char* charloc = sv_find(char_sv, buffer);
        if(charloc)
        {
            StringView set_sv = cstr_sv(buffer.data + char_sv.size + 1);
            if(set_sv.size)
            {
                set_sv.size -= 1;
            }

            char *small_buf = malloc(8);
            sv_cstr(set_sv, small_buf);
            config->character = small_buf;
            continue;
        }

        StringView path = cstr_sv(buffer.data);
        if(path.size)
        {
            path.size -= 1;
        }
        addPath(config, path);
    }

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
        StringView mono_ident   = cstr_sv("--mono");
        StringView heat0_ident  = cstr_sv("--heat0");
        StringView heat1_ident  = cstr_sv("--heat1");
        StringView heat2_ident  = cstr_sv("--heat2");
        StringView heat3_ident  = cstr_sv("--heat3");
        StringView heat4_ident  = cstr_sv("--heat4");
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
            config->flags |= FLAG_INFO;
        }
        else if(sv_same(arg, mono_ident))
        {
            config->flags |= FLAG_MONO;
        }
        else if(sv_same(arg, heat0_ident) && i + 1 < argc)
        {
            StringView chosen_sv  = cstr_sv(argv[i + 1]);
            char       *small_buf = malloc(8);
            sv_cstr(chosen_sv, small_buf);
            config->mono0 = small_buf;
            ++i;
        }
        else if(sv_same(arg, heat1_ident) && i + 1 < argc)
        {
            StringView chosen_sv  = cstr_sv(argv[i + 1]);
            char       *small_buf = malloc(8);
            sv_cstr(chosen_sv, small_buf);
            config->mono1 = small_buf;
            ++i;
        }
        else if(sv_same(arg, heat2_ident) && i + 1 < argc)
        {
            StringView chosen_sv  = cstr_sv(argv[i + 1]);
            char       *small_buf = malloc(8);
            sv_cstr(chosen_sv, small_buf);
            config->mono2 = small_buf;
            ++i;
        }
        else if(sv_same(arg, heat3_ident) && i + 1 < argc)
        {
            StringView chosen_sv  = cstr_sv(argv[i + 1]);
            char       *small_buf = malloc(8);
            sv_cstr(chosen_sv, small_buf);
            config->mono3 = small_buf;
            ++i;
        }
        else if(sv_same(arg, heat4_ident) && i + 1 < argc)
        {
            StringView chosen_sv  = cstr_sv(argv[i + 1]);
            char       *small_buf = malloc(8);
            sv_cstr(chosen_sv, small_buf);
            config->mono4 = small_buf;
            ++i;
        }
        else if(sv_same(arg, char_ident) && i + 1 < argc)
        {
            StringView chosen_sv  = cstr_sv(argv[i + 1]);
            char       *small_buf = malloc(8);
            sv_cstr(chosen_sv, small_buf);
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

f_internal void gatherData
(
    gfConf            *config,
    gfDisplaySettings *set
){
    git_libgit2_init();

    StringView any_sv      = cstr_sv("any");
    uint32_t   authorcount = sv_count_by_delim(config->authors, ';');
    StringView authorlist[authorcount];

    sv_separate_by_delim(config->authors, authorlist, ';');

    #ifdef DEBUG
    uint64_t commitCount = 0;
    #endif

    for(uint32_t i = 0; i < set->repositoryCount; ++i)
    {
        git_repository *repo    = 0;
        git_revwalk    *revwalk = 0;
        git_oid        oid      = {0};

        StringView repository      = sv_find_by_delim(config->repositories, ';', i);
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

        uint8_t    any_author = 0;
        git_commit *commit    = 0;

        while(!git_revwalk_next(&oid, revwalk))
        {
            #ifdef DEBUG
            ++commitCount;
            #endif
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
                else if(sv_same(authorlist[j], any_sv))
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

            git_commit_free(commit);
        }

        if(repoCommitCount > set->repoMax)
        {
            set->biggestRepo = cstr_sv_cpy(current_repo_cstr, set->biggestRepoBuf);
            set->repoMax     = repoCommitCount;
        }

        git_revwalk_free(revwalk);
        git_repository_free(repo);
    }

    #ifdef DEBUG
        printf("\ntotal scanned commits: %lu\n\n", commitCount);
    #endif
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

    if(config->flags & FLAG_INFO)
    {
        printf("most commits in the last 365 days (%u) made %u days ago.\n",
               max, maxday);
        printf("most commits in single repository (%u) in '"PRI_SV"'.\n\n",
               set->repoMax, ARG_SV(set->biggestRepo));
    }

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
        printf("time since first commit: %lu days\n\n", daysCommit);
        printf("longest streak: %u days\n", longestStreak);
        printf("current streak: %u days\n", currentStreak);
        printf("commits today: %u ", set->heatmap[0]);
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

    char sortedRepos[PATH_MAX * 100];
    char sortedAuthors[PATH_MAX * 2];
    char biggestRepoBuf[PATH_MAX + 1];

    uint32_t heatmap[MAX_DAYS] = {0};
    uint32_t sorted[366]       = {0};

    gfConf config = {0};
    config.sortedRepos   = sortedRepos;
    config.sortedAuthors = sortedAuthors;

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

    if(!config.repositories.data)
    {
        StringView fallback = cstr_sv(".");
        addPath(&config, fallback);
    }

    gfDisplaySettings set = {0};
    set.heatmap          = heatmap;
    set.sorted           = sorted;
    set.biggestRepoBuf   = biggestRepoBuf;
    set.oldestCommitTime = INT64_MAX;
    set.repositoryCount  = sv_count_by_delim(config.repositories, ';');
    set.now              = gfQueryTime();
    set.currDayEnd       = set.now - set.now % (24 * 3600) + 24 * 3600;

    gatherData(&config,  &set);
    displayData(&config, &set);

    if(config.character)
    {
        free((void*)config.character);
    }
    return git_libgit2_shutdown();
}
