#include "gitfluss.h"

#include "pd_path.h"
#include "pd_print_macros.h"

f_internal void printHelp
(
    void
){
    printf("gitfluss usage:\n");
    printf("\t--author [author@mailcorp.com]\n");
    printf("\t\tadd author (by email) to whitelist of authors to filter by.\n");
    printf("\t\t'--author any' will not filter out any authors.\n");
    printf("\t--authorlist [~/authorlist]\n");
    printf("\t\tadd all lines of file as authors to whitelist, like above.\n");
    printf("\t--char [x]\n");
    printf("\t\tchange the character inside the heatmap to x.\n");
    printf("\t--colour [red/green/blue/cyan/yellow/purple/pink/white]\n");
    printf("\t\tchange colour of heatmap display.\n");
    printf("\t--help\n");
    printf("\t\tshow this help menu, then exit.\n");
    printf("\t--info\n");
    printf("\t\tshow general information about collected data.\n");
    printf("\t\t'--noinfo' will negate 'info: true' from the config file.\n");
    printf("\t--mono\n");
    printf("\t\tprint monochrome heatmap.\n");
    printf("\t\twith --heat[0-4] [x], you can change the displays for\n");
    printf("\t\theats 0 through 4 to, analogous to --char.\n");
    printf("\t--profile\n");
    printf("\t\tprint timings and effective speed.\n");
    printf("\t\t'--noprofile' will negate 'profile: true' from the config file.\n");
    printf("\t--repolist [~/repolist]\n");
    printf("\t\tadd all lines of file as repositories to paths to scan.\n");
    printf("\t--streak\n");
    printf("\t\tprint longest & current streak.\n");
    printf("\t\t'--nostreak' will negate 'streak: true' from the config file.\n");
    printf("\t--summary\n");
    printf("\t\tprint commit summaries.\n");
    printf("\t\t'--nosummary' will negate 'summary: true' from the config file.\n");
    printf("\t--version\n");
    printf("\t\tshow version number & license, then exit.\n");
    printf("\t--years [n]\n");
    printf("\t\tprint n heatmaps for t-n years back\n");
    printf("\t\tn = 0 will still print 1 year, but without the interval header.\n");
}

f_internal void printVersion
(
    void
){
    printf("gitfluss v0.9.4\n");
    printf("Copyright (C) 2026 BadAcronym.\n");
    printf("Licensed under GPLv3: ");
    printf("https://www.gnu.org/licenses/gpl-3.0.en.html\n");
    printf("This is open-source software; ");
    printf("you are free to change and redistribute it.\n\n");
}

f_internal void setColour
(
    gfConf     *config,
    StringView colour
){
    StringView red_sv    = pdCstrSV("red");
    StringView green_sv  = pdCstrSV("green");
    StringView blue_sv   = pdCstrSV("blue");
    StringView cyan_sv   = pdCstrSV("cyan");
    StringView purple_sv = pdCstrSV("purple");
    StringView pink_sv   = pdCstrSV("pink");
    StringView yellow_sv = pdCstrSV("yellow");
    StringView white_sv  = pdCstrSV("white");

    if(pdSVSame(colour, red_sv))
    {
        config->colour = RED;
    }
    else if(pdSVSame(colour, green_sv))
    {
        config->colour = GREEN;
    }
    else if(pdSVSame(colour, blue_sv))
    {
        config->colour = BLUE;
    }
    else if(pdSVSame(colour, cyan_sv))
    {
        config->colour = CYAN;
    }
    else if(pdSVSame(colour, purple_sv))
    {
        config->colour = PURPLE;
    }
    else if(pdSVSame(colour, pink_sv))
    {
        config->colour = PINK;
    }
    else if(pdSVSame(colour, yellow_sv))
    {
        config->colour = YELLOW;
    }
    else if(pdSVSame(colour, white_sv))
    {
        config->colour = WHITE;
    }

    PD_DEBUG("detected colour: "PRI_SV"", ARG_SV(colour));
}

