#include "gitfluss.h"
#include "pd_path.h"

f_internal void setColour
(
    gfConf     *config,
    StringView colour
){
    StringView red_sv    = cstr_sv("red");
    StringView green_sv  = cstr_sv("green");
    StringView blue_sv   = cstr_sv("blue");
    StringView cyan_sv   = cstr_sv("cyan");
    StringView purple_sv = cstr_sv("purple");
    StringView pink_sv   = cstr_sv("pink");
    StringView yellow_sv = cstr_sv("yellow");
    StringView white_sv  = cstr_sv("white");

    if(sv_same(colour, red_sv))
    {
        config->colour = RED;
    }
    else if(sv_same(colour, green_sv))
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

void gfAddAuthor
(
    gfConf     *config,
    StringView author
){
    if(!author.size)
    {
        return;
    }

    StringView sep = cstr_sv(";");

    if(config->authors.data && config->authors.size)
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
        fprintf(stderr, "author list: "PRI_SV"\n", ARG_SV(config->authors));
    #endif
}

void gfAddAuthorlist
(
    gfConf     *config,
    StringView path
){
    char path_cstr[4096];
    sv_cstr(path, path_cstr);

    char path_expanded_cstr[4096];
    pdExpandPath(path, path_expanded_cstr);

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
        fprintf(stderr, "\033[33;1mWARNING: tasked with opening author list file: '%s' "
                        ", failed to open.\033[0m\n", path_expanded_cstr);
        #endif
        return;
    }

    char buf[bufsize];
    while(fgets(buf, bufsize, file))
    {
        StringView buffer = cstr_sv(buf);

        StringView comment_sv  = cstr_sv("//");
        const char *commentloc = sv_find(comment_sv, buffer);
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
        fprintf(stderr, "\033[33;3mWARNING: path '"PRI_SV"' is a file, not a "
                        "directory.\033[0m\n", ARG_SV(resolved));
        return;
    }
    else if(result == PD_TYPE_ERROR || result == PD_TYPE_OTHER)
    {
        StringView dashes = cstr_sv("--");
        if(sv_find(dashes, resolved) == resolved.data)
        {
            fprintf(stderr, "\033[31;3mERROR: unknown option '"PRI_SV
                    "'.\033[0m\n", ARG_SV(resolved));
            // TODO: print help msg with list of options or something.
            exit(1);
        }
        fprintf(stderr, "\033[33;3mWARNING: path '"PRI_SV"' does not exist. Ignoring..."
                "\033[0m\n", ARG_SV(resolved));
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
    sv_cstr(path, path_cstr);

    StringView sep = cstr_sv(";");

    if(config->repositories.data && config->repositories.size)
    {
        char *repositories_cstr = malloc(config->repositories.size + MAX_PATH + 2);
        sv_concat(config->repositories, sep, repositories_cstr);
        free((void*)config->repositories.data);
        config->repositories = cstr_sv(repositories_cstr);

        char resolved_cstr[MAX_PATH];
        pdExpandPath(path, resolved_cstr);
        StringView resolved = cstr_sv(resolved_cstr);

        char pathSep[resolved.size + 2];
        StringView pathComp = cstr_sv(resolved_cstr);
        pathSep[resolved.size] = ';';
        pathSep[resolved.size + 1] = '\0';
        if(sv_find(pathComp, config->repositories))
        {
            fprintf(stderr, "\033[33;3mPath '"PRI_SV"' already in repository "
                            "list. Ignoring duplicate...\033[0m\n", ARG_SV(resolved));
            return;
        }

        verifyDirectory(resolved);

        sv_concat(config->repositories, resolved, repositories_cstr);
        config->repositories = cstr_sv(repositories_cstr);
    }
    else
    {
        char *resolved_cstr = calloc(MAX_PATH, 1);
        pdExpandPath(path, resolved_cstr);
        StringView resolved = cstr_sv(resolved_cstr);
        config->repositories = resolved;

        verifyDirectory(resolved);
    }

    #ifdef DEBUG
        fprintf(stderr, "path list: "PRI_SV"\n", ARG_SV(config->repositories));
    #endif
}

void gfAddPathlist
(
    gfConf     *config,
    StringView path
){
    char path_expanded_cstr[4096];
    pdExpandPath(path, path_expanded_cstr);

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

        StringView comment_sv  = cstr_sv("//");
        const char *commentloc = sv_find(comment_sv, buffer);
        if(commentloc == buf)
        {
            continue;
        }

        gfAddPath(config, buffer);
    }

    fclose(file);
}

