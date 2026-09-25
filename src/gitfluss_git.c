#include "gitfluss.h"

#include "datasurf_main.h"

#include "pd_path.h"
#include "pd_dyn_arr.h"
#include "pd_print_macros.h"

s_global const StringView sep             = { .size = 1,  .data = "/"              };
s_global const StringView spaceKarat      = { .size = 2,  .data = " <"             };
s_global const StringView karatSpace      = { .size = 2,  .data = "> "             };
s_global const StringView idxIdent        = { .size = 4,  .data = ".idx"           };
s_global const StringView refIdent        = { .size = 4,  .data = "ref:"           };
s_global const StringView idxV2Magic      = { .size = 4,  .data = "\377tOc"        };
s_global const StringView authorIdent     = { .size = 6,  .data = "author"         };
s_global const StringView parentIdent     = { .size = 6,  .data = "parent"         };
s_global const StringView commiterIdent   = { .size = 8,  .data = "commiter"       };
s_global const StringView gitObjectFolder = { .size = 14, .data = "/.git/objects/" };

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

    freeSV(&commit->hash);
    freeSV(&commit->summary);
    freeSV(&commit->parentHash);
    freeSV(&commit->authorName);
    freeSV(&commit->authorMail);
    freeSV(&commit->commiterName);
    freeSV(&commit->commiterMail);
}

f_internal char valueToHexChar
(
    uint8_t value
){
    PD_ASSERT(value < 0x10, "cannot express values bigger than 15 in 4 bits.");

    if(value > 9)
    {
        return value + 0x57;
    }

    return value + 0x30;
}

