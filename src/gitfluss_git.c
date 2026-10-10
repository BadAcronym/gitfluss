#include "gitfluss.h"
#include "gf_print_macros.h"

#include "datasurf_main.h"

#include "pd_path.h"
#include "pd_dyn_arr.h"

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

void gfFreeCommit
(
    gfCommitInfo *commit
){
    commit->authorTime    = 0;
    commit->committerTime = 0;

    pdSVFree(&commit->hash);
    pdSVFree(&commit->summary);
    pdSVFree(&commit->parentHash);
    pdSVFree(&commit->authorName);
    pdSVFree(&commit->authorMail);
    pdSVFree(&commit->committerName);
    pdSVFree(&commit->committerMail);
}

f_internal char valueToHexChar
(
    uint8_t value
){
    GF_ASSERT(value < 0x10, "cannot express values bigger than 15 in 4 bits.");

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

    GF_ASSERT((c1 > 0x2F && c1 < 0x3A) || (c1 > 0x60 && c1 < 0x67),
              "c1 outside of valid range. passed char: '%c' (0x%X)", c1, c1);
    GF_ASSERT((c2 > 0x2F && c2 < 0x3A) || (c2 > 0x60 && c2 < 0x67),
              "c2 outside of valid range. passed char: '%c' (0x%X)", c2, c2);

    v1 = (uint8_t)c1 - '0';
    if(c1 > 0x60)
    {
        v1 = (uint8_t)c1 - 0x57;
    }

    v2 = (uint8_t)c2 - '0';
    if(c2 > 0x60)
    {
        v2 = (uint8_t)c2 - 0x57;
    }

    return (uint8_t)((v1 << 4) | v2);
}

