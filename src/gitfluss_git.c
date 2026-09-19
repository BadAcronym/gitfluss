#include "gitfluss.h"
#include "pd_path.h"
#include "pd_print_macros.h"

void gfGetCommitInfo
(
    StringView   repository,
    StringView   hash,
    gfCommitInfo *commit
){
    PD_ASSERT(hash.size == 40 || hash.size == 64, "commit hash has invalid size: %lu. "
              "should be either 40 or 64.", hash.size);
    PD_WARN("commit hash lookup unimplemented. tee-hee");
    // open commit from either .git/objects/firsttwocharacters/rest
    // or look in packfile
    // return info into commit
}

void gfGetRepositoryHead
(
    StringView   repository,
    gfCommitInfo *commit
){
    char absoluteBuf[4096] = {0};
    StringView absolute = pdExpandPath(repository, absoluteBuf);
    StringView head     = cstr_sv("/.git/HEAD");

    head = sv_concat(absolute, head, absoluteBuf);

    PD_TRACE("resolved head of '"PRI_SV"' to '"PRI_SV"'",
             ARG_SV(repository), ARG_SV(head));

    FILE *file = fopen(absoluteBuf, "rb");
    if(!file)
    {
        PD_WARN("couldn't open repository: '"PRI_SV"'", ARG_SV(repository));
        return;
    }

    char headBuf[4096] = {0};
    uint64_t elements = 1;
    for(uint64_t i = 0; elements == 1; ++i)
    {
        elements = fread(&headBuf[i], 1, 1, file);
    }

    StringView readHead = cstr_sv(headBuf);
    StringView refIdent = cstr_sv("ref:");

    if(!(sv_find(refIdent, readHead) == readHead.data))
    {
        PD_TRACE("identified HEAD: '"PRI_SV"'", ARG_SV(readHead));
        gfGetCommitInfo(absolute, readHead, commit);
        return;
    }

    readHead.data += 5;
    readHead.size -= 5;
    PD_TRACE("identified HEAD ref: '"PRI_SV"'", ARG_SV(readHead));

    StringView ref = cstr_sv("/.git/");
    char refBuf[4096] = {0};

    ref      = sv_concat(ref, readHead, refBuf);
    readHead = sv_concat(repository, ref, headBuf);

    fclose(file);

    PD_TRACE("opening ref under '%s'...", headBuf);

    file = fopen(headBuf, "r");
    if(!file)
    {
        PD_WARN("couldn't open file under ref: '"PRI_SV"'", ARG_SV(readHead));
        return;
    }

    char hashBuf[128] = {0};
    elements = 1;
    for(uint8_t i = 0; i < 127 && elements == 1; ++i)
    {
        elements = fread(&hashBuf[i], 1, 1, file);
    }

    StringView hash = cstr_sv(hashBuf);

    PD_TRACE("identified HEAD: '"PRI_SV"'", ARG_SV(hash));
    gfGetCommitInfo(repository, hash, commit);
    fclose(file);
}
