#define STRING_VIEW_IMPL
#include "string_view.h"

#include <git2.h>

#include <stdint.h>
#include <stdio.h>

#define f_internal static

#ifdef BUILD_LINUX
    #define CONF_PATH ".conf"
#endif
#ifdef BUILD_WINDOWS
    #define CONF_PATH "gitfluss.ini"
#endif

#define bufsize 4096

f_internal void readConfig
(
    StringView *repositories,
    StringView *authors
){
    FILE *file = fopen(CONF_PATH, "r");
    if(!file)
    {
        fprintf(stderr, "\033[33;3mWARNING: could not open configuration file."
                "\033[0m\n");
        return;
    }

    char buf[bufsize];
    while(fgets(buf, bufsize, file))
    {
        StringView buffer;
        buffer.data = buf;
        buffer.size = bufsize;

        StringView author_sv  = cstr_sv("author: ");
        const char* authorloc = sv_find(author_sv, buffer);
        if(authorloc)
        {
            // add author
            continue;
        }

        // add path
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

    for(uint32_t i = 0; i < repository_count; ++i)
    {
        git_repository *repo    = 0;
        git_revwalk    *revwalk = 0;
        git_oid        oid      = {0};

        StringView repository   = sv_find_by_delim(repositories, ';', i);
        uint32_t   commit_count = 0;

        printf("\nAnalyzing repository %u: "PRI_SV"\n", i, ARG_SV(repository));

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

            if(sv_find(author_mail, authors))
            {
                ++commit_count;
            }

            git_commit_free(commit);
        }
        printf("commits in "PRI_SV": %i\n", ARG_SV(repository), commit_count);

        git_revwalk_free(revwalk);
        git_repository_free(repo);
        free((void*)current_repo_cstr);
    }

    return git_libgit2_shutdown();
}
