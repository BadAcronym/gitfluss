#include "gitfluss.h"
#include "gf_print_macros.h"

#include "datasurf_main.h"

#include "pd_path.h"
#include "pd_util.h"
#include "pd_dyn_arr.h"

#include <inttypes.h>
#include <memory.h>

#define TEN_MILLION 10000000

s_global const StringView sep             = { .size = 1,  .data = "/"                 };
s_global const StringView spaceKarat      = { .size = 2,  .data = " <"                };
s_global const StringView karatSpace      = { .size = 2,  .data = "> "                };
s_global const StringView idxIdent        = { .size = 4,  .data = ".idx"              };
s_global const StringView refIdent        = { .size = 4,  .data = "ref:"              };
s_global const StringView idxV2Magic      = { .size = 4,  .data = "\377tOc"           };
s_global const StringView authorIdent     = { .size = 6,  .data = "author"            };
s_global const StringView parentIdent     = { .size = 6,  .data = "parent"            };
s_global const StringView sha256Ident     = { .size = 6,  .data = "sha256"            };
s_global const StringView committerIdent  = { .size = 9,  .data = "committer"         };
s_global const StringView objFormatIdent  = { .size = 14, .data = "objectFormat ="    };
s_global const StringView gitObjectFolder = { .size = 14, .data = "/.git/objects/"    };
s_global const StringView headPackedIdent = { .size = 15, .data = "refs/heads/main"   };
s_global const StringView packedHeadIdent = { .size = 17, .data = "/.git/packed-refs" };

s_global const char hexValues[] = "0123456789abcdef";

f_internal void gfValueToHex
(
    const u08 *src,
    const u08 byteCount,
    char      *dst
){

    for(u08 i = 0; i < byteCount; ++i)
    {
        dst[2 * i]     = hexValues[src[i] >> 4];
        dst[2 * i + 1] = hexValues[src[i] & 0x0F];
    }
}

f_internal inline i64 readTimeFromStr
(
    String str
){
    i64 time = 0;

    for(u08 i = 0; i < str.size; ++i)
    {
        if(str.data[i] == 0x20 || str.data[i] == 0x0A || !str.data[i])
        {
            break;
        }
        time *= 10;
        time += str.data[i] - '0';
    }

    return time;
}

void gfFreeCommit
(
    gfCommitInfo *commit
){
    if(!commit)
    {
        return;
    }

    commit->authorTime    = 0;
    commit->committerTime = 0;

    pdSVFree(&commit->hash);
    pdSVFree(&commit->summary);

    if(commit->parentHashes)
    {
        for(u16 i = 0; i < pdArrSize(commit->parentHashes); ++i)
        {
            pdSVFree(&commit->parentHashes[i]);
        }
        pdArrFree(commit->parentHashes);
        commit->parentHashes = 0;
    }

    pdSVFree(&commit->authorName);
    pdSVFree(&commit->authorMail);
    pdSVFree(&commit->committerName);
    pdSVFree(&commit->committerMail);
}

f_internal bool readCommitFromPtr
(
    u08          *commitBuf,
    gfCommitInfo *commit,
    StringView   hash,
    u64          bufsize
){
    GF_ASSERT(commitBuf, "passed nullptr buffer to readCommitFromPtr.");
    GF_ASSERT(commit,    "passed nullptr commit to readCommitFromPtr.");
    GF_ASSERT(hash.data, "passed nullptr hash data to readCommitFromPtr.");

    commit->hash = hash;

    char lineBuf[1024] = {0};
    String line = {0};
    line.data = lineBuf;

    StringView commitBufSV = {0};
    commitBufSV.data = (char*)commitBuf;
    commitBufSV.size = bufsize;

    pdArrReserve(commit->parentHashes, 2);

    u64 index = 0;
    while(pdSVReadLine(commitBufSV, &index, &line, 1024))
    {
        if(pdSVFind(parentIdent, *((StringView*)&line)) == line.data)
        {
            line.data += parentIdent.size + 1;
            line.size -= parentIdent.size + 1;

            pdArrPush(commit->parentHashes, pdSVCpy(*((StringView*)&line)));
        }
        else if(pdSVFind(authorIdent, *((StringView*)&line)) == line.data)
        {
            const char *karatLoc = pdSVFind(spaceKarat, *((StringView*)&line));

            StringView authorName;
            authorName.data = line.data + authorIdent.size + 1;
            authorName.size = (u64)(karatLoc - authorName.data);

            line.data = line.data + authorName.size;
            line.size -= authorName.size + 2;

            const char *karatEndLoc = pdSVFind(karatSpace, *((StringView*)&line));

            StringView authorMail;
            authorMail.data = (char*)(authorName.data + authorName.size + 2);
            authorMail.size = (u64)(karatEndLoc - authorMail.data);

            line.data = (char*)authorMail.data + authorMail.size + 2;
            line.size -= authorMail.size + 2;

            commit->authorName = pdSVCpy(authorName);
            commit->authorMail = pdSVCpy(authorMail);
            commit->authorTime = readTimeFromStr(line);
        }
        else if(pdSVFind(committerIdent, *((StringView*)&line)) == line.data)
        {
            const char *karatLoc = pdSVFind(spaceKarat, *((StringView*)&line));

            StringView committerName;
            committerName.data = line.data + committerIdent.size + 1;
            committerName.size = (u64)(karatLoc - committerName.data);

            line.data = line.data + committerName.size;
            line.size -= committerName.size + 2;

            const char *karatEndLoc = pdSVFind(karatSpace, *((StringView*)&line));

            StringView committerMail;
            committerMail.data = (char*)(committerName.data + committerName.size + 2);
            committerMail.size = (u64)(karatEndLoc - committerMail.data);

            line.data = (char*)committerMail.data + committerMail.size + 2;
            line.size -= committerMail.size + 2;

            commit->committerName = pdSVCpy(committerName);
            commit->committerMail = pdSVCpy(committerMail);
            commit->committerTime = readTimeFromStr(line);
        }
    }

    if(pdSVReadLine(commitBufSV, &index, &line, 1024))
    {
        commit->summary = pdSVCpy(*((StringView*)&line));
    }

    // GF_TRACE("read commit:");
    // GF_TRACE("commit->hash:          '"PRI_SV"'", ARG_SV(commit->hash));
    // GF_TRACE("commit->summary:       '"PRI_SV"'", ARG_SV(commit->summary));

    // #ifdef GF_PRI_TRACE
    // if(commit->parentHashes)
    // {
    //     for(u16 i = 0; i < pdArrSize(commit->parentHashes); ++i)
    //     {
    //         GF_TRACE("commit->parentHash:    '"PRI_SV"'", ARG_SV(commit->parentHashes[i]));
    //     }
    // }
    // #endif

    // GF_TRACE("commit->authorName:    '"PRI_SV"'", ARG_SV(commit->authorName));
    // GF_TRACE("commit->authorMail:    '"PRI_SV"'", ARG_SV(commit->authorMail));
    // GF_TRACE("commit->committerName: '"PRI_SV"'", ARG_SV(commit->committerName));
    // GF_TRACE("commit->committerMail: '"PRI_SV"'", ARG_SV(commit->committerMail));
    // GF_TRACE("commit->authorTime:    %"PRIu64,    commit->authorTime);
    // GF_TRACE("commit->committerTime: %"PRIu64,    commit->committerTime);

    return true;
}