f_internal uint8_t twoCharsToByte
(
    char c1,
    char c2
){
    uint8_t v1 = 0;
    uint8_t v2 = 0;

    PD_ASSERT((c1 > 0x2F && c1 < 0x3A) || (c1 > 0x60 && c1 < 0x67),
              "c1 outside of valid range. passed char: '%c' (0x%X)", c1, c1);
    PD_ASSERT((c2 > 0x2F && c2 < 0x3A) || (c2 > 0x60 && c2 < 0x67),
              "c2 outside of valid range. passed char: '%c' (0x%X)", c2, c2);

    v1 = (uint8_t)c1 - '0';
    if(c1 > 0x60)
    {
        v1 = (uint8_t)c1 - 0x54;
    }

    v2 = (uint8_t)c2 - '0';
    if(c2 > 0x60)
    {
        v2 = (uint8_t)c2 - 0x54;
    }

    return (uint8_t)((v1 << 4) | v2);
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
    gfCommitInfo *commit
){
    uint8_t *commitBuf = calloc(8192, 1);

    DeflateInfo dfInfo = dsReadZlibPtr(zlib, commitBuf, 8192);

    StringView commitSV = {0};
    commitSV.data = (char*)commitBuf;
    commitSV.size = 8192;

    StringView summary      = {0};
    StringView authorName   = {0};
    StringView authorMail   = {0};
    StringView commiterName = {0};
    StringView commiterMail = {0};
    StringView parent       = {0};
    int64_t    authorTime   = 0;
    int64_t    commiterTime = 0;

    const char *parentLoc = sv_find(parentIdent, commitSV);
    if(parentLoc)
    {
        parent.data = parentLoc + 7;
        parent.size = 40;
        if(parent.data[40] != 0x0A)
        {
            parent.size = 64;
        }
    }

    const char *authorLoc = sv_find(authorIdent, commitSV);
    if(authorLoc)
    {
        authorName.data = authorLoc + 7;
        authorName.size = 4096;

        summary = sv_find_by_delim(authorName, '\n', 2);

        const char *startKaratLoc = sv_find(spaceKarat, authorName);
        const char *endKaratLoc   = sv_find(karatSpace, authorName);

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

    const char *commiterLoc = sv_find(commiterIdent, commitSV);
    if(commiterLoc)
    {
        commiterName.data = commiterLoc + 9;
        commiterName.size = 4096;

        summary = sv_find_by_delim(authorName, '\n', 2);

        const char *startKaratLoc = sv_find(spaceKarat, commiterName);
        const char *endKaratLoc   = sv_find(karatSpace, commiterName);

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

        PD_TRACE("parsed commiterName:   "PRI_SV, ARG_SV(commiterName));
        PD_TRACE("parsed commiterMail:   "PRI_SV, ARG_SV(commiterMail));
        PD_TRACE("parsed commiterTime:   "PRI_SV, ARG_SV(commiterTimeSV));
    }

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

    PD_TRACE("reading commit from loose object.");
    readCommitFromPtr(zlibBuf, commit);

    fclose(file);
}

bool gfGetCommitInfo
(
    StringView   repository,
    StringView   hash,
    gfCommitInfo *commit,
    gfCommitInfo **table
){
    if(!commit)
    {
        PD_ERROR("commit that was passed is nullptr.");
        return false;
    }

    PD_ASSERT(repository.data && repository.size, "cannot open null repository.");

    PD_ASSERT(hash.size == 40 || hash.size == 64, "commit hash has invalid size: %lu. "
              "should be either 40 or 64 characters big. passed hash was: '"PRI_SV"'",
              hash.size, ARG_SV(hash));

    PD_ASSERT(hash.data, "cannot lookup commit with no hash.");

    PD_TRACE("looking for commit with hash: "PRI_SV, ARG_SV(hash));

    StringView hashStart = hash;
    hashStart.size = 2;

    StringView hashRest = hash;
    sv_trim(&hashRest, 2, SV_LEFT);

    char pathBuf[4096] = {0};
    StringView commitPath = sv_concat(repository, gitObjectFolder, pathBuf);
    commitPath = sv_concat(commitPath, hashStart, pathBuf);
    commitPath = sv_concat(commitPath, sep, pathBuf);
    commitPath = sv_concat(commitPath, hashRest, pathBuf);

    uint8_t result = pdVerifyPath(commitPath);
    if(result == PD_TYPE_FILE)
    {
        gfFreeCommit(commit);
        readCommitFromFile(commitPath, commit);
        return true;
    }

    bool found = false;

    if(!table)
    {
        PD_TRACE("table does not exist.");
        goto notfound;
    }

    uint8_t firstTwo = twoCharsToByte(hash.data[0], hash.data[1]);
    if(!table[firstTwo])
    {
        PD_TRACE("table[0x%x] has no data.", firstTwo);
        goto notfound;
    }

    uint64_t arraySize = pdArrSize(table[firstTwo]);
    PD_TRACE("looking for commit in table[%c%c]: size %"PRIu64,
             hash.data[0], hash.data[1], arraySize);
    for(uint64_t i = 0; i < arraySize; ++i)
    {
        PD_TRACE("checking against commit in table[%c%c]: "PRI_SV,
                 hash.data[0], hash.data[1], ARG_SV(table[firstTwo][i].hash));
        if(sv_same(hash, table[firstTwo][i].hash))
        {
            gfFreeCommit(commit);
            *commit = table[firstTwo][i];
            PD_TRACE("found commit!");
            found = true;
            break;
        }
    }

    if(!found)
    {
        goto notfound;
    }

    return true;

notfound:
    PD_ERROR("could not find commit with hash '"PRI_SV"' anywhere.", ARG_SV(hash));
    return false;
}

f_internal bool readIDXFanout
(
    FILE     *file,
    uint32_t *fanouts
){
    uint8_t byte = 0;

    for(uint16_t i = 0; i < 256; ++i)
    {
        for(uint8_t j = 0; j < 4; ++j)
        {
            if(fread(&byte, 1, 1, file) != 1)
            {
                PD_ERROR("could not read fanout entry %"PRIu16".", j);
                return false;
            }

            fanouts[i] |= (uint32_t)(byte << (8 * (3 - j)));
        }
    }

    return true;
}

f_internal gfObjectOffset *readIDXV1
(
    FILE     *file,
    uint64_t packFileSize
){
    uint32_t       fanout[256] = {0};
    gfObjectOffset *objof      = 0;

    if(!readIDXFanout(file, fanout))
    {
        PD_ERROR("could not read fanouts of v1 IDX file.");
        goto closefile;
    }

    for(uint32_t i = 0; i < fanout[255]; ++i)
    {
        uint8_t  byte   = 0;
        uint32_t offset = 0;
        for(uint8_t j = 0; j < 4; ++j)
        {
            if(fread(&byte, 1, 1, file) != 1)
            {
                PD_ERROR("could not read offset of object %"PRIu32".", i);
                goto closefile;
            }

            offset |= (uint32_t)(byte << (8 * (3 - j)));
        }

        if(offset >= packFileSize)
        {
            PD_ERROR("pack offset is out of bounds.");
            goto closefile;
        }

        PD_WARN("TODO: handle v1 .idx files");
        goto closefile;
    }

closefile:
    fclose(file);
    return objof;
}

// PERF: probably do the same thing as I tried to do with the packfile. read the entire
// thing into memory first, then iterate byte-by-byte instead of reading byte-by-byte.
f_internal gfObjectOffset *readIDXV2
(
    FILE     *file,
    uint64_t packFileSize
){
    gfObjectOffset *objof = 0;

    uint8_t  byte    = 0;
    uint32_t version = 0;
    for(uint8_t i = 0; i < 4; ++i)
    {
        if(fread(&byte, 1, 1, file) != 1)
        {
            PD_ERROR("failed to read IDX v2 version number.");
            goto closefile;
        }

        version |= (uint32_t)(byte << (3 - i));
    }

    if(version != 2)
    {
        PD_ERROR("unknown IDX v2 version number: %"PRIu32, version);
        goto closefile;
    }

    uint32_t fanout[256] = {0};
    if(!readIDXFanout(file, fanout))
    {
        PD_ERROR("could not read fanouts of v2 IDX file.");
        goto closefile;
    }

    // TODO: figure out object ID size (20 or 32 bytes, SHA-1 or 256.)
    uint8_t oidSize = 20;

    pdArrReserve(objof, fanout[255]);

    for(uint32_t i = 0; i < fanout[255]; ++i)
    {
        char nameBuf[64] = {0};

        for(uint8_t j = 0; j < oidSize * 2; j += 2)
        {
            if(fread(&byte, 1, 1, file) != 1)
            {
                PD_ERROR("could not read name of object %"PRIu32".", i);
                pdArrFree(objof);
                goto closefile;
            }
            nameBuf[j]     = valueToHexChar(byte >> 4);
            nameBuf[j + 1] = valueToHexChar(byte & 0x0F);
        }

        StringView name = {0};
        name.data = nameBuf;
        name.size = oidSize * 2;

        gfObjectOffset oo = {0};
        oo.hash = sv_cpy(name);
        pdArrPush(objof, oo);
    }

    if(fseek(file, fanout[255] * 4, SEEK_CUR) != 0)
    {
        PD_ERROR("could not skip CRC table.");
        pdArrFree(objof);
        goto closefile;
    }

    for(uint32_t i = 0; i < fanout[255]; ++i)
    {
        uint32_t offset = 0;
        for(uint8_t j = 0; j < 4; ++j)
        {
            if(fread(&byte, 1, 1, file) != 1)
            {
                PD_ERROR("could not read offset of object %"PRIu32".", i);
                pdArrFree(objof);
                goto closefile;
            }

            offset |= (uint32_t)(byte << (8 * (3 - j)));
        }

        if(offset & 0x80000000)
        {
            offset &= 0x7FFFFFFF;
            // TODO: do something with these
            PD_WARN("TODO: handle object offset via index into large offset table: %"
                    PRIu32, offset);
            continue;
        }

        if(offset >= packFileSize)
        {
            PD_ERROR("invalid pack offset %"PRIu32" for filesize %"PRIu64,
                     offset, packFileSize);
            pdArrFree(objof);
            goto closefile;
        }

        objof[i].offset = offset;
    }

closefile:
    fclose(file);
    return objof;
}

f_internal void readCommitsFromOffsets
(
    StringView     path,
    gfObjectOffset *objof,
    gfCommitInfo   **commitTable
){
    FILE *file = fopen(path.data, "rb");
    if(!file)
    {
        PD_WARN("couldn't open pack file: '"PRI_SV"'", ARG_SV(path));
        return;
    }
    uint8_t *packFile = 0;

    fseek(file, 0, SEEK_END);
    uint64_t packFileSize = (uint64_t)ftell(file);
    packFile = malloc(packFileSize);

    fseek(file, 0, SEEK_SET);

    uint64_t elements = fread(packFile, 1, packFileSize, file);
    if(elements != packFileSize)
    {
        PD_WARN("couldn't read pack file into memory. tried to read %"PRIu64", but "
                "read %"PRIu64" instead.: '"PRI_SV"'",
                packFileSize, elements, ARG_SV(path));
        goto closefile;
    }

    uint64_t arraySize = pdArrSize(objof);
    PD_TRACE("reading %"PRIu64" commits from offsets into packfile.", arraySize);
    for(uint32_t i = 0; i < arraySize; ++i)
    {
        uint64_t index = objof[i].offset;
        uint8_t  byte  = packFile[index++];

        bool     readMore = byte >> 7;
        uint8_t  type     = byte >> 4 & 0x07;

        #ifdef DEBUG
        uint8_t  shift  = 4;
        uint64_t length = byte & 0x0F;
        #endif

        for(uint8_t j = 0; readMore && j < 10; ++j)
        {
            byte = packFile[index++];

            #ifdef DEBUG
            uint64_t chunk = byte & 0x7F;
            #endif

            PD_ASSERT(shift < 64, "cannot shift more than 64 bits.");
            PD_ASSERT(chunk < (UINT64_MAX >> shift), "chunk is too large.");

            readMore = byte  >> 7;

            #ifdef DEBUG
            length  |= chunk << shift;
            shift   += 7;
            #endif
        }
        PD_TRACE("parsed length from pack object %"PRIu32" (type %"PRIu8"): %"PRIu64"",
                 i, type, length);

        PD_ASSERT(type > 0 && type < 8, "invalid object type on obj %"PRIu32": %"PRIu32
                  ". read Byte: 0x%X", i, type, byte);

        if(index >= packFileSize)
        {
            PD_ERROR("pack offset is out of bounds.");
            goto closefile;
        }

        if(type == GF_OBJ_COMMIT)
        {
            PD_TRACE("reading commit from offset %"PRIu32"in packfile.",
                     objof[i].offset);

            gfCommitInfo commit = {0};
            DeflateInfo  dfInfo = readCommitFromPtr(&packFile[index], &commit);

            PD_ASSERT(dfInfo.bytesWritten == length, "expected to decompress into %"
                      PRIu64" bytes, actual: %"PRIu64".", length, dfInfo.bytesWritten);

            if(!dfInfo.success)
            {
                PD_WARN("could not successfully read commit object %"PRIu32" in pack "
                        "file '"PRI_SV"'.", i, ARG_SV(path));
                gfFreeCommit(&commit);
                goto closefile;
            }
            PD_ASSERT(length == dfInfo.bytesWritten, "did not write the expected "
                      "amount (%"PRIu64") of bytes, but instead %"PRIu64, length,
                      dfInfo.bytesWritten);

            uint8_t firstTwo = twoCharsToByte(objof[i].hash.data[0],
                                              objof[i].hash.data[1]);

            commit.hash = sv_cpy(objof[i].hash);

            pdArrPush(commitTable[firstTwo], commit);
        }
        else if(type == GF_OBJ_OFS_DELTA)
        {
            // variable-length negative offset
            // recursively read base object from curr - offset
            // read delta (inflate)
            // apply delta patch

            //
            PD_WARN("OBJ_OFS_DELTA unhandled.");
            goto closefile;
            //
        }
        else if(type == GF_OBJ_REF_DELTA)
        {
            // object name (find base object by name)
            // recursively read base object, looking up by object hash
            // read delta (inflate)
            // apply delta patch

            //
            PD_WARN("OBJ_REF_DELTA unhandled.");
            goto closefile;
            //
        }
    }

closefile:
    if(packFile)
    {
        free(packFile);
    }
    fclose(file);
}

f_internal void readPackedCommits
(
    StringView   idxPath,
    gfCommitInfo **commitTable
){
    FILE *file = fopen(idxPath.data, "rb");
    if(!file)
    {
        PD_ERROR("could not open .idx file: '"PRI_SV"'", ARG_SV(idxPath));
        return;
    }

    bool    v2   = true;
    uint8_t byte = 0;
    for(uint8_t i = 0; i < 4; ++i)
    {
        if(fread(&byte, 1, 1, file) != 1)
        {
            PD_ERROR("couldn't read first 4 bytes of .idx file: '"PRI_SV"'",
                     ARG_SV(idxPath));
            return;
        }

        if((char)byte != idxV2Magic.data[i])
        {
            v2 = false;
            break;
        }
    }

    char packBuf[4096] = {0};
    idxPath.size -= 4;
    StringView packPath = sv_concat(idxPath, cstr_sv(".pack"), packBuf);
    idxPath.size += 4;

    uint64_t packFileSize = 0;

    FILE *packFile = fopen(packPath.data, "rb");
    if(!packFile)
    {
        PD_ERROR("could not open packfile from path '%s'.", packPath.data);
        return;
    }
    else
    {
        fseek(packFile, 0, SEEK_END);
        packFileSize = (uint64_t)ftell(packFile);
    }
    fclose(packFile);

    gfObjectOffset *objof = 0;

    if(v2)
    {
        PD_TRACE("reading .idx v2 file: '"PRI_SV"'", ARG_SV(idxPath));
        objof = readIDXV2(file, packFileSize);
    }
    else
    {
        PD_TRACE("reading .idx v1 file: '"PRI_SV"'", ARG_SV(idxPath));
        fseek(file, 0, SEEK_SET);
        objof = readIDXV1(file, packFileSize);
    }

    if(!objof)
    {
        PD_ERROR("could not read any object-offset pairs.");
        return;
    }

    readCommitsFromOffsets(packPath, objof, commitTable);

    uint64_t arraySize = pdArrSize(objof);
    for(uint64_t i = 0; i < arraySize; ++i)
    {
        freeSV(&objof[i].hash);
    }
    pdArrFree(objof);
}

void gfInitRepository
(
    gfRepository *repo,
    gfCommitInfo *head,
    gfCommitInfo **commitTable
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
        if(sv_find(idxIdent, fileBuf[i]))
        {
            char tmpBuf[4096] = {0};
            StringView idxPath = sv_concat(gitPACK, fileBuf[i], tmpBuf);
            readPackedCommits(idxPath, commitTable);
        }
    }

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
        gfGetCommitInfo(absolute, readHead, head, 0);
        goto closefile;
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
        PD_WARN("TODO: resolve packed ref: '%s'", headBuf);
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
    gfGetCommitInfo(repo->path, hash, head, 0);

closefile:
    fclose(file);
}
