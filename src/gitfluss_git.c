#include "gitfluss.h"

#include "datasurf_main.h"

#include "pd_path.h"
#include "pd_dyn_arr.h"
#include "pd_print_macros.h"

s_global const StringView singleDot      = { .size = 1,  .data = "."                };
s_global const StringView doubleDot      = { .size = 2,  .data = ".."               };
s_global const StringView spaceKarat     = { .size = 2,  .data = " <"               };
s_global const StringView karatSpace     = { .size = 2,  .data = "> "               };
s_global const StringView idxIdent       = { .size = 4,  .data = ".idx"             };
s_global const StringView refIdent       = { .size = 4,  .data = "ref:"             };
s_global const StringView packMagic      = { .size = 4,  .data = "PACK"             };
s_global const StringView packIdent      = { .size = 5,  .data = ".pack"            };
s_global const StringView authorIdent    = { .size = 6,  .data = "author"           };
s_global const StringView parentIdent    = { .size = 6,  .data = "parent"           };
s_global const StringView commiterIdent  = { .size = 8,  .data = "commiter"         };
s_global const StringView multiPackIndex = { .size = 16, .data = "multi-pack-index" };

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

void gfFreeCommit
(
    gfCommitInfo *commit
){
    commit->authorTime   = 0;
    commit->commiterTime = 0;
    freeSV(&commit->summary);
    freeSV(&commit->parentHash);
    freeSV(&commit->authorName);
    freeSV(&commit->authorMail);
    freeSV(&commit->commiterName);
    freeSV(&commit->commiterMail);
}

f_internal int64_t readTimeFromSV
(
    StringView sv
){
    int64_t time = 0;

    for(uint8_t i = 0; i < sv.size; ++i)
    {
        if(sv.data[i] == 0x20 || sv.data[i] == 0x0A || !sv.data[i])
        {
            break;
        }
        time *= 10;
        time += sv.data[i] - '0';
    }

    return time;
}

f_internal DeflateInfo readCommitFromPtr
(
    uint8_t      *zlib,
    gfCommitInfo *commit,
    uint64_t     length
){
    uint8_t *commitBuf = calloc(8192, 1);

    DeflateInfo dfInfo = dsReadZlibPtr(zlib, commitBuf, 8192);

    StringView parsed = {0};
    parsed.data = (char*)commitBuf;
    parsed.size = 8192;

    StringView svBuf[10] = {0};
    sv_separate_by_delim(parsed, svBuf, '\n', 10);

    StringView summary      = {0};
    StringView authorName   = {0};
    StringView authorMail   = {0};
    StringView commiterName = {0};
    StringView commiterMail = {0};
    StringView parent       = {0};
    int64_t    authorTime   = 0;
    int64_t    commiterTime = 0;

    uint8_t summaryLine = 0;

    for(uint8_t i = 0; i < 6; ++i)
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
                if(authorTimeSV.data[j] == 0x20 || !authorTimeSV.data[j])
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
                if(commiterTimeSV.data[j] == 0x20 || !commiterTimeSV.data[j])
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
        PD_TRACE("parsed parent: "PRI_SV, ARG_SV(parent));
        commit->parentHash = sv_cpy(parent);
    }

    free(commitBuf);
    return dfInfo;
}

f_internal void readCommitFromFile
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

    readCommitFromPtr(zlibBuf, commit, 1024);

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

    gfFreeCommit(commit);

    uint8_t result = pdVerifyPath(commitPath);
    if(result == PD_TYPE_FILE)
    {
        readCommitFromFile(commitPath, commit);
        return;
    }

    PD_WARN("TODO: handle commit '"PRI_SV"' from packfile. ", ARG_SV(commitPath));
}

f_internal void readMpiFile
(
    StringView path
){
    PD_WARN("TODO: handle mpi file: '"PRI_SV"'", ARG_SV(path));
}

f_internal void readIdxFile
(
    StringView path
){
    PD_WARN("TODO: handle idx file: '"PRI_SV"'", ARG_SV(path));
}

