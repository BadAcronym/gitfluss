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

#define bufsize     4096
#define MAX_AUTHORS 1024

#define HEAT_0 " "

#define HEAT_1_RED   "\033[38;5;52m\u25FC\033[0m"
#define HEAT_2_RED   "\033[38;5;88m\u25FC\033[0m"
#define HEAT_3_RED   "\033[38;5;124m\u25FC\033[0m"
#define HEAT_4_RED   "\033[38;5;160m\u25FC\033[0m"
#define HEAT_5_RED   "\033[38;5;196m\u25FC\033[0m"

#define HEAT_1_GREEN "\033[38;5;190m\u25FC\033[0m"
#define HEAT_2_GREEN "\033[38;5;154m\u25FC\033[0m"
#define HEAT_3_GREEN "\033[38;5;118m\u25FC\033[0m"
#define HEAT_4_GREEN "\033[38;5;82m\u25FC\033[0m"
#define HEAT_5_GREEN "\033[38;5;46m\u25FC\033[0m"

f_internal void readConfig
(
    StringView *repositories,
    StringView *authors,
    uint8_t    *green
){
    const char *path_expanded     = gfExpandPath(CONF_PATH);
    const char *fallback_expanded = gfExpandPath(CONF_FALLBACK);

    FILE *file = fopen(path_expanded, "r");
    if(!file)
    {
        file = fopen(fallback_expanded, "r");
        if(!file)
        {
            fprintf(stderr, "\033[33;3mWARNING: could not open configuration file."
                    "\033[0m\n");
            goto cleanup;
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

        if(repositories->data)
        {
            const char *repositories_cstr = sv_concat(*repositories, sep);
            free((void*)repositories->data);
            *repositories = cstr_sv_cpy(repositories_cstr);

            const char *resolved   = gfExpandPath(buffer.data);
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
            if(resolved)
            {
                free((void*)resolved);
            }
        }
        else
        {
            const char *resolved = gfExpandPath(buffer.data);
            *repositories = cstr_sv_cpy(resolved);

            if(repositories->size)
            {
                repositories->size -= 1;
            }

            if(resolved)
            {
                free((void*)resolved);
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

cleanup:
    if(path_expanded)
    {
        free((void*)path_expanded);
    }
    if(fallback_expanded)
    {
        free((void*)fallback_expanded);
    }
    fclose(file);
}

f_internal void printMonthHeader
(
    uint8_t current
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

    for(uint8_t i = 0; i < 12; ++i)
    {
        printf(" %s ", months[(current + i) % 12]);
    }
    printf("\n");
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
    uint32_t   oldestCommitTime = UINT32_MAX;
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

    uint32_t days_epoch  = now / (24 * 3600);
    uint32_t days_commit = (now - oldestCommitTime) / (24 * 3600);
    printf("\ndays since epoch: %u\n", days_epoch);
    printf("days since first commit: %u\n", days_commit);

    printf("\nheatmap (last 365 days):\n");
    uint8_t currentMonth = 6;
    printMonthHeader(currentMonth);

    // printf("%s", months[currentMonth]);

    printf(HEAT_0);
    printf(HEAT_1_RED);
    printf(HEAT_2_RED);
    printf(HEAT_3_RED);
    printf(HEAT_4_RED);
    printf(HEAT_5_RED);

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
