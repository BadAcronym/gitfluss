#include "gitfluss.h"
#include "gf_print_macros.h"

#include "pd_path.h"

s_global StringView authorlistIdent = { .data = "authorlist", .size = 0 };
s_global StringView repolistIdent   = { .data = "repolist",   .size = 8 };
s_global StringView authorIdent     = { .data = "author",     .size = 6 };
s_global StringView colourIdent     = { .data = "colour",     .size = 6 };
s_global StringView infoIdent       = { .data = "info",       .size = 4 };
s_global StringView noinfoIdent     = { .data = "noinfo",     .size = 6 };
s_global StringView monoIdent       = { .data = "mono",       .size = 4 };
s_global StringView profileIdent    = { .data = "profile",    .size = 7 };
s_global StringView noprofileIdent  = { .data = "noprofile",  .size = 9 };
s_global StringView heat0Ident      = { .data = "heat0",      .size = 5 };
s_global StringView heat1Ident      = { .data = "heat1",      .size = 5 };
s_global StringView heat2Ident      = { .data = "heat2",      .size = 5 };
s_global StringView heat3Ident      = { .data = "heat3",      .size = 5 };
s_global StringView heat4Ident      = { .data = "heat4",      .size = 5 };
s_global StringView charIdent       = { .data = "char",       .size = 4 };
s_global StringView yearsIdent      = { .data = "years",      .size = 5 };
s_global StringView versionIdent    = { .data = "version",    .size = 7 };
s_global StringView nomatchIdent    = { .data = "nomatch",    .size = 7 };
s_global StringView summaryIdent    = { .data = "summary",    .size = 7 };
s_global StringView nosummaryIdent  = { .data = "nosummary",  .size = 9 };
s_global StringView streakIdent     = { .data = "streak",     .size = 6 };
s_global StringView nostreakIdent   = { .data = "nostreak",   .size = 8 };
s_global StringView helpIdent       = { .data = "help",       .size = 4 };

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
    StringView redSV    = pdCstrSV("red");
    StringView greenSV  = pdCstrSV("green");
    StringView blueSV   = pdCstrSV("blue");
    StringView cyanSV   = pdCstrSV("cyan");
    StringView purpleSV = pdCstrSV("purple");
    StringView pinkSV   = pdCstrSV("pink");
    StringView yellowSV = pdCstrSV("yellow");
    StringView whiteSV  = pdCstrSV("white");

    if(pdSVSame(colour, redSV))
    {
        config->colour = RED;
    }
    else if(pdSVSame(colour, greenSV))
    {
        config->colour = GREEN;
    }
    else if(pdSVSame(colour, blueSV))
    {
        config->colour = BLUE;
    }
    else if(pdSVSame(colour, cyanSV))
    {
        config->colour = CYAN;
    }
    else if(pdSVSame(colour, purpleSV))
    {
        config->colour = PURPLE;
    }
    else if(pdSVSame(colour, pinkSV))
    {
        config->colour = PINK;
    }
    else if(pdSVSame(colour, yellowSV))
    {
        config->colour = YELLOW;
    }
    else if(pdSVSame(colour, whiteSV))
    {
        config->colour = WHITE;
    }

    GF_DEBUG("detected colour: "PRI_SV"", ARG_SV(colour));
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
        char *authorsCstr = malloc(config->authors.size + author.size + 2);
        pdSVConcat(config->authors, sep, authorsCstr);
        free((void*)config->authors.data);
        config->authors = pdCstrSV(authorsCstr);

        pdSVConcat(config->authors, author, authorsCstr);
        config->authors = pdCstrSV(authorsCstr);
    }
    else
    {
        char *newBuf = malloc(GF_BUFSIZE);
        char authorCstr[512];
        pdSVCstr(author, authorCstr);
        config->authors = pdCstrSVCpy(authorCstr, newBuf);
    }

    GF_TRACE("author list: "PRI_SV"", ARG_SV(config->authors));
}