f_internal uint8_t parseDigits
(
    const char *string
){
    uint8_t number = 0;
    for(uint16_t i = 0; string[i] != '\0' && string[i] != '\n'; ++i)
    {
        if(string[i] > 0x2F && string[i] < 0x3A)
        {
            number *= 10;
            number += ((uint8_t)string[i] - 0x30);
        }
        else
        {
            fprintf(stderr, "\033[33;3mWARNING: character '%c' is not a digit. "
                    "Ignoring...\033[0m\n", string[i]);
        }
    }

    return number;
}

void gfReadConfig
(
    gfConf *config
){
    StringView conf     = cstr_sv(CONF_PATH);
    StringView fallback = cstr_sv(CONF_FALLBACK);

    char path_expanded[MAX_PATH];
    char fallback_expanded[MAX_PATH];
    pdExpandPath(conf, path_expanded);
    pdExpandPath(fallback, fallback_expanded);

    StringView authorlist_sv = cstr_sv("authorlist:");
    StringView repolist_sv   = cstr_sv("repolist:");

    StringView author_sv  = cstr_sv("author:");
    StringView colour_sv  = cstr_sv("colour:");
    StringView info_sv    = cstr_sv("info:");
    StringView mono_sv    = cstr_sv("mono:");
    StringView profile_sv = cstr_sv("profile:");
    StringView heat0_sv   = cstr_sv("heat0:");
    StringView heat1_sv   = cstr_sv("heat1:");
    StringView heat2_sv   = cstr_sv("heat2:");
    StringView heat3_sv   = cstr_sv("heat3:");
    StringView heat4_sv   = cstr_sv("heat4:");
    StringView char_sv    = cstr_sv("character:");
    StringView years_sv   = cstr_sv("years:");
    StringView true_sv    = cstr_sv("true");

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

            gfAddAuthorlist(config, authorlist);
            continue;
        }

        const char* repolistloc = sv_find(repolist_sv, buffer);
        if(repolistloc)
        {
            StringView repolist = cstr_sv(buffer.data + repolist_sv.size + 1);

            gfAddPathlist(config, repolist);
            continue;
        }

        const char* authorloc = sv_find(author_sv, buffer);
        if(authorloc)
        {
            StringView author = cstr_sv(buffer.data + author_sv.size + 1);

            gfAddAuthor(config, author);
            continue;
        }

        const char* colourloc = sv_find(colour_sv, buffer);
        if(colourloc)
        {
            StringView chosen_sv = cstr_sv(buffer.data + colour_sv.size + 1);
            setColour(config, chosen_sv);
            continue;
        }

        const char* infoloc = sv_find(info_sv, buffer);
        if(infoloc)
        {
            StringView set_sv = cstr_sv(buffer.data + info_sv.size + 1);

            if(sv_same(set_sv, true_sv))
            {
                config->flags |= GF_FLAG_INFO;
            }
            continue;
        }

        const char* monoloc = sv_find(mono_sv, buffer);
        if(monoloc)
        {
            StringView set_sv = cstr_sv(buffer.data + mono_sv.size + 1);

            if(sv_same(set_sv, true_sv))
            {
                config->flags |= GF_FLAG_MONO;
            }
            continue;
        }

        const char* profileloc = sv_find(profile_sv, buffer);
        if(profileloc)
        {
            StringView set_sv = cstr_sv(buffer.data + profile_sv.size + 1);

            if(sv_same(set_sv, true_sv))
            {
                config->flags |= GF_FLAG_PROFILE;
            }
            continue;
        }

        const char* heat0loc = sv_find(heat0_sv, buffer);
        if(heat0loc)
        {
            StringView set_sv = cstr_sv(buffer.data + heat0_sv.size + 1);

            char *small_buf = malloc(8);
            sv_cstr(set_sv, small_buf);
            config->mono0 = small_buf;
            continue;
        }

        const char* heat1loc = sv_find(heat1_sv, buffer);
        if(heat1loc)
        {
            StringView set_sv = cstr_sv(buffer.data + heat1_sv.size + 1);

            char *small_buf = malloc(8);
            sv_cstr(set_sv, small_buf);
            config->mono1 = small_buf;
            continue;
        }

        const char* heat2loc = sv_find(heat2_sv, buffer);
        if(heat2loc)
        {
            StringView set_sv = cstr_sv(buffer.data + heat2_sv.size + 1);

            char *small_buf = malloc(8);
            sv_cstr(set_sv, small_buf);
            config->mono2 = small_buf;
            continue;
        }

        const char* heat3loc = sv_find(heat3_sv, buffer);
        if(heat3loc)
        {
            StringView set_sv = cstr_sv(buffer.data + heat3_sv.size + 1);

            char *small_buf = malloc(8);
            sv_cstr(set_sv, small_buf);
            config->mono3 = small_buf;
            continue;
        }

        const char* heat4loc = sv_find(heat4_sv, buffer);
        if(heat4loc)
        {
            StringView set_sv = cstr_sv(buffer.data + heat4_sv.size + 1);

            char *small_buf = malloc(8);
            sv_cstr(set_sv, small_buf);
            config->mono4 = small_buf;
            continue;
        }

        const char* charloc = sv_find(char_sv, buffer);
        if(charloc)
        {
            StringView set_sv = cstr_sv(buffer.data + char_sv.size + 1);

            char *small_buf = malloc(8);
            sv_cstr(set_sv, small_buf);
            config->character = small_buf;
            continue;
        }

        const char* yearloc = sv_find(years_sv, buffer);
        if(yearloc)
        {
            config->years = parseDigits(buffer.data + years_sv.size + 1);
            continue;
        }

        StringView path = cstr_sv(buffer.data);
        gfAddPath(config, path);
    }

    fclose(file);
}

