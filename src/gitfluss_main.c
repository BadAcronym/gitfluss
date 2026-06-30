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

f_internal void readConfig
(
    StringView *repositories,
    StringView *authors
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
        if(authorloc)
        {
            StringView author = cstr_sv(buffer.data + author_sv.size);
            author.size -= 1;

            if(authors->data)
            {
                const char *authors_cstr = sv_concat(*authors, sep);
                *authors = cstr_sv(authors_cstr);

                authors_cstr = sv_concat(*authors, author);
                *authors = cstr_sv(authors_cstr);
            }
            else
            {
                *authors = cstr_sv_cpy(buffer.data + author_sv.size);
                authors->size -= 1;
            }

            #ifdef DEBUG
                fprintf(stderr, "detected author: "PRI_SV"\n", ARG_SV(author));
                fprintf(stderr, "author list: "PRI_SV"\n", ARG_SV(*authors));
            #endif

            continue;
        }

        StringView path = cstr_sv(buffer.data);
        if(path.size)
        {
            path.size -= 1;
        }

        if(repositories->data)
        {
            // ASAN: negative-size-param
            const char *repositories_cstr = sv_concat(*repositories, sep);
            *repositories = cstr_sv(repositories_cstr);

            repositories_cstr = sv_concat(*repositories, path);
            *repositories = cstr_sv(repositories_cstr);
        }
        else
        {
            const char *resolved = gfExpandPath(buffer.data);

            *repositories = cstr_sv_cpy(resolved);
            repositories->size -= 1;

            free((void*)resolved);
        }

        #ifdef DEBUG
            fprintf(stderr, "detected path: "PRI_SV"\n", ARG_SV(path));
            fprintf(stderr, "path list: "PRI_SV"\n", ARG_SV(*repositories));
        #endif
    }

    const char *sorted_authors = sv_sort_by_delim(*authors, ';');
    *authors = cstr_sv(sorted_authors);

    const char *sorted_repos = sv_sort_by_delim(*repositories, ';');
    *repositories = cstr_sv(sorted_repos);

    #ifdef DEBUG
        fprintf(stderr, "\nfinal, sorted author list:\n"PRI_SV"\n", ARG_SV(*authors));
        fprintf(stderr, "\nfinal, sorted paths:\n"PRI_SV"\n", ARG_SV(*repositories));
    #endif

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

int main
(
    void
){
    StringView repositories = {0};
    StringView authors      = {0};

    readConfig(&repositories, &authors);
    uint32_t repository_count = sv_count_by_delim(repositories, ';');

    git_libgit2_init();

    uint32_t   heatmap[16384] = {0};
    uint32_t   repo_max       = 0;
    StringView biggestRepo    = {0};

    for(uint32_t i = 0; i < repository_count; ++i)
    {
        git_repository *repo    = 0;
        git_revwalk    *revwalk = 0;
        git_oid        oid      = {0};

        StringView repository        = sv_find_by_delim(repositories, ';', i);
        uint32_t   repo_commit_count = 0;

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
                    int64_t now  = gfQueryTime();
                    int64_t days = (now - commit_time.time) / (24 * 3600);

                    ++heatmap[days];
                    ++repo_commit_count;
                    break;
                }
            }

            git_commit_free(commit);
        }

        if(repo_commit_count > repo_max)
        {
            repo_max    = repo_commit_count;
            biggestRepo = cstr_sv_cpy(current_repo_cstr);
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

    printf("most commits this year (%u) made %u days ago.\n", singleday_max, maxday);
    printf("most commits in single repository (%u) in '"PRI_SV"'.\n", repo_max,
           ARG_SV(biggestRepo));

    return git_libgit2_shutdown();
}