void gfAddAuthor
(
    gfConf     *config,
    StringView author
){
    if(!author.size)
    {
        return;
    }

    StringView sep = pdCstrSV(";");

    if(config->authors.data && config->authors.size)
    {
        char *authors_cstr = malloc(config->authors.size + author.size + 2);
        pdSVConcat(config->authors, sep, authors_cstr);
        free((void*)config->authors.data);
        config->authors = pdCstrSV(authors_cstr);

        pdSVConcat(config->authors, author, authors_cstr);
        config->authors = pdCstrSV(authors_cstr);
    }
    else
    {
        char *new_buf = malloc(GF_BUFSIZE);
        char author_cstr[512];
        pdSVCstr(author, author_cstr);
        config->authors = pdCstrSVCpy(author_cstr, new_buf);
    }

    PD_TRACE("author list: "PRI_SV"", ARG_SV(config->authors));
}

void gfAddAuthorlist
(
    gfConf     *config,
    StringView path
){
    char path_cstr[MAX_PATH];
    pdSVCstr(path, path_cstr);

    char path_expanded_cstr[4096];
    pdExpandPath(path, path_expanded_cstr);

    StringView path_expanded = pdCstrSV(path_expanded_cstr);

    if(pdVerifyPath(path_expanded) != PD_TYPE_FILE)
    {
        PD_WARN("tasked with opening author list file: '"PRI_SV"', no such file "
                "exists.", ARG_SV(path_expanded));
        return;
    }

    FILE *file = fopen(path_expanded_cstr, "r");
    if(!file)
    {
        PD_WARN("tasked with opening author list file: '%s' , failed to open.",
                path_expanded_cstr);
        return;
    }

    char buf[GF_BUFSIZE];
    while(fgets(buf, GF_BUFSIZE, file))
    {
        StringView buffer = pdCstrSV(buf);

        StringView comment_sv  = pdCstrSV("//");
        const char *commentloc = pdSVFind(comment_sv, buffer);
        if(commentloc == buf)
        {
            continue;
        }

        gfAddAuthor(config, buffer);
    }

    fclose(file);
}

f_internal void verifyDirectory
(
    StringView resolved
){
    uint8_t result = pdVerifyPath(resolved);
    if(result == PD_TYPE_FILE)
    {
        PD_WARN("path '"PRI_SV"' is a file, not a directory.", ARG_SV(resolved));
        return;
    }
    else if(result == PD_TYPE_ERROR || result == PD_TYPE_OTHER)
    {
        StringView dashes = pdCstrSV("--");
        if(pdSVFind(dashes, resolved) == resolved.data)
        {
            return;
        }

        PD_WARN("path '"PRI_SV"' does not exist. Ignoring...", ARG_SV(resolved));
    }
}

void gfAddPath
(
    gfConf     *config,
    StringView path
){
    if(!path.size)
    {
        return;
    }

    char path_cstr[path.size + 1];
    pdSVCstr(path, path_cstr);

    StringView sep = pdCstrSV(";");

    if(config->repositories.data && config->repositories.size)
    {
        char *repositories_cstr = malloc(config->repositories.size + MAX_PATH + 2);
        pdSVConcat(config->repositories, sep, repositories_cstr);
        free((void*)config->repositories.data);
        config->repositories = pdCstrSV(repositories_cstr);

        char resolved_cstr[MAX_PATH];
        pdExpandPath(path, resolved_cstr);
        StringView resolved = pdCstrSV(resolved_cstr);

        char pathSep[resolved.size + 2];
        StringView pathComp = pdCstrSV(resolved_cstr);
        pathSep[resolved.size] = ';';
        pathSep[resolved.size + 1] = '\0';
        if(pdSVFind(pathComp, config->repositories))
        {
            PD_DEBUG("path '"PRI_SV"' already in repository list. Ignoring "
                     "duplicate...", ARG_SV(resolved));
            return;
        }

        verifyDirectory(resolved);

        pdSVConcat(config->repositories, resolved, repositories_cstr);
        config->repositories = pdCstrSV(repositories_cstr);
    }
    else
    {
        char *resolved_cstr = calloc(MAX_PATH, 1);
        pdExpandPath(path, resolved_cstr);
        StringView resolved = pdCstrSV(resolved_cstr);
        config->repositories = resolved;

        verifyDirectory(resolved);
    }

    PD_TRACE("path list: "PRI_SV"", ARG_SV(config->repositories));
}