f_internal bool readObjectHeader
(
    u08        *packFile,
    u64        *index,
    u64        packFileSize,
    gfPackInfo *outInfo
){
    u08 byte  = packFile[(*index)++];
    u64 chunk = 0;
    u08 shift = 4;
    outInfo->size = byte & 0x0F;
    outInfo->type = byte >> 4 & 0x07;

    GF_TRACE("identified type: %"PRIu8, outInfo->type);

    while(byte & 0x80)
    {
        if(*index >= packFileSize)
        {
            GF_ERROR("readObjectHeader index out of bounds. index: %"PRIu64", "
                     "fileSize: %"PRIu64, *index, packFileSize);
            return false;
        }

        byte  = packFile[(*index)++];
        chunk = byte & 0x7F;

        GF_ASSERT(shift < 64, "cannot shift more than 64 bits.");
        GF_ASSERT(chunk < (U64MAX >> shift), "chunk is too large.");

        outInfo->size |= chunk << shift;
        shift         += 7;
    }
    GF_ASSERT(outInfo->type > 0 && outInfo->type < 8, "invalid object type on obj: %"
              PRIu32". read Byte: 0x%X", outInfo->type, byte);

    return true;
}

f_internal void readCommitFromFile
(
    StringView   path,
    StringView   hash,
    gfCommitInfo *commit
){
    GF_TRACE("opening to read commit from path: '"PRI_SV"'", ARG_SV(path));

    char pathBuf[path.size + 1];
    pdSVCstr(path, pathBuf);

    FILE *file = fopen(pathBuf, "rb");
    if(!file)
    {
        GF_WARN("failed to open commit file: '%s'", pathBuf);
        return;
    }

    u08 zlibBuf[1024] = {0};

    fread(&zlibBuf, 1024, 1, file);

    GF_TRACE("reading commit from loose object.");

    u08         *dstBuf = malloc(GF_BUFSIZE * 4);
    DeflateInfo dfInfo  = dsReadZlibPtr(zlibBuf, dstBuf, GF_BUFSIZE * 4);
    if(!dfInfo.success)
    {
        goto closefile;
    }

    readCommitFromPtr(dstBuf, commit, pdSVCpy(hash), GF_BUFSIZE * 4);
    free(dstBuf);

closefile:
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
        GF_ERROR("commit that was passed is nullptr.");
        return false;
    }

    GF_ASSERT(repository.data && repository.size, "cannot open null repository.");

    GF_ASSERT(hash.size == 40 || hash.size == 64, "commit hash has invalid size: %"
              PRIu64". should be either 40 or 64 characters big. passed hash was: '"
              PRI_SV"'", hash.size, ARG_SV(hash));

    GF_ASSERT(hash.data, "cannot lookup commit with no hash.");

    GF_TRACE("looking for commit with hash: "PRI_SV, ARG_SV(hash));

    // ensure we have a clean slate to work with
    gfFreeCommit(commit);

    StringView hashStart = pdSVCpy(hash);
    hashStart.size = 2;

    StringView hashRest = hash;
    pdSVTrim(&hashRest, 2, SV_LEFT);

    char pathBuf[GF_MAX_PATH];
    StringView commitPath = pdSVConcat(repository, gitObjectFolder, pathBuf);
    commitPath = pdSVConcat(commitPath, hashStart, pathBuf);
    commitPath = pdSVConcat(commitPath, sep, pathBuf);
    commitPath = pdSVConcat(commitPath, hashRest, pathBuf);

    u08 result = pdVerifyPath(commitPath);
    if(result == PD_TYPE_FILE)
    {
        readCommitFromFile(commitPath, hash, commit);
        pdSVFree(&hashStart);
        return true;
    }

    GF_TRACE("path '"PRI_SV"' does not exist. looking in packfile table...",
             ARG_SV(commitPath));

    bool found = false;

    if(!table)
    {
        GF_TRACE("table does not exist.");
        goto notfound;
    }

    u08 firstTwo = gfTwoCharsToByte(hash.data[0], hash.data[1]);
    if(!table[firstTwo])
    {
        GF_TRACE("table[0x%x] has no data.", firstTwo);
        goto notfound;
    }

    u64 arraySize = pdArrSize(table[firstTwo]);
    GF_TRACE("looking for commit in table[0x%c%c]: size %"PRIu64,
             hash.data[0], hash.data[1], arraySize);
    for(u64 i = 0; i < arraySize; ++i)
    {
        gfCommitInfo tmp = table[firstTwo][i];

        GF_TRACE("checking against commit in table[0x%x]: "PRI_SV,
                 firstTwo, ARG_SV(tmp.hash));

        if(pdSVSame(hash, tmp.hash))
        {
            commit->hash          = pdSVCpy(tmp.hash);
            commit->summary       = pdSVCpy(tmp.summary);
            commit->authorName    = pdSVCpy(tmp.authorName);
            commit->authorMail    = pdSVCpy(tmp.authorMail);
            commit->committerName = pdSVCpy(tmp.committerName);
            commit->committerMail = pdSVCpy(tmp.committerMail);
            commit->authorTime    = tmp.authorTime;
            commit->committerTime = tmp.committerTime;

            if(tmp.parentHashes)
            {
                for(u64 j = 0; j < pdArrSize(tmp.parentHashes); ++j)
                {
                    pdArrPush(commit->parentHashes, pdSVCpy(tmp.parentHashes[j]));
                }
            }

            GF_TRACE("found commit!");
            found = true;
            break;
        }
    }

    if(!found)
    {
        goto notfound;
    }

    pdSVFree(&hashStart);
    return true;