void gfAddAuthorlist
(
    gfConf     *config,
    StringView path
){
    char pathCstr[GF_MAX_PATH];
    pdSVCstr(path, pathCstr);

    char pathExpandedCstr[GF_MAX_PATH];
    pdExpandPath(path, pathExpandedCstr);

    StringView pathExpanded = pdCstrSV(pathExpandedCstr);

    if(pdVerifyPath(pathExpanded) != PD_TYPE_FILE)
    {
        GF_WARN("tasked with opening author list file: '"PRI_SV"', no such file "
                "exists.", ARG_SV(pathExpanded));
        return;
    }

    FILE *file = fopen(pathExpandedCstr, "r");
    if(!file)
    {
        GF_WARN("tasked with opening author list file: '%s' , failed to open.",
                pathExpandedCstr);
        return;
    }

    char buf[GF_BUFSIZE];
    while(fgets(buf, GF_BUFSIZE, file))
    {
        StringView buffer = pdCstrSV(buf);

        StringView commentSV   = pdCstrSV("//");
        const char *commentloc = pdSVFind(commentSV, buffer);
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
    u08 result = pdVerifyPath(resolved);
    if(result == PD_TYPE_FILE)
    {
        GF_WARN("path '"PRI_SV"' is a file, not a directory.", ARG_SV(resolved));
        return;
    }
    else if(result == PD_TYPE_ERROR || result == PD_TYPE_OTHER)
    {
        StringView dashes = pdCstrSV("--");
        if(pdSVFind(dashes, resolved) == resolved.data)
        {
            return;
        }

        GF_WARN("path '"PRI_SV"' does not exist. Ignoring...", ARG_SV(resolved));
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
        char *repositoriesCstr = malloc(config->repositories.size + GF_MAX_PATH + 2);
        pdSVConcat(config->repositories, sep, repositoriesCstr);
        free((void*)config->repositories.data);
        config->repositories = pdCstrSV(repositoriesCstr);

        char resolved_cstr[GF_MAX_PATH];
        pdExpandPath(path, resolved_cstr);
        StringView resolved = pdCstrSV(resolved_cstr);

        char pathSep[resolved.size + 2];
        StringView pathComp = pdCstrSV(resolved_cstr);
        pathSep[resolved.size] = ';';
        pathSep[resolved.size + 1] = '\0';
        if(pdSVFind(pathComp, config->repositories))
        {
            GF_DEBUG("path '"PRI_SV"' already in repository list. Ignoring "
                     "duplicate...", ARG_SV(resolved));
            return;
        }

        verifyDirectory(resolved);

        pdSVConcat(config->repositories, resolved, repositoriesCstr);
        config->repositories = pdCstrSV(repositoriesCstr);
    }
    else
    {
        char *resolvedCstr = calloc(GF_MAX_PATH, 1);
        pdExpandPath(path, resolvedCstr);
        StringView resolved = pdCstrSV(resolvedCstr);
        config->repositories = resolved;

        verifyDirectory(resolved);
    }

    GF_TRACE("path list: "PRI_SV"", ARG_SV(config->repositories));
}

void gfAddPathlist
(
    gfConf     *config,
    StringView path
){
    char pathExpandedCstr[GF_MAX_PATH];
    pdExpandPath(path, pathExpandedCstr);

    StringView pathExpanded = pdCstrSV(pathExpandedCstr);

    if(pdVerifyPath(pathExpanded) != PD_TYPE_FILE)
    {
        GF_WARN("tasked with opening path list file: '"PRI_SV"', no such file exists.",
                ARG_SV(pathExpanded));
        return;
    }

    FILE *file = fopen(pathExpandedCstr, "r");
    if(!file)
    {
        GF_DEBUG("tasked with opening path list file: '"PRI_SV"', failed to open.",
                 ARG_SV(pathExpanded));
        return;
    }

    char buf[GF_BUFSIZE];
    while(fgets(buf, GF_BUFSIZE, file))
    {
        StringView buffer = pdCstrSV(buf);

        StringView commentSV   = pdCstrSV("//");
        const char *commentloc = pdSVFind(commentSV, buffer);
        if(commentloc == buf)
        {
            continue;
        }

        gfAddPath(config, buffer);
    }

    fclose(file);
}

f_internal u08 parseYear
(
    const char *string
){
    u08 number = 0;
    for(u08 i = 0; string[i] != '\0' && string[i] != '\n'; ++i)
    {
        if(i > 2)
        {
            GF_WARN("trying to read too many digits into year: %u.", i);
            return number;
        }

        if(string[i] > 0x2F && string[i] < 0x3A)
        {
            number *= 10;
            number += (string[i] - 0x30);
        }
        else
        {
            GF_WARN("character '%c' is not a digit. Ignoring...", string[i]);
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

    char pathExpanded[GF_MAX_PATH];
    char fallbackExpanded[GF_MAX_PATH];
    pdExpandPath(conf, pathExpanded);
    pdExpandPath(fallback, fallbackExpanded);

    StringView authorlistSV = pdCstrSV("authorlist:");
    StringView repolistSV   = pdCstrSV("repolist:");

    StringView authorSV  = pdCstrSV("author:");
    StringView colourSV  = pdCstrSV("colour:");
    StringView infoSV    = pdCstrSV("info:");
    StringView monoSV    = pdCstrSV("mono:");
    StringView profileSV = pdCstrSV("profile:");
    StringView heat0SV   = pdCstrSV("heat0:");
    StringView heat1SV   = pdCstrSV("heat1:");
    StringView heat2SV   = pdCstrSV("heat2:");
    StringView heat3SV   = pdCstrSV("heat3:");
    StringView heat4SV   = pdCstrSV("heat4:");
    StringView charSV    = pdCstrSV("character:");
    StringView yearsSV   = pdCstrSV("years:");
    StringView summarySV = pdCstrSV("summary:");
    StringView streakSV  = pdCstrSV("streak:");
    StringView trueSV    = pdCstrSV("true");

    FILE *file = fopen(pathExpanded, "r");
    if(!file)
    {
        file = fopen(fallbackExpanded, "r");
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

        StringView commentSV  = pdCstrSV("//");
        const char *commentloc = pdSVFind(commentSV, buffer);
        if(commentloc == buf)
        {
            continue;
        }

        const char* authorlistloc = pdSVFind(authorlistSV, buffer);
        if(authorlistloc)
        {
            StringView authorlist = pdCstrSV(buffer.data + authorlistSV.size + 1);

            gfAddAuthorlist(config, authorlist);
            continue;
        }

        const char* repolistloc = pdSVFind(repolistSV, buffer);
        if(repolistloc)
        {
            StringView repolist = pdCstrSV(buffer.data + repolistSV.size + 1);

            gfAddPathlist(config, repolist);
            continue;
        }

        const char* authorloc = pdSVFind(authorSV, buffer);
        if(authorloc)
        {
            StringView author = pdCstrSV(buffer.data + authorSV.size + 1);

            gfAddAuthor(config, author);
            continue;
        }

        const char* colourloc = pdSVFind(colourSV, buffer);
        if(colourloc)
        {
            StringView chosenSV = pdCstrSV(buffer.data + colourSV.size + 1);
            setColour(config, chosenSV);
            continue;
        }

        const char* infoloc = pdSVFind(infoSV, buffer);
        if(infoloc)
        {
            StringView setSV = pdCstrSV(buffer.data + infoSV.size + 1);

            if(pdSVSame(setSV, trueSV))
            {
                config->flags |= GF_FLAG_INFO;
            }
            continue;
        }

        const char* monoloc = pdSVFind(monoSV, buffer);
        if(monoloc)
        {
            StringView setSV = pdCstrSV(buffer.data + monoSV.size + 1);

            if(pdSVSame(setSV, trueSV))
            {
                config->flags |= GF_FLAG_MONO;
            }
            continue;
        }

        const char* profileloc = pdSVFind(profileSV, buffer);
        if(profileloc)
        {
            StringView setSV = pdCstrSV(buffer.data + profileSV.size + 1);

            if(pdSVSame(setSV, trueSV))
            {
                config->flags |= GF_FLAG_PROFILE;
            }
            continue;
        }

        const char* heat0loc = pdSVFind(heat0SV, buffer);
        if(heat0loc)
        {
            StringView setSV = pdCstrSV(buffer.data + heat0SV.size + 1);

            char *small_buf = malloc(8);
            pdSVCstr(setSV, small_buf);
            config->mono0 = small_buf;
            continue;
        }

        const char* heat1loc = pdSVFind(heat1SV, buffer);
        if(heat1loc)
        {
            StringView setSV = pdCstrSV(buffer.data + heat1SV.size + 1);

            char *small_buf = malloc(8);
            pdSVCstr(setSV, small_buf);
            config->mono1 = small_buf;
            continue;
        }

        const char* heat2loc = pdSVFind(heat2SV, buffer);
        if(heat2loc)
        {
            StringView setSV = pdCstrSV(buffer.data + heat2SV.size + 1);

            char *small_buf = malloc(8);
            pdSVCstr(setSV, small_buf);
            config->mono2 = small_buf;
            continue;
        }

        const char* heat3loc = pdSVFind(heat3SV, buffer);
        if(heat3loc)
        {
            StringView setSV = pdCstrSV(buffer.data + heat3SV.size + 1);

            char *small_buf = malloc(8);
            pdSVCstr(setSV, small_buf);
            config->mono3 = small_buf;
            continue;
        }

        const char* heat4loc = pdSVFind(heat4SV, buffer);
        if(heat4loc)
        {
            StringView setSV = pdCstrSV(buffer.data + heat4SV.size + 1);

            char *small_buf = malloc(8);
            pdSVCstr(setSV, small_buf);
            config->mono4 = small_buf;
            continue;
        }

        const char* charloc = pdSVFind(charSV, buffer);
        if(charloc)
        {
            StringView setSV = pdCstrSV(buffer.data + charSV.size + 1);

            char *small_buf = malloc(8);
            pdSVCstr(setSV, small_buf);
            config->character = small_buf;
            continue;
        }

        const char* yearloc = pdSVFind(yearsSV, buffer);
        if(yearloc)
        {
            config->years = parseYear(buffer.data + yearsSV.size + 1);
            continue;
        }

        const char* summaryloc = pdSVFind(summarySV, buffer);
        if(summaryloc)
        {
            StringView setSV = pdCstrSV(buffer.data + summarySV.size + 1);

            if(pdSVSame(setSV, trueSV))
            {
                config->flags |= GF_FLAG_SUMMARY;
            }
            continue;
        }

        const char* streakloc = pdSVFind(streakSV, buffer);
        if(streakloc)
        {
            StringView setSV = pdCstrSV(buffer.data + streakSV.size + 1);

            if(pdSVSame(setSV, trueSV))
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
    GF_WARN("option '%s' requires a specified argument. Ignoring...", arg);
}

f_internal u08 checkIdentMissing
(
    u16 i,
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
    u08 reposRead   = 0;
    u08 authorsRead = 0;

    if(config->repositories.data)
    {
        reposRead = 1;
    }

    if(config->authors.data)
    {
        authorsRead = 1;
    }

    for(u16 i = 1; i < argc; ++i)
    {
        StringView arg = pdCstrSV(argv[i]);

        if(arg.size < 2 || arg.data[0] != '-' || arg.data[1] != '-')
        {
            goto isPath;
        }

        arg.size -= 2;
        arg.data += 2;

        if(pdSVSame(arg, authorlistIdent))
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
        else if(pdSVSame(arg, repolistIdent))
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
        else if(pdSVSame(arg, authorIdent))
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
        else if(pdSVSame(arg, colourIdent))
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
        else if(pdSVSame(arg, infoIdent))
        {
            config->flags |= GF_FLAG_INFO;
            continue;
        }
        else if(pdSVSame(arg, noinfoIdent))
        {
            config->flags &= ~GF_FLAG_INFO;
            continue;
        }
        else if(pdSVSame(arg, monoIdent))
        {
            config->flags |= GF_FLAG_MONO;
            continue;
        }
        else if(pdSVSame(arg, profileIdent))
        {
            config->flags |= GF_FLAG_PROFILE;
            continue;
        }
        else if(pdSVSame(arg, noprofileIdent))
        {
            config->flags &= ~GF_FLAG_PROFILE;
            continue;
        }
        else if(pdSVSame(arg, heat0Ident))
        {
            if(checkIdentMissing(i, argc, argv))
            {
                continue;
            }

            StringView chosenSV  = pdCstrSV(argv[i + 1]);
            char       *smallBuf = malloc(8);
            pdSVCstr(chosenSV, smallBuf);
            config->mono0 = smallBuf;
            ++i;
            continue;
        }
        else if(pdSVSame(arg, heat1Ident))
        {
            if(checkIdentMissing(i, argc, argv))
            {
                continue;
            }

            StringView chosenSV  = pdCstrSV(argv[i + 1]);
            char       *smallBuf = malloc(8);
            pdSVCstr(chosenSV, smallBuf);
            config->mono1 = smallBuf;
            ++i;
            continue;
        }
        else if(pdSVSame(arg, heat2Ident))
        {
            if(checkIdentMissing(i, argc, argv))
            {
                continue;
            }

            StringView chosenSV  = pdCstrSV(argv[i + 1]);
            char       *smallBuf = malloc(8);
            pdSVCstr(chosenSV, smallBuf);
            config->mono2 = smallBuf;
            ++i;
            continue;
        }
        else if(pdSVSame(arg, heat3Ident))
        {
            if(checkIdentMissing(i, argc, argv))
            {
                continue;
            }

            StringView chosenSV  = pdCstrSV(argv[i + 1]);
            char       *smallBuf = malloc(8);
            pdSVCstr(chosenSV, smallBuf);
            config->mono3 = smallBuf;
            ++i;
            continue;
        }
        else if(pdSVSame(arg, heat4Ident))
        {
            if(checkIdentMissing(i, argc, argv))
            {
                continue;
            }

            StringView chosenSV  = pdCstrSV(argv[i + 1]);
            char       *smallBuf = malloc(8);
            pdSVCstr(chosenSV, smallBuf);
            config->mono4 = smallBuf;
            ++i;
            continue;
        }
        else if(pdSVSame(arg, charIdent))
        {
            if(checkIdentMissing(i, argc, argv))
            {
                continue;
            }

            StringView chosenSV  = pdCstrSV(argv[i + 1]);
            char       *smallBuf = malloc(8);
            pdSVCstr(chosenSV, smallBuf);
            config->character = smallBuf;
            ++i;
            continue;
        }
        else if(pdSVSame(arg, yearsIdent))
        {
            if(checkIdentMissing(i, argc, argv))
            {
                continue;
            }

            config->years = parseYear(argv[i + 1]);
            ++i;
            continue;
        }
        else if(pdSVSame(arg, versionIdent))
        {
            printVersion();
            exit(0);
        }
        else if(pdSVSame(arg, nomatchIdent))
        {
            config->flags |= GF_FLAG_NOMATCH;
            continue;
        }
        else if(pdSVSame(arg, summaryIdent))
        {
            config->flags |= GF_FLAG_SUMMARY;
            continue;
        }
        else if(pdSVSame(arg, nosummaryIdent))
        {
            config->flags &= ~GF_FLAG_SUMMARY;
            continue;
        }
        else if(pdSVSame(arg, streakIdent))
        {
            config->flags |= GF_FLAG_STREAK;
            continue;
        }
        else if(pdSVSame(arg, nostreakIdent))
        {
            config->flags &= ~GF_FLAG_STREAK;
            continue;
        }
        else if(pdSVSame(arg, helpIdent))
        {
            printHelp();
            exit(0);
        }
        else
        {
            GF_ERROR("unknown option '"PRI_SV"'.", ARG_SV(arg));
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