f_internal int64_t readTimeFromStr
(
    String str
){
    int64_t time = 0;

    for(uint8_t i = 0; i < str.size; ++i)
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

#ifdef DEBUG
f_internal bool verifyCommit
(
    gfCommitInfo *commit
){
    char c = 0;

    for(uint8_t i = 0; i < commit->hash.size; ++i)
    {
        c = commit->hash.data[i];
        if(!(c > 0x2F && c < 0x3A) && !(c > 0x60 && c < 0x67))
        {
            GF_ERROR("commit->hash.data[%"PRIu8"] is outside of valid range: '%c' "
                     "(0x%X)", i, c, c);
            return false;
        }
    }

    for(uint8_t i = 0; i < commit->parentHash.size; ++i)
    {
        c = commit->parentHash.data[i];
        if(!(c > 0x2F && c < 0x3A) && !(c > 0x60 && c < 0x67))
        {
            GF_ERROR("commit->parentHash.data[%"PRIu8"] is outside of valid range: '%c'"
                     " (0x%X)", i, c, c);
            return false;
        }
    }

    return true;
}
#endif

f_internal bool readLine
(
    uint8_t  *buffer,
    uint64_t bufsize,
    uint64_t *index,
    String   *line
){
    line->size = 0;

    if(*index >= bufsize)
    {
        return 0;
    }

    for(uint64_t i = 0; *index < bufsize; ++i)
    {
        line->data[i] = (char)buffer[*index];
        ++line->size;
        ++(*index);

        if(line->data[i] == '\0' || line->data[i] == '\n')
        {
            line->size -= 1;
            break;
        }
    }

    return line->size;
}

f_internal bool readCommitFromPtr
(
    uint8_t      *commitBuf,
    gfCommitInfo *commit,
    StringView   hash,
    uint64_t     bufsize
){
    GF_ASSERT(commitBuf, "passed nullptr buffer to readCommitFromPtr.");
    GF_ASSERT(commit,    "passed nullptr commit to readCommitFromPtr.");
    GF_ASSERT(hash.data, "passed nullptr hash data to readCommitFromPtr.");

    commit->hash = hash;

    char lineBuf[1024] = {0};
    String line = {0};
    line.data = lineBuf;

    uint64_t index = 0;
    while(readLine(commitBuf, bufsize, &index, &line))
    {
        if(pdSVFind(parentIdent, *((StringView*)&line)) == line.data)
        {
            line.data += parentIdent.size + 1;
            line.size -= parentIdent.size + 1;

            commit->parentHash = pdSVCpy(*((StringView*)&line));
        }
        else if(pdSVFind(authorIdent, *((StringView*)&line)) == line.data)
        {
            const char *karatLoc = pdSVFind(spaceKarat, *((StringView*)&line));

            StringView authorName = {0};
            authorName.data = line.data + authorIdent.size + 1;
            authorName.size = (uint64_t)(karatLoc - authorName.data);

            line.data = line.data + authorName.size;
            line.size -= authorName.size + 2;

            const char *karatEndLoc = pdSVFind(karatSpace, *((StringView*)&line));

            StringView authorMail = {0};
            authorMail.data = (char*)(authorName.data + authorName.size + 2);
            authorMail.size = (uint64_t)(karatEndLoc - authorMail.data);

            line.data = (char*)authorMail.data + authorMail.size + 2;
            line.size -= authorMail.size + 2;

            commit->authorName = pdSVCpy(authorName);
            commit->authorMail = pdSVCpy(authorMail);
            commit->authorTime = readTimeFromStr(line);
        }
        else if(pdSVFind(committerIdent, *((StringView*)&line)) == line.data)
        {
            const char *karatLoc = pdSVFind(spaceKarat, *((StringView*)&line));

            StringView committerName = {0};
            committerName.data = line.data + committerIdent.size + 1;
            committerName.size = (uint64_t)(karatLoc - committerName.data);

            line.data = line.data + committerName.size;
            line.size -= committerName.size + 2;

            const char *karatEndLoc = pdSVFind(karatSpace, *((StringView*)&line));

            StringView committerMail = {0};
            committerMail.data = (char*)(committerName.data + committerName.size + 2);
            committerMail.size = (uint64_t)(karatEndLoc - committerMail.data);

            line.data = (char*)committerMail.data + committerMail.size + 2;
            line.size -= committerMail.size + 2;

            commit->committerName = pdSVCpy(committerName);
            commit->committerMail = pdSVCpy(committerMail);
            commit->committerTime = readTimeFromStr(line);
        }
    }

    if(readLine(commitBuf, bufsize, &index, &line))
    {
        commit->summary = pdSVCpy(*((StringView*)&line));
    }

    GF_ASSERT(verifyCommit(commit), "returned bogus commit from readCommitFromFile.");

    GF_TRACE("read commit:");
    GF_TRACE("commit->hash:          '"PRI_SV"'", ARG_SV(commit->hash));
    GF_TRACE("commit->summary:       '"PRI_SV"'", ARG_SV(commit->summary));
    GF_TRACE("commit->parentHash:    '"PRI_SV"'", ARG_SV(commit->parentHash));
    GF_TRACE("commit->authorName:    '"PRI_SV"'", ARG_SV(commit->authorName));
    GF_TRACE("commit->authorMail:    '"PRI_SV"'", ARG_SV(commit->authorMail));
    GF_TRACE("commit->committerName: '"PRI_SV"'", ARG_SV(commit->committerName));
    GF_TRACE("commit->committerMail: '"PRI_SV"'", ARG_SV(commit->committerMail));
    GF_TRACE("commit->authorTime:    %"PRIu64,    commit->authorTime);
    GF_TRACE("commit->committerTime: %"PRIu64,    commit->committerTime);

    return true;
}

f_internal bool readObjectHeader
(
    uint8_t    *packFile,
    uint64_t   *index,
    uint64_t   packFileSize,
    gfPackInfo *outInfo
){
    uint8_t  byte  = packFile[(*index)++];
    uint64_t chunk = 0;
    uint8_t  shift = 4;
    outInfo->size  = byte & 0x0F;
    outInfo->type  = byte >> 4 & 0x07;

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
        GF_ASSERT(chunk < (UINT64_MAX >> shift), "chunk is too large.");

        outInfo->size |= chunk << shift;
        shift         += 7;
    }
    GF_ASSERT(outInfo->type > 0 && outInfo->type < 8, "invalid object type on obj: %"PRIu32
              ". read Byte: 0x%X", outInfo->type, byte);

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

    uint8_t zlibBuf[1024] = {0};

    uint64_t elements = 1;
    for(uint64_t i = 0; i < 1024 && elements == 1; ++i)
    {
        elements = fread(&zlibBuf[i], 1, 1, file);
    }

    GF_TRACE("reading commit from loose object.");

    uint8_t     *dstBuf = calloc(GF_BUFSIZE * 4, 1);
    DeflateInfo dfInfo  = dsReadZlibPtr(zlibBuf, dstBuf, GF_BUFSIZE * 4);
    if(!dfInfo.success)
    {
        goto closefile;
    }

    readCommitFromPtr(dstBuf, commit, pdSVCpy(hash), GF_BUFSIZE * 4);
    free(dstBuf);

closefile:
    fclose(file);
    GF_ASSERT(verifyCommit(commit), "returned bogus commit from readCommitFromFile.");
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

    StringView hashStart = hash;
    hashStart.size = 2;

    StringView hashRest = hash;
    pdSVTrim(&hashRest, 2, SV_LEFT);

    char pathBuf[4096] = {0};
    StringView commitPath = pdSVConcat(repository, gitObjectFolder, pathBuf);
    commitPath = pdSVConcat(commitPath, hashStart, pathBuf);
    commitPath = pdSVConcat(commitPath, sep, pathBuf);
    commitPath = pdSVConcat(commitPath, hashRest, pathBuf);

    uint8_t result = pdVerifyPath(commitPath);
    if(result == PD_TYPE_FILE)
    {
        gfFreeCommit(commit);
        readCommitFromFile(commitPath, hash, commit);
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

    uint8_t firstTwo = twoCharsToByte(hash.data[0], hash.data[1]);
    if(!table[firstTwo])
    {
        GF_TRACE("table[0x%x] has no data.", firstTwo);
        goto notfound;
    }

    uint64_t arraySize = pdArrSize(table[firstTwo]);
    GF_TRACE("looking for commit in table[%c%c]: size %"PRIu64,
             hash.data[0], hash.data[1], arraySize);
    for(uint64_t i = 0; i < arraySize; ++i)
    {
        GF_TRACE("checking against commit in table[0x%x]: "PRI_SV,
                 firstTwo, ARG_SV(table[firstTwo][i].hash));

        if(pdSVSame(hash, table[firstTwo][i].hash))
        {
            *commit = table[firstTwo][i];
            GF_TRACE("found commit!");
            found = true;
            break;
        }
    }

    if(!found)
    {
        goto notfound;
    }

    pdSVFree(&hash);
    return true;

notfound:
    GF_ERROR("could not find commit with hash '"PRI_SV"' anywhere.", ARG_SV(hash));
    pdSVFree(&hash);
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
                GF_ERROR("could not read fanout entry %"PRIu16".", j);
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
    uint8_t  oidSize,
    uint64_t packFileSize
){
    uint32_t       fanout[256] = {0};
    gfObjectOffset *objof      = 0;

    if(!readIDXFanout(file, fanout))
    {
        GF_ERROR("could not read fanouts of v1 IDX file.");
        goto closefile;
    }

    GF_WARN("TODO: .idx V1 files unhandled.");
    goto closefile;

    for(uint32_t i = 0; i < fanout[255]; ++i)
    {
        uint8_t  byte   = 0;
        uint32_t offset = 0;
        for(uint8_t j = 0; j < 4; ++j)
        {
            if(fread(&byte, 1, 1, file) != 1)
            {
                GF_ERROR("could not read offset of object %"PRIu32".", i);
                goto closefile;
            }

            offset |= (uint32_t)(byte << (8 * (3 - j)));
        }

        if(offset >= packFileSize)
        {
            GF_ERROR("pack offset is out of bounds.");
            goto closefile;
        }
    }

closefile:
    fclose(file);
    return objof;
}

// PERF: probably do the same thing as I tried to do with the packfile. read the entire
// thing into memory first, then iterate byte-by-byte instead of using fread byte-by-byte.
f_internal gfObjectOffset *readIDXV2
(
    FILE     *file,
    uint8_t  oidSize,
    uint64_t packFileSize
){
    gfObjectOffset *objof = 0;

    uint8_t  byte    = 0;
    uint32_t version = 0;
    for(uint8_t i = 0; i < 4; ++i)
    {
        if(fread(&byte, 1, 1, file) != 1)
        {
            GF_ERROR("failed to read IDX v2 version number.");
            goto closefile;
        }

        version |= (uint32_t)(byte << (3 - i));
    }

    if(version != 2)
    {
        GF_ERROR("unknown IDX v2 version number: %"PRIu32, version);
        goto closefile;
    }

    uint32_t fanout[256] = {0};
    if(!readIDXFanout(file, fanout))
    {
        GF_ERROR("could not read fanouts of v2 IDX file.");
        goto closefile;
    }

    pdArrReserve(objof, fanout[255]);

    for(uint32_t i = 0; i < fanout[255]; ++i)
    {
        char nameBuf[64] = {0};

        for(uint8_t j = 0; j < oidSize * 2; j += 2)
        {
            if(fread(&byte, 1, 1, file) != 1)
            {
                GF_ERROR("could not read name of object %"PRIu32".", i);
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
        oo.hash = pdSVCpy(name);
        pdArrPush(objof, oo);
    }

    if(fseek(file, fanout[255] * 4, SEEK_CUR) != 0)
    {
        GF_ERROR("could not skip CRC table.");
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
                GF_ERROR("could not read offset of object %"PRIu32".", i);
                pdArrFree(objof);
                goto closefile;
            }

            offset |= (uint32_t)(byte << (8 * (3 - j)));
        }

        if(offset & 0x80000000)
        {
            offset &= 0x7FFFFFFF;
            // TODO: do something with these
            GF_WARN("TODO: handle object offset via index into large offset table: %"
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
    fclose(file);
    return objof;
}

f_internal uint64_t readDeltaSize
(
    uint8_t  *buf,
    uint64_t *cursor,
    uint64_t bufsize
){
    uint8_t  byte  = buf[(*cursor)++];
    uint64_t size  = byte & 0x7F;
    uint64_t chunk = 0;
    uint8_t  shift = 7;

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
        GF_ASSERT(chunk < (UINT64_MAX >> shift), "chunk is too large.");

        size  |= chunk << shift;
        shift += 7;
    }

    GF_TRACE("read delta size of %"PRIu64" from cursor %"PRIu64, size, *cursor);

    return size;
}

f_internal bool readAndApplyDelta
(
    uint8_t    *packFile,
    uint64_t   *index,
    uint64_t   deltaDataSize,
    gfPackInfo *outInfo
){
    GF_TRACE("reading delta...");

    if(!outInfo->data)
    {
        GF_ERROR("outInfo->data is nil. cannot apply delta patch.");
        return false;
    }

    // PERF: we should really make this a scratch buffer. Deflate a little bit, then
    // parse, then deflate if more is needed, etc. For that, I need to implement scratch
    // reading in datasurf.
    uint8_t *deltaDataBuf = malloc(deltaDataSize);
    if(!deltaDataBuf)
    {
        GF_ERROR("failed to allocate buffer for delta data, size %"PRIu64,
                 deltaDataSize);
        return false;
    }

    if(deltaDataSize > TEN_MILLION)
    {
        GF_TRACE("allocated large delta: deltaDataSize: %"PRIu64, deltaDataSize);
    }

    DeflateInfo info = dsReadZlibPtr(&packFile[*index], deltaDataBuf, deltaDataSize);
    if(!info.success)
    {
        GF_ERROR("failed to read Zlib compressed data from packFile at index %"PRIu64
                 ". Read %"PRIu64" compressed bytes and wrote %"PRIu64" bytes.",
                 *index, info.bytesRead, info.bytesWritten);
        goto error;
    }

    uint64_t cursor     = 0;
    uint64_t baseSize   = readDeltaSize(deltaDataBuf, &cursor, deltaDataSize);
    uint64_t resultSize = readDeltaSize(deltaDataBuf, &cursor, deltaDataSize);

    GF_TRACE("applying delta...");

    #ifdef TRACE
    printf("full bytes:\n");
    for(uint64_t i = 0; i < deltaDataSize; ++i)
    {
        printf("0x%"PRIx8" ", deltaDataBuf[i]);
    }
    printf("\n");
    #endif

    uint8_t *resultObjBuf = malloc(resultSize);
    if(!resultObjBuf)
    {
        GF_ERROR("failed to allocate buffer for result object, size %"PRIu64,
                 resultSize);
        goto error;
    }

    if(resultSize > TEN_MILLION)
    {
        GF_TRACE("allocated large result buffer: resultSize: %"PRIu64, resultSize);
    }

    if(baseSize != outInfo->size)
    {
        GF_ERROR("baseSize %"PRIu64" does not match base object size %"PRIu64
                 " read from base object.", baseSize, outInfo->size);
        free(resultObjBuf);
        goto error;
    }

    uint64_t resultIndex = 0;
    while(cursor < deltaDataSize)
    {
        uint8_t opcode = deltaDataBuf[cursor++];

        if(opcode & 0x80)
        {
            uint64_t copySize   = 0;
            uint64_t copyOffset = 0;

            for(uint8_t bit = 0; bit < 4; ++bit)
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

                    copyOffset |= (uint64_t)deltaDataBuf[cursor++] << (bit * 8);
                }
            }

            for(unsigned bit = 0; bit < 3; ++bit)
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

                    copySize |= (uint64_t)deltaDataBuf[cursor++] << (bit * 8);
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
            uint8_t size = opcode & 0x7F;

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

// PERF: calls to this function should be multi-threaded, this is an absolute
// bottleneck.
f_internal bool resolveObjRecurse
(
    gfRecurseInfo *info
){
    uint64_t   entryStart = *info->index;
    gfPackInfo **cache    = info->cache;
    uint8_t    *packFile  = info->packFile;
    uint64_t   *index     = info->index;
    uint8_t    oidSize    = info->oidSize;
    uint64_t   fileSize   = info->fileSize;
    gfPackInfo *outInfo   = info->outInfo;

    GF_ASSERT(cache, "no cache present, cannot resolve recursively.");

    outInfo->offset = entryStart;

    uint8_t    lastTwo = entryStart & 0xFF;
    gfPackInfo *arr    = cache[lastTwo];
    if(!arr)
    {
        GF_TRACE("no objects in cache[0x%"PRIx8"]. skipping...", lastTwo);
        goto nocache;
    }

    uint64_t arrSize = pdArrSize(arr);
    GF_TRACE("LOOKING FOR CACHED OBJECT FOR OFFSET: %"PRIu64, outInfo->offset);
    for(uint16_t i = 0; i < arrSize; ++i)
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
    if(*index == UINT64_MAX)
    {
        // TODO: read midx to find where this hash is, actually, then read & return from
        // there

        //
        GF_WARN("TODO: HASH lookup unhandled.");
        return false;
        //
    }

    if(!readObjectHeader(packFile, index, fileSize, outInfo))
    {
        GF_ERROR("failed to read object header from index %"PRIu64, *index);
        return false;
    }

    GF_ASSERT(outInfo->type != 0, "object of type 0 is invalid.");
    GF_ASSERT(outInfo->type != GF_OBJ_RESERVED, "object of type 5 is reserved.");
    GF_ASSERT(outInfo->type <= GF_OBJ_REF_DELTA, "object of type %"PRIu8" is greater "
              "than the maxiumum of 7.", outInfo->type);

    if(outInfo->type == GF_OBJ_COMMIT)
    {
        GF_TRACE("identified commit object.");

        uint8_t *buf = calloc(outInfo->size, 1);
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
    else if(outInfo->type == GF_OBJ_TREE ||
            outInfo->type == GF_OBJ_BLOB ||
            outInfo->type == GF_OBJ_TAG
    ){
        GF_TRACE("identified base object of type %"PRIu8, outInfo->type);

        if(info->firstCall)
        {
            GF_TRACE("skipping base object of type %"PRIu8, outInfo->type);
            return true;
        }

        uint8_t *buf = calloc(outInfo->size, 1);
        if(!buf)
        {
            GF_ERROR("failed to allocate new outInfo->data of size %"PRIu64,
                     outInfo->size);
            return false;
        }

        if(outInfo->size > TEN_MILLION)
        {
            GF_TRACE("allocated large object: outInfo->size: %"PRIu64, outInfo->size);
        }

        DeflateInfo dfInfo = dsReadZlibPtr(&packFile[*index], buf, outInfo->size);

        if(!dfInfo.success)
        {
            GF_ERROR("could not successfully read non-commit base object from offset %"
                     PRIu64, *index);
            free(buf);
            return false;
        }

        GF_ASSERT(dfInfo.bytesWritten == outInfo->size, "expected to decompress into %"
                  PRIu64" bytes, actual: %"PRIu64".",
                  outInfo->size, dfInfo.bytesWritten);

        outInfo->data   = buf;
        outInfo->offset = entryStart;
        GF_TRACE("CACHING OBJECT (OTHER) FOR OFFSET: %"PRIu64, outInfo->offset);

        pdArrPush(cache[lastTwo], *outInfo);
        return true;
    }
    else if(outInfo->type == GF_OBJ_OFS_DELTA)
    {
        GF_TRACE("identified ofs delta object.");

        uint8_t  byte   = packFile[(*index)++];
        uint64_t offset = byte & 0x7F;

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

        uint64_t indexRec  = (entryStart - offset);
        GF_TRACE("jumping from packFile entryStart %"PRIu64" back to index %"PRIu64
                 "...", entryStart, indexRec);

        uint64_t deltaSize = outInfo->size;
        info->firstCall = false;
        info->index     = &indexRec;

        if(!resolveObjRecurse(info))
        {
            GF_ERROR("could not read recursive object from OBJ_OFS_DELTA.");
            return false;
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

        uint8_t byte = 0;

        char nameBuf[64] = {0};

        for(uint8_t j = 0; j < oidSize * 2; j += 2)
        {
            byte = packFile[(*index)++];
            nameBuf[j]     = valueToHexChar(byte >> 4);
            nameBuf[j + 1] = valueToHexChar(byte & 0x0F);
        }

        StringView name = {0};
        name.data = nameBuf;
        name.size = oidSize * 2;

        GF_TRACE("read REF_DELTA object '"PRI_SV"'", ARG_SV(name));

        uint64_t noIndex   = UINT64_MAX;
        uint64_t deltaSize = outInfo->size;
        info->firstCall = false;
        info->index     = &noIndex;

        if(!resolveObjRecurse(info))
        {
            GF_ERROR("could not read recursive object from OBJ_OFS_DELTA.");
            return false;
        }

        if(!readAndApplyDelta(packFile, index, deltaSize, outInfo))
        {
            GF_ERROR("could not apply delta from OBJ_OFS_DELTA.");
            return false;
        }

        outInfo->offset = entryStart;
        GF_TRACE("CACHING OBJECT (REF_DELTA) FOR OFFSET: UINT64_MAX");
        pdArrPush(cache[UINT64_MAX & 0xFF], *outInfo);
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
    uint8_t        oidSize
){
    FILE *file = fopen(path.data, "rb");
    if(!file)
    {
        GF_WARN("couldn't open pack file: '"PRI_SV"'", ARG_SV(path));
        return;
    }
    uint8_t *packFile = 0;

    fseek(file, 0, SEEK_END);
    uint64_t fileSize = (uint64_t)ftell(file);
    packFile = malloc(fileSize);

    fseek(file, 0, SEEK_SET);

    uint64_t elements = fread(packFile, 1, fileSize, file);
    if(elements != fileSize)
    {
        GF_WARN("couldn't read pack file into memory. tried to read %"PRIu64", but "
                "read %"PRIu64" instead.: '"PRI_SV"'",
                fileSize, elements, ARG_SV(path));
        goto closefile;
    }

    gfRecurseInfo info = {0};
    info.packFile = packFile;
    info.fileSize = fileSize;
    info.cache    = cache;
    info.oidSize  = oidSize;

    uint64_t arraySize = pdArrSize(objof);
    GF_TRACE("reading %"PRIu64" objects from offsets into packfile.", arraySize);
    for(uint32_t i = 0; i < arraySize; ++i)
    {
        gfPackInfo outInfo = {0};
        uint64_t   index   = objof[i].offset;

        info.hash      = objof[i].hash;
        info.index     = &index;
        info.outInfo   = &outInfo;
        info.firstCall = true;

        GF_TRACE("resolving '"PRI_SV"' at offset %"PRIu32" from "
                 "readCommitsFromOffsets...", ARG_SV(objof[i].hash), objof[i].offset);

        if(!resolveObjRecurse(&info)
        ){
            GF_DEBUG("could not resolve '"PRI_SV"' recursively, closing file.",
                     ARG_SV(objof[i].hash));
            goto closefile;
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

        StringView hash = pdSVCpy(objof[i].hash);
        gfFreeCommit(&commit);

        readCommitFromPtr(outInfo.data, &commit, hash, outInfo.size);

        uint8_t firstTwo = twoCharsToByte(hash.data[0], hash.data[1]);
        pdArrPush(table[firstTwo], commit);
    }

closefile:
    for(uint16_t i = 0; i < 256; ++i)
    {
        if(!cache[i])
        {
            continue;
        }

        uint64_t arrSize = pdArrSize(cache[i]);
        for(uint64_t j = 0; j < arrSize; ++j)
        {
            if(cache[i][j].data)
            {
                free(cache[i][j].data);
            }
        }

        pdArrFree(cache[i]);
    }

    if(packFile)
    {
        free(packFile);
    }
    fclose(file);
}

f_internal void readPackedCommits
(
    StringView   idxPath,
    gfPackInfo   **objCache,
    gfCommitInfo **table,
    uint8_t      oidSize
){
    FILE *file = fopen(idxPath.data, "rb");
    if(!file)
    {
        GF_ERROR("could not open .idx file: '"PRI_SV"'", ARG_SV(idxPath));
        return;
    }

    bool    v2   = true;
    uint8_t byte = 0;
    for(uint8_t i = 0; i < 4; ++i)
    {
        if(fread(&byte, 1, 1, file) != 1)
        {
            GF_ERROR("couldn't read first 4 bytes of .idx file: '"PRI_SV"'",
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
    StringView packPath = pdSVConcat(idxPath, pdCstrSV(".pack"), packBuf);
    idxPath.size += 4;

    uint64_t packFileSize = 0;

    FILE *packFile = fopen(packPath.data, "rb");
    if(!packFile)
    {
        GF_ERROR("could not open packfile from path '%s'.", packPath.data);
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
        GF_TRACE("reading .idx v2 file: '"PRI_SV"'", ARG_SV(idxPath));
        objof = readIDXV2(file, oidSize, packFileSize);
    }
    else
    {
        GF_TRACE("reading .idx v1 file: '"PRI_SV"'", ARG_SV(idxPath));
        fseek(file, 0, SEEK_SET);
        objof = readIDXV1(file, oidSize, packFileSize);
    }

    if(!objof)
    {
        GF_ERROR("could not read any object-offset pairs.");
        return;
    }

    readCommitsFromOffsets(packPath, objof, objCache, table, oidSize);

    uint64_t arraySize = pdArrSize(objof);
    for(uint64_t i = 0; i < arraySize; ++i)
    {
        pdSVFree(&objof[i].hash);
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
    char confBuf[4096]     = {0};
    char absoluteBuf[4096] = {0};
    StringView absolute = pdExpandPath(repo->path, absoluteBuf);
    StringView gitPACK  = pdCstrSV("/.git/objects/pack/");
    StringView gitHEAD  = pdCstrSV("/.git/HEAD");
    StringView gitCONF  = pdCstrSV("/.git/config");

    gitPACK = pdSVConcat(absolute, gitPACK, packBuf);
    gitHEAD = pdSVConcat(absolute, gitHEAD, absoluteBuf);
    gitCONF = pdSVConcat(absolute, gitCONF, confBuf);

    GF_TRACE("resolved head of '"PRI_SV"' to '"PRI_SV"'",
             ARG_SV(repo->path), ARG_SV(gitHEAD));

    if(pdVerifyPath(gitPACK) != PD_TYPE_DIRECTORY)
    {
        return;
    }

    char       listBuf[8192] = {0};
    StringView list          = pdListFiles(gitPACK, listBuf);
    uint64_t   fileCount     = pdSVCountByDelim(list, ';');

    StringView fileBuf[fileCount];
    for(uint64_t i = 0; i < fileCount; ++i)
    {
        fileBuf[i].data = 0;
        fileBuf[i].size = 0;
    }

    uint8_t oidSize = 20;

    FILE *configFile = fopen(confBuf, "r");
    if(!configFile)
    {
        GF_WARN("could not read git config from '%s' to determine object ID size. "
                "Assuming 20 bytes.", confBuf);
    }
    else
    {
        char line[8192] = {0};
        while(fgets(line, 8192, configFile))
        {
            StringView lineSV = {0};
            lineSV.data = line;
            lineSV.size = 8192;

            const char *objFormLoc = pdSVFind(objFormatIdent, lineSV);
            if(objFormLoc)
            {
                lineSV = pdCstrSV(objFormLoc + objFormatIdent.size);
                if(pdSVFind(sha256Ident, lineSV))
                {
                    oidSize = 32;
                }
            }
        }

        fclose(configFile);
    }

    pdSVSeparateByDelim(list, fileBuf, ';', fileCount);
    for(uint64_t i = 0; i < fileCount; ++i)
    {
        gfPackInfo *objCache[256] = {0};
        if(pdSVFind(idxIdent, fileBuf[i]))
        {
            char tmpBuf[4096] = {0};
            StringView idxPath = pdSVConcat(gitPACK, fileBuf[i], tmpBuf);
            GF_TRACE("reading packed commits from file: '"PRI_SV"'", ARG_SV(idxPath));
            readPackedCommits(idxPath, objCache, commitTable, oidSize);
        }
    }

    FILE *file = fopen(absoluteBuf, "rb");
    if(!file)
    {
        GF_WARN("couldn't open repository: '"PRI_SV"'", ARG_SV(repo->path));
        return;
    }

    char headBuf[4096] = {0};
    uint64_t elements = 1;
    for(uint64_t i = 0; elements == 1; ++i)
    {
        elements = fread(&headBuf[i], 1, 1, file);
    }

    StringView readHead = pdCstrSV(headBuf);
    const char *refLoc  = pdSVFind(refIdent, readHead);

    if(refLoc && !(refLoc == readHead.data))
    {
        GF_TRACE("identified HEAD: '"PRI_SV"'", ARG_SV(readHead));
        gfGetCommitInfo(absolute, readHead, head, 0);
        goto closefile;
    }

    readHead.data += 5;
    readHead.size -= 5;
    GF_TRACE("identified HEAD ref: '"PRI_SV"'", ARG_SV(readHead));

    StringView ref = pdCstrSV("/.git/");
    char refBuf[4096] = {0};

    ref      = pdSVConcat(ref, readHead, refBuf);
    readHead = pdSVConcat(repo->path, ref, headBuf);

    fclose(file);

    GF_TRACE("opening ref under '%s'...", headBuf);

    char hashBuf[64]   = {0};

    StringView hash = {0};

    file = fopen(headBuf, "r");
    if(file)
    {
        for(uint8_t i = 0; i < oidSize * 2; ++i)
        {
            if(fread(&hashBuf[i], 1, 1, file) != 1)
            {
                GF_ERROR("could not read HEAD commit from '"PRI_SV"'.",
                         ARG_SV(readHead));
            }
        }
    }
    else
    {
        char lineBuf[4096]       = {0};
        char packedHeadBuf[4096] = {0};

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
                for(uint8_t i = 0; i < oidSize * 2; ++i)
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
    gfGetCommitInfo(repo->path, pdSVCpy(hash), head, commitTable);

closefile:
    fclose(file);
}