notfound:
    GF_ERROR("could not find commit with hash '"PRI_SV"' anywhere.", ARG_SV(hash));
    pdSVFree(&hashStart);
    return false;
}

f_internal bool readIDXFanout
(
    u08 *idxFile,
    u64 *index,
    u32 *fanouts
){
    for(u16 i = 0; i < 256; ++i)
    {
        for(u08 j = 0; j < 4; ++j)
        {
            fanouts[i] |= (u32)(idxFile[(*index)++]<< (8 * (3 - j)));
        }
    }

    return true;
}

f_internal gfObjectOffset *readIDXV1
(
    u08 *idxFile,
    u64 *index,
    u08 oidSize,
    u64 packFileSize
){
    u32            fanout[256] = {0};
    gfObjectOffset *objof      = 0;

    //
    GF_WARN("TODO: .idx V1 files unhandled.");
    // -Wunused
    printf("-Wunused: hash size: %u\n", oidSize * 2);
    return 0;
    //

    if(!readIDXFanout(idxFile, index, fanout))
    {
        GF_ERROR("could not read fanouts of v1 IDX file.");
        return 0;
    }

    for(u32 i = 0; i < fanout[255]; ++i)
    {
        u32 offset = 0;
        for(u08 j = 0; j < 4; ++j)
        {
            offset |= (u32)(idxFile[(*index)++] << (8 * (3 - j)));
        }

        if(offset >= packFileSize)
        {
            GF_ERROR("pack offset is out of bounds.");
            return 0;
        }
    }

    return objof;
}

f_internal gfObjectOffset *readIDXV2
(
    u08 *idxFile,
    u64 *index,
    u08 oidSize,
    u64 packFileSize
){
    gfObjectOffset *objof = 0;

    pdArrReserve(objof, 1024);

    u32 version = 0;
    for(u08 j = 0; j < 4; ++j)
    {
        version |= (u32)idxFile[(*index)++] << (3 - j) * 8;
    }

    if(version != 2)
    {
        GF_ERROR("unknown IDX v2 version number: %"PRIu32, version);
        goto closefile;
    }

    u32 fanout[256] = {0};
    if(!readIDXFanout(idxFile, index, fanout))
    {
        GF_ERROR("could not read fanouts of v2 IDX file.");
        goto closefile;
    }

    pdArrReserve(objof, fanout[255]);

    for(u32 i = 0; i < fanout[255]; ++i)
    {
        char nameBuf[oidSize * 2];
        gfValueToHex(&idxFile[*index], oidSize, nameBuf);
        *index += oidSize;

        StringView name;
        name.data = nameBuf;
        name.size = oidSize * 2;

        gfObjectOffset oo;
        oo.hash = pdSVCpy(name);
        pdArrPush(objof, oo);
        // GF_TRACE("pushed object with hash '"PRI_SV"' to object-offset table.",
        //          ARG_SV(oo.hash));
    }

    *index += fanout[255] * 4;

    for(u32 i = 0; i < fanout[255]; ++i)
    {
        u32 offset = 0;
        for(u08 j = 0; j < 4; ++j)
        {
            offset |= (u32)idxFile[(*index)++] << (3 - j) * 8;
        }

        if(offset & 0x80000000)
        {
            offset &= 0x7FFFFFFF;
            // TODO: do something with these
            GF_WARN("object offset via index into large offset table unhandled: %"
                    PRIu32, offset);
            continue;
        }

        if(offset >= packFileSize)
        {
            GF_ERROR("invalid pack offset %"PRIu32" for filesize %"PRIu64,
                     offset, packFileSize);
            pdArrFree(objof);
            goto closefile;
        }

        objof[i].offset = offset;
    }

closefile:
    return objof;
}

f_internal u64 readDeltaSize
(
    u08 *buf,
    u64 *cursor,
    u64 bufsize
){
    u08 byte  = buf[(*cursor)++];
    u64 size  = byte & 0x7F;
    u64 chunk = 0;
    u08 shift = 7;

    while(byte & 0x80)
    {
        if(*cursor >= bufsize)
        {
            GF_ERROR("cursor out of bounds from readDeltaSize.");
            return 0;
        }

        byte  = buf[(*cursor)++];
        chunk = byte & 0x7F;

        GF_ASSERT(shift < 64, "cannot shift more than 64 bits.");
        GF_ASSERT(chunk < (U64MAX >> shift), "chunk is too large.");

        size  |= chunk << shift;
        shift += 7;
    }

    GF_TRACE("read delta size of %"PRIu64" from cursor %"PRIu64, size, *cursor);

    return size;
}

f_internal u08 *readDelta
(
    u08        *packFile,
    u64        *index,
    u64        deltaDataSize,
    gfPackInfo *outInfo
){
    GF_TRACE("reading delta...");

    if(!outInfo->data)
    {
        GF_ERROR("outInfo->data is nil. cannot actually read delta object.");
        return false;
    }

    u08 *deltaDataBuf = malloc(deltaDataSize);
    if(!deltaDataBuf)
    {
        GF_ERROR("failed to allocate buffer for delta data, size %"PRIu64,
                 deltaDataSize);
        return 0;
    }

    DeflateInfo info = dsReadZlibPtr(&packFile[*index], deltaDataBuf, deltaDataSize);
    if(!info.success)
    {
        GF_ERROR("failed to read Zlib compressed data from packFile at index %"PRIu64
                 ". Read %"PRIu64" compressed bytes and wrote %"PRIu64" bytes.",
                 *index, info.bytesRead, info.bytesWritten);
        free(deltaDataBuf);
        return 0;
    }

    return deltaDataBuf;
}

