#include "gitfluss.h"

gfRepository gfOpenRepository
(
    StringView repo_sv
){
    return (gfRepository){0};
}

void gfGetCommitInfo
(
    gfRepository repository,
    // placeholder
    uint8_t      *oid,
    gfCommitInfo *info
){
}

bool gfRevwalkNext
(
    gfRevwalk *revwalk,
    //placeholder
    uint8_t   *oid
){
    return 0;
}
