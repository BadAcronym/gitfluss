#include "gitfluss.h"

void gfGetRepositoryHead
(
    StringView   repository,
    gfCommitInfo *commit
){
    // resolve absolute path to repository
    // open repository/.git/HEAD and read its path
    // resolve absolute path to the content of HEAD, then read that hash
    // open that commit from either objects/firsttwocharacters/rest or look in packfile
    // return info into commit
}

void gfGetCommitInfo
(
    StringView   repository,
    StringView   hash,
    gfCommitInfo *commit
){
    // open commit from hash in either objects/firsttwocharacters/rest
    // or look in packfile
    // return info into commit
}