f_internal bool applyDelta
(
    u64        deltaDataSize,
    u08        *deltaDataBuf,
    gfPackInfo *outInfo,
    u64        *cursorPos,
    u64        baseSize,
    u64        resultSize
){
    GF_TRACE("applying delta...");

    u64 cursor = *cursorPos;

    #ifdef TRACE
    printf("full bytes:\n");
    for(u64 i = 0; i < deltaDataSize; ++i)
    {
        printf("0x%"PRIx8" ", deltaDataBuf[i]);
    }
    printf("\n");
    #endif

    u08 *resultObjBuf = malloc(resultSize);
    if(!resultObjBuf)
    {
        GF_ERROR("failed to allocate buffer for result object, size %"PRIu64,
                 resultSize);
        goto error;
    }

    if(baseSize != outInfo->size)
    {
        GF_ERROR("baseSize %"PRIu64" does not match base object size %"PRIu64
                 " read from base object.", baseSize, outInfo->size);
        GF_TRACE("failed to apply delta.");
        free(resultObjBuf);
        goto error;
    }

    u64 resultIndex = 0;
    while(cursor < deltaDataSize)
    {
        u08 opcode = deltaDataBuf[cursor++];

        if(opcode & 0x80)
        {
            u64 copySize   = 0;
            u64 copyOffset = 0;

            for(u08 bit = 0; bit < 4; ++bit)
            {
                if(opcode & (1u << bit))
                {
                    if(cursor >= deltaDataSize)
                    {
                        GF_ERROR("(copyOffset parsing) cursor %"PRIu64" >= "
                                 "deltaDataSize %"PRIu64, cursor, deltaDataSize);
                        free(resultObjBuf);
                        goto error;
                    }

                    copyOffset |= (u64)deltaDataBuf[cursor++] << (bit * 8);
                }
            }

            for(u08 bit = 0; bit < 3; ++bit)
            {
                if(opcode & (1u << (bit + 4)))
                {
                    if(cursor >= deltaDataSize)
                    {
                        GF_ERROR("(copySize parsing) cursor %"PRIu64" >= deltaDataSize "
                                 "%"PRIu64, cursor, deltaDataSize);
                        free(resultObjBuf);
                        goto error;
                    }

                    copySize |= (u64)deltaDataBuf[cursor++] << (bit * 8);
                }
            }

            if(copySize == 0)
            {
                copySize = 0x10000;
            }

            if(copyOffset > baseSize || copySize > baseSize - copyOffset)
            {
                GF_ERROR("delta copy outside base object. offset: %"PRIu64", size: %"
                         PRIu64", baseSize: %"PRIu64, copyOffset, copySize, baseSize);
                free(resultObjBuf);
                goto error;
            }

            if(resultIndex > resultSize || copySize > resultSize - resultIndex)
            {
                GF_ERROR("delta copy outside result object: resultIndex: %"PRIu64
                         ", size: %"PRIu64", resultSize: %"PRIu64,
                         resultIndex, copySize, resultSize);
                free(resultObjBuf);
                goto error;
            }

            memcpy(resultObjBuf + resultIndex, outInfo->data + copyOffset, copySize);
            resultIndex += copySize;
        }
        else if(!opcode)
        {
            GF_ERROR("git delta instruction 0 is reserved.");
            free(resultObjBuf);
            goto error;
        }
        else
        {
            u08 size = opcode & 0x7F;

            if(resultIndex > resultSize || size > resultSize - resultIndex)
            {
                GF_ERROR("delta write outside result object: resultIndex: %"PRIu64
                         ", size: %"PRIu32", resultSize: %"PRIu64,
                         resultIndex, size, resultSize);
                free(resultObjBuf);
                goto error;
            }

            memcpy(resultObjBuf + resultIndex, deltaDataBuf + cursor, size);
            resultIndex += size;
            cursor      += size;
        }
    }

    GF_ASSERT(resultIndex == resultSize, "resultIndex %"PRIu64" does not match "
              "resultSize %"PRIu64".", resultIndex, resultSize);

    free(deltaDataBuf);
    outInfo->data = resultObjBuf;
    outInfo->size = resultSize;
    return true;

error:
    free(deltaDataBuf);
    return false;
}

f_internal bool readAndApplyDelta
(
    u08        *packFile,
    u64        *index,
    u64        deltaDataSize,
    gfPackInfo *outInfo
){
    u08 *deltaDataBuf = readDelta(packFile, index, deltaDataSize, outInfo);

    u64 cursor = 0;
    u64 base   = readDeltaSize(deltaDataBuf, &cursor, deltaDataSize);
    u64 result = readDeltaSize(deltaDataBuf, &cursor, deltaDataSize);

    return applyDelta(deltaDataSize, deltaDataBuf, outInfo, &cursor, base, result);
}

f_internal bool resolveObjOnDisk
(
    gfRecurseInfo *info
){
    StringView hashStart = info->hash;
    hashStart.size = 2;

    StringView hashRest = info->hash;
    pdSVTrim(&hashRest, 2, SV_LEFT);

    char pathBuf[GF_MAX_PATH];

    StringView objPath = pdSVConcat(info->repository, gitObjectFolder, pathBuf);
    objPath = pdSVConcat(objPath, hashStart, pathBuf);
    objPath = pdSVConcat(objPath, sep, pathBuf);
    objPath = pdSVConcat(objPath, hashRest, pathBuf);

    char objPathBuf[objPath.size + 1];
    pdSVCstr(objPath, pathBuf);

    u08 result = pdVerifyPath(objPath);
    if(result == PD_TYPE_FILE)
    {
        GF_TRACE("opening to read object from path: '"PRI_SV"'", ARG_SV(objPath));

        BinBuffer zlibBuf = pdReadBinFile(objPathBuf);

        // TODO: resolve object rest of the way here and add the resolved
        // object to the cache!!

        //
        printf("-Wunused on buffer: %p\n", zlibBuf.data);
        GF_WARN("reading generic object from loose file unimplemented.");
        return false;
        //

        GF_TRACE("reading object from loose file.");
        return true;
    }

    return false;
}

