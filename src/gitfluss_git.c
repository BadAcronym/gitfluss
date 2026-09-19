#include "gitfluss.h"

#include "datasurf_main.h"
#include "pd_path.h"
#include "pd_print_macros.h"

f_internal void freeSV
(
    StringView *sv
){
    if(sv->data)
    {
        free((void*)sv->data);
        sv->size = 0;
    }
}

f_internal void readCommitData
(
    StringView   path,
    gfCommitInfo *commit
){
    if(!commit)
    {
        PD_ERROR("commit that was passed is nullptr.");
        return;
    }

    freeSV(&commit->summary);
    freeSV(&commit->parentHash);
    freeSV(&commit->authorName);
    freeSV(&commit->authorMail);
    freeSV(&commit->commiterName);
    freeSV(&commit->commiterMail);

    PD_TRACE("opening to read commit from path: '"PRI_SV"'", ARG_SV(path));

    char pathBuf[path.size + 1];
    sv_cstr(path, pathBuf);

    FILE *file = fopen(pathBuf, "rb");
    if(!file)
    {
        PD_WARN("failed to open commit file: '%s'", pathBuf);
        return;
    }

    uint8_t zlibBuf[1024] = {0};

    uint64_t elements = 1;
    for(uint64_t i = 0; i < 1024 && elements == 1; ++i)
    {
        elements = fread(&zlibBuf[i], 1, 1, file);
    }

    uint8_t commitBuf[1024] = {0};
    dsReadZlibPtr(zlibBuf, commitBuf, 1024);

    StringView parsed;
    parsed.data = (char*)commitBuf;
    parsed.size = 1024;

    StringView svBuf[8];

    sv_separate_by_delim(parsed, svBuf, '\n');

    StringView parent = svBuf[1];
    sv_trim(&parent, 8, SV_LEFT);

    StringView authorLine   = svBuf[2];
    StringView commiterLine = svBuf[3];
    StringView summary      = svBuf[4];

    PD_TRACE("parsed parent:       "PRI_SV, ARG_SV(parent));
    PD_TRACE("parsed summary:      "PRI_SV, ARG_SV(summary));
    PD_TRACE("parsed authorLine:   "PRI_SV, ARG_SV(authorLine));
    PD_TRACE("parsed commiterLine: "PRI_SV, ARG_SV(commiterLine));

    commit->summary      = sv_cpy(summary);
    // commit->authorName   = sv_cpy(authorName);
    // commit->authorMail   = sv_cpy(authorMail);
    // commit->commiterName = sv_cpy(commiterName);
    // commit->commiterMail = sv_cpy(commiterMail);
    commit->parentHash   = sv_cpy(parent);

    fclose(file);
}

void gfGetCommitInfo
(
    StringView   repository,
    StringView   hash,
    gfCommitInfo *commit
){
    if(hash.size == 40 || hash.size == 64)
    {
        hash.size -= 1;
    }

    PD_ASSERT(hash.size == 39 || hash.size == 63, "commit hash has invalid size: %lu. "
              "should be either 40 or 64 characters big. passed hash was: '"PRI_SV"'",
              hash.size, ARG_SV(hash));

    StringView hashStart = hash;
    hashStart.size = 2;

    StringView hashRest = hash;
    sv_trim(&hashRest, 2, SV_LEFT);

    StringView folder = cstr_sv("/.git/objects/");
    StringView sep    = cstr_sv("/");

    char pathBuf[4096] = {0};
    StringView commitPath = sv_concat(repository, folder, pathBuf);
    commitPath = sv_concat(commitPath, hashStart, pathBuf);
    commitPath = sv_concat(commitPath, sep, pathBuf);
    commitPath = sv_concat(commitPath, hashRest, pathBuf);

    uint8_t result = pdVerifyPath(commitPath);
    if(result == PD_TYPE_FILE)
    {
        readCommitData(commitPath, commit);
        return;
    }

    PD_WARN("could not find commit in '"PRI_SV"'. commit lookup from packfiles "
            "unimplemented. tee-hee", ARG_SV(commitPath));
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