f_internal void readPackFile
(
    StringView path
){
    FILE *file = fopen(path.data, "rb");
    if(!file)
    {
        PD_WARN("couldn't open pack file: '"PRI_SV"'", ARG_SV(path));
        return;
    }

    uint8_t *packFile = 0;

    uint8_t byte = 0;
    for(uint8_t i = 0; i < 4; ++i)
    {
        if(fread(&byte, 1, 1, file) != 1)
        {
            PD_WARN("couldn't read header from pack file: '"PRI_SV"'", ARG_SV(path));
            goto closefile;
        }

        if((char)byte != packMagic.data[i])
        {
            PD_WARN("could not validate pack header in file: '"PRI_SV"'. expected: %u, "
                    "got: %u.", ARG_SV(path), packMagic.data[i], byte);
            goto closefile;
        }
    }

    #ifdef DEBUG
    uint32_t version = 0;
    #endif
    for(uint8_t i = 0; i < 4; ++i)
    {
        if(fread(&byte, 1, 1, file) != 1)
        {
            PD_WARN("couldn't read version from pack file: '"PRI_SV"'", ARG_SV(path));
            goto closefile;
        }
        #ifdef DEBUG
        version += (uint32_t)(byte << ((3 - i) * 8));
        #endif
    }
    PD_TRACE("parsed packfile version %"PRIu32".", version);
    PD_ASSERT(version == 2 || version == 3, "unknown packfile version: %"PRIu32,
              version);

    uint32_t numObj = 0;
    for(uint8_t i = 0; i < 4; ++i)
    {
        if(fread(&byte, 1, 1, file) != 1)
        {
            PD_WARN("couldn't read number of objects from pack file: '"PRI_SV"'",
                    ARG_SV(path));
            goto closefile;
        }
        numObj += (uint32_t)(byte << ((3 - i) * 8));
    }
    PD_TRACE("parsed number of objects: %"PRIu32".", numObj);

    // FIXME: instead of fread-ing every byte separately (awfully slow),
    // read the entire file with 1 call by first viewing how big it is with
    // fseek(, , SEEK_END). that gives us the size that we can fread() and act upon
    // below. we should also assert that the index does not exceed this size.

    for(;;)
    {
        if(fread(&byte, 1, 1, file) != 1)
        {
            if(feof(file))
            {
                break;
            }

            PD_WARN("couldn't read object from pack file: '"PRI_SV"'", ARG_SV(path));
            goto closefile;
        }

        pdArrPush(packFile, byte);
    }
    PD_TRACE("read pack file into buffer that's %"PRIu64" bytes big.",
             pdArrSize(packFile));

    uint64_t index = 0;
    for(uint32_t i = 0; i < numObj; ++i)
    {
        byte = packFile[index++];

        bool     readMore = byte >> 7;
        uint8_t  type     = byte >> 4 & 0x07;
        uint64_t length   = byte & 0x0F;
        uint8_t  shift    = 4;

        for(uint8_t j = 0; readMore && j < 10; ++j)
        {
            byte = packFile[index++];

            uint64_t chunk = byte & 0x7F;

            PD_ASSERT(shift < 64, "cannot shift more than 64 bits.");
            PD_ASSERT(chunk < (UINT64_MAX >> shift), "chunk is too large.");

            readMore = byte  >> 7;
            length  |= chunk << shift;
            shift   += 7;
        }
        PD_TRACE("parsed length from pack object %"PRIu32": %"PRIu64"", i, length);

        PD_ASSERT(type > 0 && type < 8, "invalid object type on obj %"PRIu32": %"PRIu32
                  ". read Byte: 0x%X", i, type, byte);

        if(type == GF_OBJ_COMMIT)
        {
            gfCommitInfo commit = {0};
            DeflateInfo  dfInfo = readCommitFromPtr(&packFile[index], &commit,
                                                    length);

            PD_ASSERT(dfInfo.bytesWritten == length, "expected to decompress into %"
                      PRIu64" bytes, actual: %"PRIu64".", length, dfInfo.bytesWritten);

            // TODO: put commit data into hashed data structure.
            // for this, we need to know what the current hash of the commit is.
            // do we know at all?

            index += dfInfo.compressedBytesRead;

            gfFreeCommit(&commit);
        }
        else if(type == GF_OBJ_OFS_DELTA)
        {
            PD_WARN("TODO: handle OBJ_OFS_DELTA");
            goto closefile;
        }
        else if(type == GF_OBJ_REF_DELTA)
        {
            PD_WARN("TODO: handle OBJ_REF_DELTA");
            goto closefile;
        }
        else
        {
            for(uint64_t j = 0; j < length; ++j)
            {
                if((fread(&byte, 1, 1, file)) != 1)
                {
                    PD_WARN("couldn't skip ahead to next object to handle in pack "
                            "file: '"PRI_SV"'", ARG_SV(path));
                    goto closefile;
                }
            }
        }
    }

closefile:
    pdArrFree(packFile);
    fclose(file);
}