// this function is not very good, but I can't really be bothered right now to make it
// not suck. reading the MIDX would be an improvement.
f_internal gfHashLocation findObjectOffset
(
    StringView objectsPath,
    StringView hash
){
    gfHashLocation loc = {0};

    char packBuf[GF_MAX_PATH];
    StringView pack = pdCstrSV("/pack/");

    pack = pdSVConcat(objectsPath, pack, packBuf);

    char       listBuf[GF_MAX_PATH * 2];
    StringView list          = pdListFiles(pack, listBuf);
    u64        fileCount     = pdSVCountByDelim(list, ';');

    StringView fileBuf[fileCount];
    for(u64 i = 0; i < fileCount; ++i)
    {
        fileBuf[i].data = 0;
        fileBuf[i].size = 0;
    }

    pdSVSeparateByDelim(list, fileBuf, ';', fileCount);

    for(u64 f = 0; f < fileCount; ++f)
    {
        if(pdSVFind(idxIdent, fileBuf[f]))
        {
            char tmpBuf[GF_MAX_PATH];
            StringView idxPath = pdSVConcat(pack, fileBuf[f], tmpBuf);

            FILE *idxFile = fopen(tmpBuf, "rb");
            if(!idxFile)
            {
                GF_ERROR("failed to open idxFile from findObjectOffset.");
                return (gfHashLocation){0};
            }

            fseek(idxFile, 0, SEEK_END);
            u64 idxFileSize = (u64)ftell(idxFile);
            u08 *idxFileBuf = malloc(idxFileSize);
            fseek(idxFile, 0, SEEK_SET);

            u64 elements = fread(idxFileBuf, 1, idxFileSize, idxFile);
            if(elements != idxFileSize)
            {
                GF_WARN("couldn't read .idx idxFile into memory. tried to read %"PRIu64", but "
                        "read %"PRIu64" instead.: '"PRI_SV"'",
                        idxFileSize, elements, ARG_SV(idxPath));
                return (gfHashLocation){0};
            }
            fclose(idxFile);

            bool v2   = true;
            for(u08 i = 0; i < 4; ++i)
            {
                if((char)idxFileBuf[i] != idxV2Magic.data[i])
                {
                    v2 = false;
                }
            }

            u64 index = 4;
            u32 foundIndex = U32MAX;

            if(v2)
            {
                u32 version = 0;
                for(u08 i = 0; i < 4; ++i)
                {
                    version |= (u32)idxFileBuf[index++] << ((3 - i) * 8);
                }

                if(version != 2)
                {
                    GF_ERROR("unknown IDX v2 version number: %"PRIu32, version);
                    return (gfHashLocation){0};
                }

                u32 fanout[256] = {0};
                if(!readIDXFanout(idxFileBuf, &index, fanout))
                {
                    GF_ERROR("could not read fanouts of v2 IDX file.");
                    return (gfHashLocation){0};
                }

                for(u32 i = 0; i < fanout[255]; ++i)
                {
                    char nameBuf[hash.size];
                    gfValueToHex(&idxFileBuf[index], (u08)(hash.size / 2), nameBuf);
                    index += (u08)hash.size / 2;

                    StringView name = {0};
                    name.data = nameBuf;
                    name.size = hash.size;

                    if(pdSVSame(hash, name))
                    {
                        GF_TRACE("FOUND OBJECT '"PRI_SV"' IN FILE '"PRI_SV
                                 "' AT INDEX %"PRIu32,
                                 ARG_SV(hash), ARG_SV(idxPath), i);
                        foundIndex = i;
                        break;
                    }
                }

                u64 offsetTablePos = 8 + 256 * 4 + fanout[255] * hash.size / 2 +
                                     fanout[255] * 4;
                u64 entryPos = offsetTablePos + (u64)foundIndex * 4;

                index = entryPos;
                // if(fseek(idxFile, (long)entryPos, SEEK_SET) != 0)
                // {
                //     GF_ERROR("could not seek to object offset.");
                //     fclose(idxFile);
                //     return (gfHashLocation){0};
                // }

                u32 offset = 0;
                for(u08 j = 0; j < 4; ++j)
                {
                    offset |= (u32)(idxFileBuf[index++] << (8 * (3 - j)));
                }

                if(offset & 0x80000000)
                {
                    offset &= 0x7FFFFFFF;
                    // TODO: do something with these
                    GF_WARN("object offset via index into large offset table unhandled:"
                            " %" PRIu32, offset);
                    return (gfHashLocation){0};
                }

                char packFileBuf[idxPath.size + 2];
                packFileBuf[idxPath.size + 1] = '\0';

                pdSVTrim(&idxPath, 3, SV_RIGHT);
                pdSVConcat(idxPath, pdCstrSV("pack"), packFileBuf);

                StringView packFile = pdCstrSV(packFileBuf);

                loc.index        = offset;
                loc.packFilePath = pdSVCpy(packFile);
            }
            else
            {
                //
                GF_ERROR("IDX v1 from findObjectOffset unhandled.");
                //
            }

            free(idxFileBuf);
            return loc;
        }
    }

    return (gfHashLocation){0};
}