f_internal void printSpecMissing
(
    const char *arg
){
    fprintf(stderr, "\033[33;3mWARNING: option '%s' requires a "
            "specified argument. Ignoring...\033[0m\n", arg);
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
        StringView arg = cstr_sv(argv[i]);

        if(arg.size < 2 || arg.data[0] != '-' || arg.data[1] != '-')
        {
            goto isPath;
        }

        StringView authorlist_ident = cstr_sv("authorlist");
        StringView repolist_ident   = cstr_sv("repolist");
        StringView author_ident     = cstr_sv("author");
        StringView colour_ident     = cstr_sv("colour");
        StringView info_ident       = cstr_sv("info");
        StringView noinfo_ident     = cstr_sv("noinfo");
        StringView mono_ident       = cstr_sv("mono");
        StringView profile_ident    = cstr_sv("profile");
        StringView noprofile_ident  = cstr_sv("noprofile");
        StringView heat0_ident      = cstr_sv("heat0");
        StringView heat1_ident      = cstr_sv("heat1");
        StringView heat2_ident      = cstr_sv("heat2");
        StringView heat3_ident      = cstr_sv("heat3");
        StringView heat4_ident      = cstr_sv("heat4");
        StringView char_ident       = cstr_sv("char");
        StringView years_ident      = cstr_sv("years");
        StringView version_ident    = cstr_sv("version");
        StringView nomatch_ident    = cstr_sv("nomatch");
        StringView summary_ident    = cstr_sv("summary");

        arg.size -= 2;
        arg.data += 2;

        if(sv_same(arg, authorlist_ident))
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

            gfAddAuthorlist(config, cstr_sv(argv[i + 1]));
            ++i;
            continue;
        }
        else if(sv_same(arg, repolist_ident))
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

            gfAddPathlist(config, cstr_sv(argv[i + 1]));
            ++i;
            continue;
        }
        else if(sv_same(arg, author_ident))
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

            StringView author = cstr_sv(argv[i + 1]);
            gfAddAuthor(config, author);

            ++i;
            continue;
        }
        else if(sv_same(arg, colour_ident))
        {
            if(checkIdentMissing(i, argc, argv))
            {
                continue;
            }

            StringView colour = cstr_sv(argv[i + 1]);
            setColour(config, colour);

            ++i;
            continue;
        }
        else if(sv_same(arg, info_ident))
        {
            config->flags |= GF_FLAG_INFO;
            continue;
        }
        else if(sv_same(arg, noinfo_ident))
        {
            config->flags &= ~GF_FLAG_INFO;
            continue;
        }
        else if(sv_same(arg, mono_ident))
        {
            config->flags |= GF_FLAG_MONO;
            continue;
        }
        else if(sv_same(arg, profile_ident))
        {
            config->flags |= GF_FLAG_PROFILE;
            continue;
        }
        else if(sv_same(arg, noprofile_ident))
        {
            config->flags &= ~GF_FLAG_PROFILE;
            continue;
        }
        else if(sv_same(arg, heat0_ident))
        {
            if(checkIdentMissing(i, argc, argv))
            {
                continue;
            }

            StringView chosen_sv  = cstr_sv(argv[i + 1]);
            char       *small_buf = malloc(8);
            sv_cstr(chosen_sv, small_buf);
            config->mono0 = small_buf;
            ++i;
            continue;
        }
        else if(sv_same(arg, heat1_ident))
        {
            if(checkIdentMissing(i, argc, argv))
            {
                continue;
            }

            StringView chosen_sv  = cstr_sv(argv[i + 1]);
            char       *small_buf = malloc(8);
            sv_cstr(chosen_sv, small_buf);
            config->mono1 = small_buf;
            ++i;
            continue;
        }
        else if(sv_same(arg, heat2_ident))
        {
            if(checkIdentMissing(i, argc, argv))
            {
                continue;
            }

            StringView chosen_sv  = cstr_sv(argv[i + 1]);
            char       *small_buf = malloc(8);
            sv_cstr(chosen_sv, small_buf);
            config->mono2 = small_buf;
            ++i;
            continue;
        }
        else if(sv_same(arg, heat3_ident))
        {
            if(checkIdentMissing(i, argc, argv))
            {
                continue;
            }

            StringView chosen_sv  = cstr_sv(argv[i + 1]);
            char       *small_buf = malloc(8);
            sv_cstr(chosen_sv, small_buf);
            config->mono3 = small_buf;
            ++i;
            continue;
        }
        else if(sv_same(arg, heat4_ident))
        {
            if(checkIdentMissing(i, argc, argv))
            {
                continue;
            }

            StringView chosen_sv  = cstr_sv(argv[i + 1]);
            char       *small_buf = malloc(8);
            sv_cstr(chosen_sv, small_buf);
            config->mono4 = small_buf;
            ++i;
            continue;
        }
        else if(sv_same(arg, char_ident))
        {
            if(checkIdentMissing(i, argc, argv))
            {
                continue;
            }

            StringView chosen_sv  = cstr_sv(argv[i + 1]);
            char       *small_buf = malloc(8);
            sv_cstr(chosen_sv, small_buf);
            config->character = small_buf;
            ++i;
            continue;
        }
        else if(sv_same(arg, years_ident))
        {
            if(checkIdentMissing(i, argc, argv))
            {
                continue;
            }

            config->years = parseDigits(argv[i + 1]);
            ++i;
            continue;
        }
        else if(sv_same(arg, version_ident))
        {
            config->flags |= GF_FLAG_VERSION;
            return;
        }
        else if(sv_same(arg, nomatch_ident))
        {
            config->flags |= GF_FLAG_NOMATCH;
            continue;
        }
        else if(sv_same(arg, summary_ident))
        {
            config->flags |= GF_FLAG_SUMMARY;
            continue;
        }

    isPath:
        if(reposRead)
        {
            reposRead = 0;
            free((void*)config->repositories.data);
            config->repositories.size = 0;
        }

        StringView path = cstr_sv(argv[i]);
        gfAddPath(config, path);
    }
}