void gfInitRepository
(
    gfRepository *repo,
    gfCommitInfo *head
){
    char packBuf[4096]     = {0};
    char absoluteBuf[4096] = {0};
    StringView absolute = pdExpandPath(repo->path, absoluteBuf);
    StringView gitPACK  = cstr_sv("/.git/objects/pack/");
    StringView gitHEAD  = cstr_sv("/.git/HEAD");

    gitPACK = sv_concat(absolute, gitPACK, packBuf);
    gitHEAD = sv_concat(absolute, gitHEAD, absoluteBuf);

    PD_TRACE("resolved head of '"PRI_SV"' to '"PRI_SV"'",
             ARG_SV(repo->path), ARG_SV(gitHEAD));

    if(pdVerifyPath(gitPACK) != PD_TYPE_DIRECTORY)
    {
        return;
    }

    char       listBuf[8192] = {0};
    StringView list          = pdListFiles(gitPACK, listBuf);
    uint64_t   fileCount     = sv_count_by_delim(list, ';');

    StringView fileBuf[fileCount];
    for(uint64_t i = 0; i < fileCount; ++i)
    {
        fileBuf[i].data = 0;
        fileBuf[i].size = 0;
    }

    sv_separate_by_delim(list, fileBuf, ';', fileCount);
    for(uint64_t i = 0; i < fileCount; ++i)
    {
        if(sv_same(fileBuf[i], singleDot) || sv_same(fileBuf[i], doubleDot))
        {
            continue;
        }
        else if(sv_same(fileBuf[i], multiPackIndex))
        {
            char tmpBuf[4096] = {0};
            readMpiFile(sv_concat(gitPACK, fileBuf[i], tmpBuf));
            continue;
        }
        else if(sv_find(idxIdent, fileBuf[i]))
        {
            char tmpBuf[4096] = {0};
            readIdxFile(sv_concat(gitPACK, fileBuf[i], tmpBuf));
            continue;
        }
        else if(sv_find(packIdent, fileBuf[i]))
        {
            char tmpBuf[4096] = {0};
            readPackFile(sv_concat(gitPACK, fileBuf[i], tmpBuf));
            continue;
        }

        PD_TRACE("unhandled fileType in objects/pack: '"PRI_SV"'", ARG_SV(fileBuf[i]));
    }

    // TODO: open & read all packfiles, create index of what hashes are where for later
    // lookup

    FILE *file = fopen(absoluteBuf, "rb");
    if(!file)
    {
        PD_WARN("couldn't open repository: '"PRI_SV"'", ARG_SV(repo->path));
        return;
    }

    char headBuf[4096] = {0};
    uint64_t elements = 1;
    for(uint64_t i = 0; elements == 1; ++i)
    {
        elements = fread(&headBuf[i], 1, 1, file);
    }

    StringView readHead = cstr_sv(headBuf);

    if(!(sv_find(refIdent, readHead) == readHead.data))
    {
        PD_TRACE("identified HEAD: '"PRI_SV"'", ARG_SV(readHead));
        gfGetCommitInfo(absolute, readHead, head);
        return;
    }

    readHead.data += 5;
    readHead.size -= 5;
    PD_TRACE("identified HEAD ref: '"PRI_SV"'", ARG_SV(readHead));

    StringView ref = cstr_sv("/.git/");
    char refBuf[4096] = {0};

    ref      = sv_concat(ref, readHead, refBuf);
    readHead = sv_concat(repo->path, ref, headBuf);

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
    gfGetCommitInfo(repo->path, hash, head);
    fclose(file);
}