f_internal bool resolveObjRecurse
(
    gfRecurseInfo *info
){
    u64        entryStart = *info->index;
    gfPackInfo **cache    = info->cache;
    u08        *packFile  = info->packFile;
    u64        *index     = info->index;
    u08        oidSize    = info->oidSize;
    u64        fileSize   = info->fileSize;
    gfPackInfo *outInfo   = info->outInfo;

    GF_ASSERT(cache, "no cache present, cannot resolve recursively.");

    outInfo->offset = entryStart;

    u08        lastTwo = entryStart & 0xFF;
    gfPackInfo *arr    = cache[lastTwo];
    if(!arr)
    {
        GF_TRACE("no objects in cache[0x%"PRIx8"]. skipping...", lastTwo);
        goto nocache;
    }

    u64 arrSize = pdArrSize(arr);
    GF_TRACE("LOOKING FOR CACHED OBJECT FOR OFFSET: %"PRIu64, outInfo->offset);
    for(u16 i = 0; i < arrSize; ++i)
    {
        if(arr[i].offset == entryStart)
        {
            GF_TRACE("FOUND CACHED OBJECT FOR OFFSET: %"PRIu64, outInfo->offset);
            outInfo->data = arr[i].data;
            outInfo->size = arr[i].size;
            outInfo->type = arr[i].type;
            return true;
        }
    }

nocache:
    if(*index == U64MAX)
    {
        GF_TRACE("resolving by name and not by offset.");
        if(resolveObjOnDisk(info))
        {
            return true;
        }

        gfHashLocation result = findObjectOffset(info->repository, info->hash);

        char packFileBuf[result.packFilePath.size + 1];
        pdSVCstr(result.packFilePath, packFileBuf);

        FILE *packFileFile = fopen(packFileBuf, "rb");
        if(!packFileFile)
        {
            GF_ERROR("failed to open packfile: '%s'", packFileBuf);
            return false;
        }
        pdSVFree(&result.packFilePath);

        fseek(packFileFile, 0, SEEK_END);
        u64 packFileSize  = (u64)ftell(packFileFile);
        u08 *packFileRead = malloc(packFileSize);
        fseek(packFileFile, 0, SEEK_SET);

        if(fread(packFileRead, packFileSize, 1, packFileFile) != 1)
        {
            GF_ERROR("failed to read new packfile.");
            fclose(packFileFile);
            return false;
        }

        fclose(packFileFile);

        u08        *savedPackFile = info->packFile;
        u64         savedFileSize = info->fileSize;
        u64        *savedIndex    = info->index;
        StringView  savedHash     = info->hash;

        info->packFile = packFileRead;
        info->fileSize = packFileSize;
        info->index    = &result.index;

        bool success = resolveObjRecurse(info);

        info->packFile = savedPackFile;
        info->fileSize = savedFileSize;
        info->index    = savedIndex;
        info->hash     = savedHash;

        free(packFileRead);
        return success;
    }

    GF_TRACE("resolving by offset...");

    if(!readObjectHeader(packFile, index, fileSize, outInfo))
    {
        GF_ERROR("failed to read object header from index %"PRIu64, *index);
        return false;
    }

    GF_ASSERT(outInfo->type != 0, "object of type 0 is invalid.");
    GF_ASSERT(outInfo->type != GF_OBJ_RESERVED, "object of type 5 is reserved.");
    GF_ASSERT(outInfo->type <= GF_OBJ_REF_DELTA, "object of type %"PRIu8" is greater "
              "than the maxiumum of 7.", outInfo->type);

    if(outInfo->type == GF_OBJ_TREE ||
       outInfo->type == GF_OBJ_BLOB ||
       outInfo->type == GF_OBJ_TAG
    ){
        GF_TRACE("skipping base object of type %"PRIu8, outInfo->type);
        return true;
    }
    else if(outInfo->type == GF_OBJ_COMMIT)
    {
        GF_TRACE("identified commit object.");

        u08 *buf = calloc(outInfo->size, 1);
        if(!buf)
        {
            GF_ERROR("failed to allocate new outInfo->data of size %"PRIu64,
                     outInfo->size);
            return false;
        }

        GF_TRACE("reading commit from offset %"PRIu64" in packfile. Bufsize: %"PRIu64,
                 *index, outInfo->size);

        DeflateInfo dfInfo = dsReadZlibPtr(&packFile[*index], buf, outInfo->size);

        if(!dfInfo.success)
        {
            GF_ERROR("could not successfully read base object from offset %"PRIu64,
                     *index);
            free(buf);
            return false;
        }

        GF_ASSERT(dfInfo.bytesWritten == outInfo->size, "expected to decompress into %"
                  PRIu64" bytes, actual: %"PRIu64".",
                  outInfo->size, dfInfo.bytesWritten);

        outInfo->data   = buf;
        outInfo->offset = entryStart;
        GF_TRACE("CACHING OBJECT (COMMIT) FOR OFFSET: %"PRIu64, outInfo->offset);

        pdArrPush(cache[lastTwo], *outInfo);
        return true;
    }
    else if(outInfo->type == GF_OBJ_OFS_DELTA)
    {
        GF_TRACE("identified ofs delta object.");

        u08 byte   = packFile[(*index)++];
        u64 offset = byte & 0x7F;

        while(byte & 0x80)
        {
            if(*index >= fileSize)
            {
                GF_ERROR("index out of bounds from OBJ_OFS_DELTA.");
                return false;
            }
            byte   = packFile[(*index)++];
            offset = ((offset + 1) << 7) | (byte & 0x7F);
        }

        if(offset >= entryStart)
        {
            GF_ERROR("negative offset %"PRIu64" is larger than current position of "
                     "file %"PRIu64".", offset, entryStart);
            return false;
        }

        GF_TRACE("read OFS_DELTA object with offset -%"PRIu64, offset);

        u64 indexRec  = (entryStart - offset);
        GF_TRACE("jumping from packFile entryStart %"PRIu64" back to index %"PRIu64
                 "...", entryStart, indexRec);

        u64 deltaSize = outInfo->size;
        info->firstCall = false;
        info->index     = &indexRec;

        if(!resolveObjRecurse(info))
        {
            GF_ERROR("could not read recursive object from OBJ_OFS_DELTA.");
            return false;
        }

        if(info->outInfo->type != GF_OBJ_COMMIT)
        {
            GF_TRACE("skipping non-commit base object.");
            return true;
        }

        if(!readAndApplyDelta(packFile, index, deltaSize, outInfo))
        {
            GF_ERROR("could not apply delta from OBJ_OFS_DELTA.");
            return false;
        }

        outInfo->offset = entryStart;
        GF_TRACE("CACHING OBJECT (OFS_DELTA) FOR OFFSET: %"PRIu64, outInfo->offset);

        pdArrPush(cache[lastTwo], *outInfo);
        return true;
    }
    else if(outInfo->type == GF_OBJ_REF_DELTA)
    {
        GF_TRACE("identified ref delta object.");

        char nameBuf[oidSize * 2];
        gfValueToHex(&packFile[*index], oidSize, nameBuf);
        *index += oidSize;

        StringView name = {0};
        name.data = nameBuf;
        name.size = oidSize * 2;

        u64 deltaSize = outInfo->size;

        GF_TRACE("read REF_DELTA object '"PRI_SV"' of size: %"PRIu64,
                 ARG_SV(name), outInfo->size);

        u64 noIndex   = U64MAX;
        info->firstCall = false;
        info->index     = &noIndex;
        info->hash      = name;

        if(!resolveObjRecurse(info))
        {
            GF_ERROR("could not read recursive object from OBJ_OFS_DELTA.");
            return false;
        }

        if(info->outInfo->type != GF_OBJ_COMMIT)
        {
            GF_TRACE("skipping non-commit base object.");
            return true;
        }

        if(!readAndApplyDelta(packFile, index, deltaSize, outInfo))
        {
            GF_ERROR("could not apply delta from OBJ_OFS_DELTA.");
            return false;
        }

        outInfo->offset = entryStart;
        GF_TRACE("CACHING OBJECT (REF_DELTA) FOR OFFSET: U64MAX");

        pdArrPush(cache[U64MAX & 0xFF], *outInfo);
        return true;
    }

    return false;
}

