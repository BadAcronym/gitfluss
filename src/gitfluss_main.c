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
    const char *repo_name = "/home/mandi/repository/river2D_mapedit";
    const char *authors   = "test@123.com;terribleacronym@gmail.com";
    uint8_t     actions   = 0; // enum int with like COMMITS_ONLY, COMMITS_MERGES, etc etc

    git_repository *repo;
    git_revwalk    *revwalk;
    git_oid        oid;

    git_libgit2_init();

    // for(;;) <- for each repository
    {
        uint32_t commit_count = 0;

        git_repository_open(&repo, repo_name);
        git_revwalk_new(&revwalk, repo);
        git_revwalk_push_head(revwalk);

        while(!git_revwalk_next(&oid, revwalk))
        {
            git_commit *commit;
            git_commit_lookup(&commit, repo, &oid);

            const git_signature *sign = git_commit_author(commit);

            if(sign->email != authors)
            {
            }

            ++commit_count;

            git_commit_free(commit);
        }
        printf("commits in %s: %i\n", repo_name, commit_count);
    }

    git_revwalk_free(revwalk);
    git_repository_free(repo);
    git_libgit2_shutdown();

    return 0;
}