void gfAddPathlist
(
    gfConf     *config,
    StringView path
){
    char path_expanded_cstr[4096];
    pdExpandPath(path, path_expanded_cstr);

    StringView path_expanded = pdCstrSV(path_expanded_cstr);

    if(pdVerifyPath(path_expanded) != PD_TYPE_FILE)
    {
        PD_WARN("tasked with opening path list file: '"PRI_SV"', no such file exists.",
                ARG_SV(path_expanded));
        return;
    }

    FILE *file = fopen(path_expanded_cstr, "r");
    if(!file)
    {
        PD_DEBUG("tasked with opening path list file: '"PRI_SV"', failed to open.",
                 ARG_SV(path_expanded));
        return;
    }

    char buf[GF_BUFSIZE];
    while(fgets(buf, GF_BUFSIZE, file))
    {
        StringView buffer = pdCstrSV(buf);

        StringView comment_sv  = pdCstrSV("//");
        const char *commentloc = pdSVFind(comment_sv, buffer);
        if(commentloc == buf)
        {
            continue;
        }

        gfAddPath(config, buffer);
    }

    fclose(file);
}

f_internal uint8_t parseYear
(
    const char *string
){
    uint8_t number = 0;
    for(uint8_t i = 0; string[i] != '\0' && string[i] != '\n'; ++i)
    {
        if(i > 2)
        {
            PD_WARN("trying to read too many digits into year: %u.", i);
            return number;
        }

        if(string[i] > 0x2F && string[i] < 0x3A)
        {
            number *= 10;
            number += (string[i] - 0x30);
        }
        else
        {
            PD_WARN("character '%c' is not a digit. Ignoring...", string[i]);
        }
    }

    return number;
}