f_internal void readCommitsFromOffsets
(
    StringView     path,
    gfObjectOffset *objof,
    gfPackInfo     **cache,
    gfCommitInfo   **table,
    gfThreadLock   *tableLocks,
    u08            oidSize
){
    char pathCstr[path.size + 1];
    pdSVCstr(path, pathCstr);
    path.data = pathCstr;

    GF_TRACE("opening packFile: '"PRI_SV"'...", ARG_SV(path));
    BinBuffer packFile = pdReadBinFile(path.data);
    if(!packFile.data)
    {
        GF_WARN("couldn't open pack file: '"PRI_SV"'", ARG_SV(path));
        return;
    }

    StringView repository = path;
    size trimCount = path.size - ((size)(pdSVFind(gitObjectFolder, path) - path.data) +
                                         gitObjectFolder.size);
    pdSVTrim(&repository, trimCount, SV_RIGHT);

    gfRecurseInfo info = {0};
    info.repository = repository;
    info.packFile   = packFile.data;
    info.fileSize   = packFile.size;
    info.cache      = cache;
    info.oidSize    = oidSize;

    u64 arraySize = pdArrSize(objof);

    GF_ASSERT(arraySize < U32MAX, "arraySize >= U32MAX!");
    GF_TRACE("reading %"PRIu64" objects from offsets into packfile.", arraySize);

    for(u64 i = 0; i < arraySize; ++i)
    {
        gfPackInfo outInfo = {0};
        u64        index   = objof[i].offset;

        info.hash      = objof[i].hash;
        info.index     = &index;
        info.outInfo   = &outInfo;
        info.firstCall = true;

        GF_TRACE("resolving '"PRI_SV"' at offset %"PRIu32" from "
                 "readCommitsFromOffsets...", ARG_SV(objof[i].hash), objof[i].offset);

        if(!resolveObjRecurse(&info))
        {
            GF_ERROR("could not resolve '"PRI_SV"' recursively.",
                     ARG_SV(objof[i].hash));
            free(packFile.data);
            return;
        }

        if(outInfo.type != GF_OBJ_COMMIT)
        {
            GF_TRACE("'"PRI_SV"' is not a commit, but of type: %"PRIu8". skipping...",
                     ARG_SV(objof[i].hash), outInfo.type);
            continue;
        }

        GF_TRACE("resolved '"PRI_SV"' from readCommitsFromOffsets.",
                 ARG_SV(objof[i].hash));

        gfCommitInfo commit = {0};
        StringView   hash   = pdSVCpy(objof[i].hash);

        gfFreeCommit(&commit);
        readCommitFromPtr(outInfo.data, &commit, hash, outInfo.size);

        u08 firstTwo = gfTwoCharsToByte(hash.data[0], hash.data[1]);
        gfLock(&tableLocks[firstTwo]);
        pdArrPush(table[firstTwo], commit);
        gfUnlock(&tableLocks[firstTwo]);
    }

    for(u16 i = 0; i < 256; ++i)
    {
        if(!cache[i])
        {
            continue;
        }

        u64 arrSize = pdArrSize(cache[i]);
        for(u64 j = 0; j < arrSize; ++j)
        {
            if(cache[i][j].data)
            {
                free(cache[i][j].data);
            }
        }

        pdArrFree(cache[i]);
    }

    free(packFile.data);
}

f_internal void threadReadPackfile
(
    void *args
){
    gfPackedThreadData *data = (gfPackedThreadData*)args;

    StringView   idxPath     = data->idxPath;
    gfCommitInfo **table     = data->table;
    gfThreadLock *tableLocks = data->tableLocks;
    u08          oidSize     = data->oidSize;

    FILE *file = fopen(idxPath.data, "rb");
    if(!file)
    {
        GF_ERROR("could not open .idx file: '"PRI_SV"'", ARG_SV(idxPath));
        return;
    }

    fseek(file, 0, SEEK_END);
    u64 fileSize    = (u64)ftell(file);
    u08 *idxFileBuf = malloc(fileSize);
    fseek(file, 0, SEEK_SET);

    u64 elements = fread(idxFileBuf, 1, fileSize, file);
    if(elements != fileSize)
    {
        GF_WARN("couldn't read .idx file into memory. tried to read %"PRIu64", but "
                "read %"PRIu64" instead.: '"PRI_SV"'",
                fileSize, elements, ARG_SV(idxPath));
        return;
    }
    fclose(file);

    bool v2 = true;
    for(u08 i = 0; i < 4; ++i)
    {
        if((char)idxFileBuf[i] != idxV2Magic.data[i])
        {
            v2 = false;
            break;
        }
    }

    u64 index = 4;

    char packBuf[GF_MAX_PATH];
    idxPath.size -= 4;
    StringView packPath = pdSVConcat(idxPath, pdCstrSV(".pack"), packBuf);
    idxPath.size += 4;

    u64 packFileSize = 0;

    FILE *packFile = fopen(packPath.data, "rb");
    if(!packFile)
    {
        GF_ERROR("could not open packfile from path '%s'.", packPath.data);
        return;
    }
    else
    {
        fseek(packFile, 0, SEEK_END);
        packFileSize = (u64)ftell(packFile);
    }

    gfObjectOffset *objof = 0;

    if(v2)
    {
        GF_TRACE("reading .idx v2 file: '"PRI_SV"'", ARG_SV(idxPath));
        objof = readIDXV2(idxFileBuf, &index, oidSize, packFileSize);
    }
    else
    {
        GF_TRACE("reading .idx v1 file: '"PRI_SV"'", ARG_SV(idxPath));
        index = 0;
        objof = readIDXV1(idxFileBuf, &index, oidSize, packFileSize);
    }

    gfPackInfo *cache[256] = {0};
    readCommitsFromOffsets(packPath, objof, cache, table, tableLocks, oidSize);

    u64 arraySize = pdArrSize(objof);
    for(u64 i = 0; i < arraySize; ++i)
    {
        pdSVFree(&objof[i].hash);
    }
    pdArrFree(objof);

    free(idxFileBuf);
    fclose(packFile);
}

