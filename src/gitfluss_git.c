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
    }
    sv->data = 0;
    sv->size = 0;
}

f_internal int64_t readTimeFromSV
(
    StringView sv
){
    int64_t time = 0;

    for(uint8_t i = 0; i < sv.size; ++i)
    {
        if(sv.data[i] == 0x20)
        {
            break;
        }
        time *= 10;
        time += sv.data[i] - '0';
    }

    return time;
}

f_internal void readCommitData
(
    StringView   path,
    gfCommitInfo *commit
){
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

    StringView svBuf[10];

    sv_separate_by_delim(parsed, svBuf, '\n');

    StringView summary      = {0};
    StringView authorName   = {0};
    StringView authorMail   = {0};
    StringView commiterName = {0};
    StringView commiterMail = {0};
    StringView parent       = {0};
    int64_t    authorTime   = 0;
    int64_t    commiterTime = 0;

    StringView spaceKarat    = cstr_sv(" <");
    StringView karatSpace    = cstr_sv("> ");
    StringView authorIdent   = cstr_sv("author");
    StringView parentIdent   = cstr_sv("parent");
    StringView commiterIdent = cstr_sv("commiter");

    uint8_t summaryLine = 0;

    for(uint8_t i = 0; i < 8; ++i)
    {
        if(sv_find(authorIdent, svBuf[i]))
        {
            summaryLine = i + 2;
            authorName  = svBuf[i];
            sv_trim(&authorName, 7, SV_LEFT);
            const char *startKaratLoc = sv_find(spaceKarat, authorName);
            const char *endKaratLoc   = sv_find(karatSpace, authorName);

            if(!startKaratLoc || !endKaratLoc)
            {
                continue;
            }

            authorName.size = (uint64_t)(startKaratLoc - authorName.data);

            authorMail.data = startKaratLoc + 2;
            authorMail.size = (uint64_t)(endKaratLoc - authorMail.data);

            StringView authorTimeSV = {0};
            authorTimeSV.data = authorMail.data + authorMail.size + 2;
            for(uint32_t j = 0; j < 20; ++j)
            {
                if(authorTimeSV.data[j] == 0x20 || authorTimeSV.data[j] == 0x0A)
                {
                    break;
                }
                ++authorTimeSV.size;
            }

            authorTime = readTimeFromSV(authorTimeSV);

            PD_TRACE("parsed authorName:   "PRI_SV, ARG_SV(authorName));
            PD_TRACE("parsed authorMail:   "PRI_SV, ARG_SV(authorMail));
            PD_TRACE("parsed authorTime:   "PRI_SV, ARG_SV(authorTimeSV));
        }
        else if(sv_find(commiterIdent, svBuf[i]))
        {
            summaryLine  = i + 2;
            commiterName = svBuf[i];
            sv_trim(&commiterName, 9, SV_LEFT);
            const char *startKaratLoc = sv_find(spaceKarat, commiterName);
            const char *endKaratLoc   = sv_find(karatSpace, commiterName);

            if(!startKaratLoc || !endKaratLoc)
            {
                continue;
            }

            commiterName.size = (uint64_t)(startKaratLoc - commiterName.data);

            commiterMail.data = startKaratLoc + 2;
            commiterMail.size = (uint64_t)(endKaratLoc - commiterMail.data);

            StringView commiterTimeSV = {0};
            commiterTimeSV.data = commiterMail.data + commiterMail.size + 2;
            for(uint32_t j = 0; j < 20; ++j)
            {
                if(commiterTimeSV.data[j] == 0x20 || commiterTimeSV.data[j] == 0x0A)
                {
                    break;
                }
                ++commiterTimeSV.size;
            }

            commiterTime = readTimeFromSV(commiterTimeSV);

            PD_TRACE("parsed commiterName: "PRI_SV, ARG_SV(commiterName));
            PD_TRACE("parsed commiterMail: "PRI_SV, ARG_SV(commiterMail));
            PD_TRACE("parsed commiterTime: "PRI_SV, ARG_SV(commiterTimeSV));
        }
        else if(sv_find(parentIdent, svBuf[i]))
        {
            parent = svBuf[i];
            sv_trim(&parent, 7, SV_LEFT);

            PD_TRACE("parsed parent:       "PRI_SV, ARG_SV(parent));
        }
    }

    summary = svBuf[summaryLine];

    PD_TRACE("parsed summary: '"PRI_SV"'", ARG_SV(summary));

    commit->authorTime   = authorTime;
    commit->commiterTime = commiterTime;

    if(summary.size)
    {
        commit->summary = sv_cpy(summary);
    }
    if(authorName.size)
    {
        commit->authorName = sv_cpy(authorName);
    }
    if(authorMail.size)
    {
        commit->authorMail = sv_cpy(authorMail);
    }
    if(commiterName.size)
    {
        commit->commiterName = sv_cpy(commiterName);
    }
    if(commiterMail.size)
    {
        commit->commiterMail = sv_cpy(commiterMail);
    }
    if(parent.size)
    {
        commit->parentHash = sv_cpy(parent);
    }

    fclose(file);
}

void gfGetCommitInfo
(
    StringView   repository,
    StringView   hash,
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

    PD_ASSERT(repository.data && repository.size, "cannot open null repository.");

    PD_ASSERT(hash.size == 40 || hash.size == 64, "commit hash has invalid size: %lu. "
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