void gfReadConfig
(
    gfConf *config
){
    StringView conf     = pdCstrSV(CONF_PATH);
    StringView fallback = pdCstrSV(CONF_FALLBACK);

    char path_expanded[MAX_PATH];
    char fallback_expanded[MAX_PATH];
    pdExpandPath(conf, path_expanded);
    pdExpandPath(fallback, fallback_expanded);

    StringView authorlist_sv = pdCstrSV("authorlist:");
    StringView repolist_sv   = pdCstrSV("repolist:");

    StringView author_sv  = pdCstrSV("author:");
    StringView colour_sv  = pdCstrSV("colour:");
    StringView info_sv    = pdCstrSV("info:");
    StringView mono_sv    = pdCstrSV("mono:");
    StringView profile_sv = pdCstrSV("profile:");
    StringView heat0_sv   = pdCstrSV("heat0:");
    StringView heat1_sv   = pdCstrSV("heat1:");
    StringView heat2_sv   = pdCstrSV("heat2:");
    StringView heat3_sv   = pdCstrSV("heat3:");
    StringView heat4_sv   = pdCstrSV("heat4:");
    StringView char_sv    = pdCstrSV("character:");
    StringView years_sv   = pdCstrSV("years:");
    StringView summary_sv = pdCstrSV("summary:");
    StringView streak_sv  = pdCstrSV("streak:");
    StringView true_sv    = pdCstrSV("true");

    FILE *file = fopen(path_expanded, "r");
    if(!file)
    {
        file = fopen(fallback_expanded, "r");
        if(!file)
        {
            return;
        }
    }

    char buf[GF_BUFSIZE];
    while(fgets(buf, GF_BUFSIZE, file))
    {
        StringView buffer;
        buffer.data = buf;
        buffer.size = GF_BUFSIZE;

        StringView comment_sv  = pdCstrSV("//");
        const char *commentloc = pdSVFind(comment_sv, buffer);
        if(commentloc == buf)
        {
            continue;
        }

        const char* authorlistloc = pdSVFind(authorlist_sv, buffer);
        if(authorlistloc)
        {
            StringView authorlist = pdCstrSV(buffer.data + authorlist_sv.size + 1);

            gfAddAuthorlist(config, authorlist);
            continue;
        }

        const char* repolistloc = pdSVFind(repolist_sv, buffer);
        if(repolistloc)
        {
            StringView repolist = pdCstrSV(buffer.data + repolist_sv.size + 1);

            gfAddPathlist(config, repolist);
            continue;
        }

        const char* authorloc = pdSVFind(author_sv, buffer);
        if(authorloc)
        {
            StringView author = pdCstrSV(buffer.data + author_sv.size + 1);

            gfAddAuthor(config, author);
            continue;
        }

        const char* colourloc = pdSVFind(colour_sv, buffer);
        if(colourloc)
        {
            StringView chosen_sv = pdCstrSV(buffer.data + colour_sv.size + 1);
            setColour(config, chosen_sv);
            continue;
        }

        const char* infoloc = pdSVFind(info_sv, buffer);
        if(infoloc)
        {
            StringView set_sv = pdCstrSV(buffer.data + info_sv.size + 1);

            if(pdSVSame(set_sv, true_sv))
            {
                config->flags |= GF_FLAG_INFO;
            }
            continue;
        }

        const char* monoloc = pdSVFind(mono_sv, buffer);
        if(monoloc)
        {
            StringView set_sv = pdCstrSV(buffer.data + mono_sv.size + 1);

            if(pdSVSame(set_sv, true_sv))
            {
                config->flags |= GF_FLAG_MONO;
            }
            continue;
        }

        const char* profileloc = pdSVFind(profile_sv, buffer);
        if(profileloc)
        {
            StringView set_sv = pdCstrSV(buffer.data + profile_sv.size + 1);

            if(pdSVSame(set_sv, true_sv))
            {
                config->flags |= GF_FLAG_PROFILE;
            }
            continue;
        }

        const char* heat0loc = pdSVFind(heat0_sv, buffer);
        if(heat0loc)
        {
            StringView set_sv = pdCstrSV(buffer.data + heat0_sv.size + 1);

            char *small_buf = malloc(8);
            pdSVCstr(set_sv, small_buf);
            config->mono0 = small_buf;
            continue;
        }

        const char* heat1loc = pdSVFind(heat1_sv, buffer);
        if(heat1loc)
        {
            StringView set_sv = pdCstrSV(buffer.data + heat1_sv.size + 1);

            char *small_buf = malloc(8);
            pdSVCstr(set_sv, small_buf);
            config->mono1 = small_buf;
            continue;
        }

        const char* heat2loc = pdSVFind(heat2_sv, buffer);
        if(heat2loc)
        {
            StringView set_sv = pdCstrSV(buffer.data + heat2_sv.size + 1);

            char *small_buf = malloc(8);
            pdSVCstr(set_sv, small_buf);
            config->mono2 = small_buf;
            continue;
        }

        const char* heat3loc = pdSVFind(heat3_sv, buffer);
        if(heat3loc)
        {
            StringView set_sv = pdCstrSV(buffer.data + heat3_sv.size + 1);

            char *small_buf = malloc(8);
            pdSVCstr(set_sv, small_buf);
            config->mono3 = small_buf;
            continue;
        }

        const char* heat4loc = pdSVFind(heat4_sv, buffer);
        if(heat4loc)
        {
            StringView set_sv = pdCstrSV(buffer.data + heat4_sv.size + 1);

            char *small_buf = malloc(8);
            pdSVCstr(set_sv, small_buf);
            config->mono4 = small_buf;
            continue;
        }

        const char* charloc = pdSVFind(char_sv, buffer);
        if(charloc)
        {
            StringView set_sv = pdCstrSV(buffer.data + char_sv.size + 1);

            char *small_buf = malloc(8);
            pdSVCstr(set_sv, small_buf);
            config->character = small_buf;
            continue;
        }

        const char* yearloc = pdSVFind(years_sv, buffer);
        if(yearloc)
        {
            config->years = parseYear(buffer.data + years_sv.size + 1);
            continue;
        }

        const char* summaryloc = pdSVFind(summary_sv, buffer);
        if(summaryloc)
        {
            StringView set_sv = pdCstrSV(buffer.data + summary_sv.size + 1);

            if(pdSVSame(set_sv, true_sv))
            {
                config->flags |= GF_FLAG_SUMMARY;
            }
            continue;
        }

        const char* streakloc = pdSVFind(streak_sv, buffer);
        if(streakloc)
        {
            StringView set_sv = pdCstrSV(buffer.data + streak_sv.size + 1);

            if(pdSVSame(set_sv, true_sv))
            {
                config->flags |= GF_FLAG_STREAK;
            }
            continue;
        }

        StringView path = pdCstrSV(buffer.data);
        gfAddPath(config, path);
    }

    fclose(file);
}