void gfInitRepository
(
    u32          repoCount,
    gfRepository *repo,
    gfCommitInfo *head,
    gfCommitInfo **commitTable
){
    char packBuf[GF_MAX_PATH];
    char confBuf[GF_MAX_PATH];
    char absoluteBuf[GF_MAX_PATH];

    StringView absolute = pdExpandPath(repo->path, absoluteBuf);
    StringView gitPACK  = pdCstrSV("/.git/objects/pack/");
    StringView gitHEAD  = pdCstrSV("/.git/HEAD");
    StringView gitCONF  = pdCstrSV("/.git/config");

    gitPACK = pdSVConcat(absolute, gitPACK, packBuf);
    gitHEAD = pdSVConcat(absolute, gitHEAD, absoluteBuf);
    gitCONF = pdSVConcat(absolute, gitCONF, confBuf);

    StringView gitOBJ = gitPACK;
    gitOBJ.size -= 5;

    GF_TRACE("resolved head of '"PRI_SV"' to '"PRI_SV"'",
             ARG_SV(repo->path), ARG_SV(gitHEAD));

    if(pdVerifyPath(gitOBJ) != PD_TYPE_DIRECTORY)
    {
        GF_TRACE("repository has no object folder in "PRI_SV".", ARG_SV(gitOBJ));
        return;
    }

    char       listBuf[512 + GF_MAX_PATH] = {0};
    StringView list          = pdListFiles(gitPACK, listBuf);
    u32        fileCount     = pdSVCountByDelim(list, ';');

    if(!fileCount)
    {
        GF_TRACE("found no files in .git/objects/pack folder.");
        return;
    }
    GF_TRACE("found %"PRIu32" files in .git/objects/pack folder.", fileCount);

    StringView fileBuf[fileCount];
    for(u32 i = 0; i < fileCount; ++i)
    {
        fileBuf[i].data = 0;
        fileBuf[i].size = 0;
    }

    u08 oidSize = 20;

    StringView configFile = pdSVReadFile(confBuf);
    if(!configFile.data)
    {
        GF_WARN("could not read git config from '%s' to determine object ID size. "
                "Assuming 20 bytes.", confBuf);
    }
    else
    {
        u64  index         = 0;
        char lineBuf[GF_MAX_PATH] = {0};

        String line = pdCstrStr(lineBuf);
        while(pdSVReadLine(configFile, &index, &line, GF_MAX_PATH))
        {
            const char *objFormLoc = pdSVFind(objFormatIdent, *((StringView*)&line));
            if(objFormLoc)
            {
                StringView lineSV = pdCstrSV(objFormLoc + objFormatIdent.size);
                if(pdSVFind(sha256Ident, lineSV))
                {
                    oidSize = 32;
                }
            }
        }
    }

    gfThread threads[fileCount];
    gfPackedThreadData threadData[fileCount];
    gfThreadLock tableLocks[256] = {0};
    char tmpBufs[fileCount * GF_MAX_PATH];

    #ifdef BUILD_WINDOWS
        for(u16 i = 0; i < 256; ++i)
        {
            InitializeSRWLock(&tableLocks[i]);
        }
    #endif

    bool dispatched[fileCount];

    pdSVSeparateByDelim(list, fileBuf, ';', fileCount);
    for(u32 i = 0; i < fileCount; ++i)
    {
        if(pdSVFind(idxIdent, fileBuf[i]))
        {
            StringView idxPath = pdSVConcat(gitPACK, fileBuf[i],
                                            &tmpBufs[i * GF_MAX_PATH]);
            threadData[i].id         = i + repoCount;
            threadData[i].idxPath    = idxPath;
            threadData[i].table      = commitTable;
            threadData[i].tableLocks = tableLocks;
            threadData[i].oidSize    = oidSize;

            GF_TRACE("dispatching thread #%"PRIu32".", i);
            gfDispatchThread(&threads[i], (void*)threadReadPackfile, &threadData[i]);
            dispatched[i] = true;
        }
        else
        {
            dispatched[i] = false;
        }
    }

    for(u32 i = 0; i < fileCount; ++i)
    {
        if(!dispatched[i])
        {
            continue;
        }

        GF_TRACE("waiting on thread #%u", i);
        gfWaitThread(threads[i]);
    }

    StringView readHead = pdSVReadFile(absoluteBuf);
    if(!readHead.data)
    {
        GF_WARN("couldn't read HEAD file.");
        return;
    }

    const char *refLoc = pdSVFind(refIdent, readHead);
    if(refLoc && !(refLoc == readHead.data))
    {
        GF_TRACE("identified HEAD: '"PRI_SV"'", ARG_SV(readHead));
        gfGetCommitInfo(absolute, readHead, head, 0);
        pdSVFree(&readHead);
        return;
    }

    readHead.data += 5;
    readHead.size -= 6;
    GF_TRACE("identified HEAD ref: '"PRI_SV"'", ARG_SV(readHead));

    StringView ref = pdCstrSV("/.git/");
    char refBuf[GF_MAX_PATH];
    char headBuf[GF_MAX_PATH];

    ref      = pdSVConcat(ref, readHead, refBuf);
    readHead = pdSVConcat(repo->path, ref, headBuf);

    GF_TRACE("opening ref under '"PRI_SV"'...", ARG_SV(readHead));

    // URGENT: make this better, but be careful with killing the logic.
    char hashBuf[64] = {0};
    StringView hash  = {0};

    FILE *file = fopen(headBuf, "r");
    if(file)
    {
        if(fread(hashBuf, 1, oidSize * 2, file) != oidSize * 2)
        {
            GF_ERROR("could not read HEAD commit from '"PRI_SV"'.",
                     ARG_SV(readHead));
        }
    }
    else
    {
        char lineBuf[GF_MAX_PATH];
        char packedHeadBuf[GF_MAX_PATH];

        StringView packedHead = pdSVConcat(repo->path, packedHeadIdent, packedHeadBuf);
        file = fopen(packedHeadBuf, "r");
        if(!file)
        {
            GF_ERROR("found ref neither under '"PRI_SV"' nor in '"PRI_SV"'.",
                     ARG_SV(readHead), ARG_SV(packedHead));
            return;
        }

        while(fgets(lineBuf, 4096, file))
        {
            StringView path = pdCstrSV(lineBuf + oidSize * 2 + 1);

            if(pdSVSame(path, headPackedIdent))
            {
                for(u08 i = 0; i < oidSize * 2; ++i)
                {
                    hashBuf[i] = lineBuf[i];
                }
                break;
            }
        }
    }

    hash.data = hashBuf;
    hash.size = oidSize * 2;
    GF_TRACE("identified HEAD: '"PRI_SV"'", ARG_SV(hash));
    gfGetCommitInfo(repo->path, hash, head, commitTable);

    // pdSVFree(&headHash);
    pdSVFree(&readHead);
}
