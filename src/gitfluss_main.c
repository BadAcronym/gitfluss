#define STRING_VIEW_IMPL
#include "string_view.h"

#include <git2.h>

#include <stdint.h>
#include <stdio.h>

int main
(
    void
){
    // TODO: read these from some config file
    StringView repositories = cstr_sv("/home/mandi/repository/river2D_mapedit;/home/mandi/repository/river2D");
    StringView authors      = cstr_sv("test@123.com;terribleacronym@gmail.com");
    uint8_t    actions      = 0; // enum int with like COMMITS_ONLY, COMMITS_MERGES, etc

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