f_internal void printSpecMissing
(
    const char *arg
){
    PD_WARN("option '%s' requires a specified argument. Ignoring...", arg);
}

f_internal uint8_t checkIdentMissing
(
    uint16_t i,
    int      argc,
    char     **argv
){
    if(argc < i + 2 || (argv[i + 1][0] == '-' && argv[i + 1][1] == '-'))
    {
        printSpecMissing(argv[i]);
        return 1;
    }

    return 0;
}

void gfReadArgs
(
    int    argc,
    char   **argv,
    gfConf *config
){
    uint8_t reposRead   = 0;
    uint8_t authorsRead = 0;

    if(config->repositories.data)
    {
        reposRead = 1;
    }

    if(config->authors.data)
    {
        authorsRead = 1;
    }

    for(uint16_t i = 1; i < argc; ++i)
    {
        StringView arg = pdCstrSV(argv[i]);

        if(arg.size < 2 || arg.data[0] != '-' || arg.data[1] != '-')
        {
            goto isPath;
        }

        StringView authorlist_ident = pdCstrSV("authorlist");
        StringView repolist_ident   = pdCstrSV("repolist");
        StringView author_ident     = pdCstrSV("author");
        StringView colour_ident     = pdCstrSV("colour");
        StringView info_ident       = pdCstrSV("info");
        StringView noinfo_ident     = pdCstrSV("noinfo");
        StringView mono_ident       = pdCstrSV("mono");
        StringView profile_ident    = pdCstrSV("profile");
        StringView noprofile_ident  = pdCstrSV("noprofile");
        StringView heat0_ident      = pdCstrSV("heat0");
        StringView heat1_ident      = pdCstrSV("heat1");
        StringView heat2_ident      = pdCstrSV("heat2");
        StringView heat3_ident      = pdCstrSV("heat3");
        StringView heat4_ident      = pdCstrSV("heat4");
        StringView char_ident       = pdCstrSV("char");
        StringView years_ident      = pdCstrSV("years");
        StringView version_ident    = pdCstrSV("version");
        StringView nomatch_ident    = pdCstrSV("nomatch");
        StringView summary_ident    = pdCstrSV("summary");
        StringView nosummary_ident  = pdCstrSV("nosummary");
        StringView streak_ident     = pdCstrSV("streak");
        StringView nostreak_ident   = pdCstrSV("nostreak");
        StringView help_ident       = pdCstrSV("help");

        arg.size -= 2;
        arg.data += 2;

        if(pdSVSame(arg, authorlist_ident))
        {
            if(checkIdentMissing(i, argc, argv))
            {
                continue;
            }

            if(authorsRead)
            {
                authorsRead = 0;
                free((void*)config->authors.data);
                config->authors.size = 0;
            }

            gfAddAuthorlist(config, pdCstrSV(argv[i + 1]));
            ++i;
            continue;
        }
        else if(pdSVSame(arg, repolist_ident))
        {
            if(checkIdentMissing(i, argc, argv))
            {
                continue;
            }

            if(reposRead)
            {
                reposRead = 0;
                free((void*)config->repositories.data);
                config->repositories.size = 0;
            }

            gfAddPathlist(config, pdCstrSV(argv[i + 1]));
            ++i;
            continue;
        }
        else if(pdSVSame(arg, author_ident))
        {
            if(checkIdentMissing(i, argc, argv))
            {
                continue;
            }

            if(authorsRead)
            {
                authorsRead = 0;
                free((void*)config->authors.data);
                config->authors.size = 0;
            }

            StringView author = pdCstrSV(argv[i + 1]);
            gfAddAuthor(config, author);

            ++i;
            continue;
        }
        else if(pdSVSame(arg, colour_ident))
        {
            if(checkIdentMissing(i, argc, argv))
            {
                continue;
            }

            StringView colour = pdCstrSV(argv[i + 1]);
            setColour(config, colour);

            ++i;
            continue;
        }
        else if(pdSVSame(arg, info_ident))
        {
            config->flags |= GF_FLAG_INFO;
            continue;
        }
        else if(pdSVSame(arg, noinfo_ident))
        {
            config->flags &= ~GF_FLAG_INFO;
            continue;
        }
        else if(pdSVSame(arg, mono_ident))
        {
            config->flags |= GF_FLAG_MONO;
            continue;
        }
        else if(pdSVSame(arg, profile_ident))
        {
            config->flags |= GF_FLAG_PROFILE;
            continue;
        }
        else if(pdSVSame(arg, noprofile_ident))
        {
            config->flags &= ~GF_FLAG_PROFILE;
            continue;
        }
        else if(pdSVSame(arg, heat0_ident))
        {
            if(checkIdentMissing(i, argc, argv))
            {
                continue;
            }

            StringView chosen_sv  = pdCstrSV(argv[i + 1]);
            char       *small_buf = malloc(8);
            pdSVCstr(chosen_sv, small_buf);
            config->mono0 = small_buf;
            ++i;
            continue;
        }
        else if(pdSVSame(arg, heat1_ident))
        {
            if(checkIdentMissing(i, argc, argv))
            {
                continue;
            }

            StringView chosen_sv  = pdCstrSV(argv[i + 1]);
            char       *small_buf = malloc(8);
            pdSVCstr(chosen_sv, small_buf);
            config->mono1 = small_buf;
            ++i;
            continue;
        }
        else if(pdSVSame(arg, heat2_ident))
        {
            if(checkIdentMissing(i, argc, argv))
            {
                continue;
            }

            StringView chosen_sv  = pdCstrSV(argv[i + 1]);
            char       *small_buf = malloc(8);
            pdSVCstr(chosen_sv, small_buf);
            config->mono2 = small_buf;
            ++i;
            continue;
        }
        else if(pdSVSame(arg, heat3_ident))
        {
            if(checkIdentMissing(i, argc, argv))
            {
                continue;
            }

            StringView chosen_sv  = pdCstrSV(argv[i + 1]);
            char       *small_buf = malloc(8);
            pdSVCstr(chosen_sv, small_buf);
            config->mono3 = small_buf;
            ++i;
            continue;
        }
        else if(pdSVSame(arg, heat4_ident))
        {
            if(checkIdentMissing(i, argc, argv))
            {
                continue;
            }

            StringView chosen_sv  = pdCstrSV(argv[i + 1]);
            char       *small_buf = malloc(8);
            pdSVCstr(chosen_sv, small_buf);
            config->mono4 = small_buf;
            ++i;
            continue;
        }
        else if(pdSVSame(arg, char_ident))
        {
            if(checkIdentMissing(i, argc, argv))
            {
                continue;
            }

            StringView chosen_sv  = pdCstrSV(argv[i + 1]);
            char       *small_buf = malloc(8);
            pdSVCstr(chosen_sv, small_buf);
            config->character = small_buf;
            ++i;
            continue;
        }
        else if(pdSVSame(arg, years_ident))
        {
            if(checkIdentMissing(i, argc, argv))
            {
                continue;
            }

            config->years = parseYear(argv[i + 1]);
            ++i;
            continue;
        }
        else if(pdSVSame(arg, version_ident))
        {
            printVersion();
            exit(0);
        }
        else if(pdSVSame(arg, nomatch_ident))
        {
            config->flags |= GF_FLAG_NOMATCH;
            continue;
        }
        else if(pdSVSame(arg, summary_ident))
        {
            config->flags |= GF_FLAG_SUMMARY;
            continue;
        }
        else if(pdSVSame(arg, nosummary_ident))
        {
            config->flags &= ~GF_FLAG_SUMMARY;
            continue;
        }
        else if(pdSVSame(arg, streak_ident))
        {
            config->flags |= GF_FLAG_STREAK;
            continue;
        }
        else if(pdSVSame(arg, nostreak_ident))
        {
            config->flags &= ~GF_FLAG_STREAK;
            continue;
        }
        else if(pdSVSame(arg, help_ident))
        {
            printHelp();
            exit(0);
        }
        else
        {
            PD_ERROR("unknown option '"PRI_SV"'.", ARG_SV(arg));
            printHelp();
            exit(1);
        }

    isPath:
        if(reposRead)
        {
            reposRead = 0;
            free((void*)config->repositories.data);
            config->repositories.size = 0;
        }

        StringView path = pdCstrSV(argv[i]);
        gfAddPath(config, path);
    }
}
