#include "gitfluss.h"

#include "datasurf_main.h"

#include "pd_path.h"
#include "pd_dyn_arr.h"
#include "pd_print_macros.h"

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
    commit->authorTime   = 0;
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
            PD_ERROR("commit->hash.data[%"PRIu8"] is outside of valid range: '%c' "
                     "(0x%X)", i, c, c);
            return false;
        }
    }

    for(uint8_t i = 0; i < commit->parentHash.size; ++i)
    {
        c = commit->parentHash.data[i];
        if(!(c > 0x2F && c < 0x3A) && !(c > 0x60 && c < 0x67))
        {
            PD_ERROR("commit->parentHash.data[%"PRIu8"] is outside of valid range: '%c'"
                     " (0x%X)", i, c, c);
            return false;
        }
    }

    return true;
}
#endif

// FIXME: are we reading a newline or null char at start of line?
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
    PD_ASSERT(commitBuf, "passed nullptr buffer to readCommitFromPtr.");
    PD_ASSERT(commit,    "passed nullptr commit to readCommitFromPtr.");
    PD_ASSERT(hash.data, "passed nullptr hash data to readCommitFromPtr.");

    commit->hash = hash;

    // StringView summary      = {0};
    // StringView authorName   = {0};
    // StringView authorMail   = {0};
    // StringView commiterName = {0};
    // StringView commiterMail = {0};
    // StringView parent       = {0};
    // int64_t    authorTime   = 0;
    // int64_t    commiterTime = 0;

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
            PD_TRACE("FOUND LINE WITH AUTHOR: '"PRI_SV"'", ARG_SV(line));

            const char *karatLoc = pdSVFind(spaceKarat, *((StringView*)&line));

            StringView authorName = {0};
            authorName.data = line.data + authorIdent.size + 1;
            authorName.size = (uint64_t)(karatLoc - authorName.data);

            commit->authorName = pdSVCpy(authorName);

            PD_TRACE("FOUND AUTHORNAME: '"PRI_SV"'", ARG_SV(authorName));
        }
        else if(pdSVFind(committerIdent, *((StringView*)&line)) == line.data)
        {
            PD_TRACE("FOUND LINE WITH COMMITTER: '"PRI_SV"'", ARG_SV(line));
        }
    }

    if(readLine(commitBuf, bufsize, &index, &line))
    {
        PD_TRACE("FOUND LINE WITH SUMMARY: '"PRI_SV"'", ARG_SV(line));
        commit->summary = pdSVCpy(*((StringView*)&line));
    }

    // const char *authorLoc = pdSVFind(authorIdent, commitSV);
    // if(authorLoc)
    // {
    //     authorName.data = authorLoc + 7;
    //     authorName.size = bufsize - (uint64_t)(((uint8_t*)authorLoc - commitBuf) - 7);
    //
    //     // ASAN: heap-buffer-overflow, once again
    //     // summary = pdSVFindByDelim(authorName, '\n', 2);
    //
    //     const char *startKaratLoc = pdSVFind(spaceKarat, authorName);
    //     const char *endKaratLoc   = pdSVFind(karatSpace, authorName);
    //
    //     authorName.size = (uint64_t)(startKaratLoc - authorName.data);
    //
    //     authorMail.data = startKaratLoc + 2;
    //     authorMail.size = (uint64_t)(endKaratLoc - authorMail.data);
    //
    //     StringView authorTimeSV = {0};
    //     authorTimeSV.data = authorMail.data + authorMail.size + 2;
    //     for(uint32_t j = 0; j < 20; ++j)
    //     {
    //         if(authorTimeSV.data[j] == 0x20 || !authorTimeSV.data[j])
    //         {
    //             break;
    //         }
    //         ++authorTimeSV.size;
    //     }
    //
    //     authorTime = readTimeFromSV(authorTimeSV);
    //
    //     PD_TRACE("parsed authorName:   "PRI_SV, ARG_SV(authorName));
    //     PD_TRACE("parsed authorMail:   "PRI_SV, ARG_SV(authorMail));
    //     PD_TRACE("parsed authorTime:   "PRI_SV, ARG_SV(authorTimeSV));
    // }
    //
    // // ASAN: heap-buffer-overflow here
    // PD_DEBUG("trying to find commit inside String: '"PRI_SV"' of size %"PRIu64,
    //          ARG_SV(commitSV), commitSV.size);
    // const char *commiterLoc = pdSVFind(commiterIdent, commitSV);
    // if(commiterLoc)
    // {
    //     commiterName.data = commiterLoc + 9;
    //     commiterName.size = bufsize - (uint64_t)(((uint8_t*)commiterLoc - commitBuf)
    //                         - 9);
    //
    //     summary = pdSVFindByDelim(authorName, '\n', 2);
    //
    //     const char *startKaratLoc = pdSVFind(spaceKarat, commiterName);
    //     const char *endKaratLoc   = pdSVFind(karatSpace, commiterName);
    //
    //     commiterName.size = (uint64_t)(startKaratLoc - commiterName.data);
    //
    //     commiterMail.data = startKaratLoc + 2;
    //     commiterMail.size = (uint64_t)(endKaratLoc - commiterMail.data);
    //
    //     StringView commiterTimeSV = {0};
    //     commiterTimeSV.data = commiterMail.data + commiterMail.size + 2;
    //     for(uint32_t j = 0; j < 20; ++j)
    //     {
    //         if(commiterTimeSV.data[j] == 0x20 || !commiterTimeSV.data[j])
    //         {
    //             break;
    //         }
    //         ++commiterTimeSV.size;
    //     }
    //
    //     commiterTime = readTimeFromSV(commiterTimeSV);
    //
    //     PD_TRACE("parsed commiterName:   "PRI_SV, ARG_SV(commiterName));
    //     PD_TRACE("parsed commiterMail:   "PRI_SV, ARG_SV(commiterMail));
    //     PD_TRACE("parsed commiterTime:   "PRI_SV, ARG_SV(commiterTimeSV));
    // }
    //
    // PD_TRACE("parsed summary: '"PRI_SV"'", ARG_SV(summary));
    //
    // commit->authorTime   = authorTime;
    // commit->commiterTime = commiterTime;
    //
    // if(summary.size)
    // {
    //     commit->summary = pdSVCpy(summary);
    // }
    // if(authorName.size)
    // {
    //     commit->authorName = pdSVCpy(authorName);
    // }
    // if(authorMail.size)
    // {
    //     commit->authorMail = pdSVCpy(authorMail);
    // }
    // if(commiterName.size)
    // {
    //     commit->commiterName = pdSVCpy(commiterName);
    // }
    // if(commiterMail.size)
    // {
    //     commit->commiterMail = pdSVCpy(commiterMail);
    // }
    // if(parent.size)
    // {
    //     PD_TRACE("parsed parent: "PRI_SV, ARG_SV(parent));
    //     commit->parentHash = pdSVCpy(parent);
    // }
    //
    // PD_ASSERT(verifyCommit(commit), "returned bogus commit from readCommitFromFile.");
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
    bool     read  = byte & 0x80;
    outInfo->size  = byte & 0x0F;
    outInfo->type  = byte >> 4 & 0x07;

    for(uint8_t j = 0; read && j < 10; ++j)
    {
        if(*index >= packFileSize)
        {
            PD_ERROR("readObjectHeader index out of bounds.");
            return false;
        }

        byte  = packFile[(*index)++];
        chunk = byte & 0x7F;

        PD_ASSERT(shift < 64, "cannot shift more than 64 bits.");
        PD_ASSERT(chunk < (UINT64_MAX >> shift), "chunk is too large.");

        read = byte & 0x80;

        outInfo->size |= chunk << shift;
        shift         += 7;
    }
    PD_TRACE("parsed length from pack object (type %"PRIu8"): %"PRIu64"",
             outInfo->type, outInfo->size);

    PD_ASSERT(outInfo->type > 0 && outInfo->type < 8, "invalid object type on obj: %"PRIu32
              ". read Byte: 0x%X", outInfo->type, byte);

    return true;
}

f_internal void readCommitFromFile
(
    StringView   path,
    StringView   hash,
    gfCommitInfo *commit
){
    PD_TRACE("opening to read commit from path: '"PRI_SV"'", ARG_SV(path));

    char pathBuf[path.size + 1];
    pdSVCstr(path, pathBuf);

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
    PD_ASSERT(verifyCommit(commit), "returned bogus commit from readCommitFromFile.");
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

    PD_ASSERT(hash.size == 40 || hash.size == 64, "commit hash has invalid size: %"
              PRIu64". should be either 40 or 64 characters big. passed hash was: '"
              PRI_SV"'", hash.size, ARG_SV(hash));

    PD_ASSERT(hash.data, "cannot lookup commit with no hash.");

    PD_TRACE("looking for commit with hash: "PRI_SV, ARG_SV(hash));

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

        if(pdSVSame(hash, table[firstTwo][i].hash))
        {
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

    pdSVFree(&hash);
    return true;

notfound:
    PD_ERROR("could not find commit with hash '"PRI_SV"' anywhere.", ARG_SV(hash));
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
    uint8_t  oidSize,
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
        oo.hash = pdSVCpy(name);
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

// NOTE: if `index` is UINT64_MAX, instead resolve object by hash
// NOTE: (leave unimplemented, for now).
f_internal bool resolveObjRecurse
(
    uint8_t    *packFile,
    StringView hash,
    uint64_t   index,
    uint8_t    oidSize,
    uint64_t   packFileSize,
    gfPackInfo *outInfo
){
    uint64_t entryStart = index;

    if(!readObjectHeader(packFile, &index, packFileSize, outInfo))
    {
        return false;
    }

    PD_ASSERT(outInfo->type != 0, "object of type 0 is invalid.");
    PD_ASSERT(outInfo->type != GF_OBJ_RESERVED, "object of type 5 is reserved.");
    PD_ASSERT(outInfo->type <= GF_OBJ_REF_DELTA, "object of type %"PRIu8" is greater "
              "than the maxiumum of 7.", outInfo->type);

    if(outInfo->type == GF_OBJ_COMMIT)
    {
        PD_TRACE("reading commit from offset %"PRIu64" in packfile.", index);

        uint8_t     *buf   = calloc(outInfo->size, 1);
        DeflateInfo dfInfo = dsReadZlibPtr(&packFile[index], buf, outInfo->size);

        if(!dfInfo.success)
        {
            PD_ERROR("could not successfully read base object from offset %"PRIu64,
                     index);
            free(buf);
            return false;
        }

        PD_ASSERT(dfInfo.bytesWritten == outInfo->size, "expected to decompress into %"
                  PRIu64" bytes, actual: %"PRIu64".",
                  outInfo->size, dfInfo.bytesWritten);

        if(outInfo->data)
        {
            free(outInfo->data);
            outInfo->data = 0;
        }
        outInfo->data = buf;
        return true;
    }
    else if(outInfo->type == GF_OBJ_OFS_DELTA)
    {
        uint8_t  byte   = packFile[index++];
        uint64_t offset = byte & 0x7F;

        bool readMore = true;
        for(uint8_t j = 0; readMore && j < 11; ++j)
        {
            if(index >= packFileSize)
            {
                PD_ERROR("readPackEntry index out of bounds.");
                return false;
            }
            byte     = packFile[index++];
            readMore = byte & 0x80;
            offset   = ((offset + 1) << 7) | (byte & 0x7F);
        }

        if(offset >= entryStart)
        {
            PD_ERROR("negative offset %"PRIu64" is larger than current position of "
                     "file %"PRIu64".", offset, entryStart);
            return false;
        }

        PD_TRACE("read OFS_DELTA object with offset -%"PRIu64, offset);

        // recurse into entryStart - offset;

        // read delta (inflate)
        // apply delta patch

        PD_WARN("TODO: OBJ_OFS_DELTA unhandled.");
    }
    else if(outInfo->type == GF_OBJ_REF_DELTA)
    {
        uint8_t byte = 0;

        char nameBuf[64] = {0};

        for(uint8_t j = 0; j < oidSize * 2; j += 2)
        {
            byte = packFile[index++];
            nameBuf[j]     = valueToHexChar(byte >> 4);
            nameBuf[j + 1] = valueToHexChar(byte & 0x0F);
        }

        StringView name = {0};
        name.data = nameBuf;
        name.size = oidSize * 2;

        PD_TRACE("read REF_DELTA object '"PRI_SV"'", ARG_SV(name));

        // recursively read base object, looking up by object hash
        // read delta (inflate)
        // apply delta patch

        PD_WARN("TODO: OBJ_REF_DELTA unhandled.");
    }

    return true;
}

f_internal void readCommitsFromOffsets
(
    StringView     path,
    gfObjectOffset *objof,
    gfCommitInfo   **table,
    uint8_t        oidSize
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
        gfPackInfo info = {0};

        if(!resolveObjRecurse(packFile, objof[i].hash, objof[i].offset, oidSize,
                              packFileSize, &info)
        ){
            goto closefile;
        }

        if(info.type != GF_OBJ_COMMIT)
        {
            goto freeData;
        }

        gfCommitInfo commit = {0};

        PD_TRACE("hash inside readCommitsFromOffsets: '"PRI_SV"'",
                 ARG_SV(objof[i].hash));

        StringView hash = pdSVCpy(objof[i].hash);
        gfFreeCommit(&commit);
        readCommitFromPtr(info.data, &commit, hash, info.size);

        uint8_t firstTwo = twoCharsToByte(hash.data[0], hash.data[1]);
        pdArrPush(table[firstTwo], commit);

    freeData:
        if(info.data)
        {
            free(info.data);
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
    gfCommitInfo **table,
    uint8_t      oidSize
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
    StringView packPath = pdSVConcat(idxPath, pdCstrSV(".pack"), packBuf);
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
        objof = readIDXV2(file, oidSize, packFileSize);
    }
    else
    {
        PD_TRACE("reading .idx v1 file: '"PRI_SV"'", ARG_SV(idxPath));
        fseek(file, 0, SEEK_SET);
        objof = readIDXV1(file, oidSize, packFileSize);
    }

    if(!objof)
    {
        PD_ERROR("could not read any object-offset pairs.");
        return;
    }

    readCommitsFromOffsets(packPath, objof, table, oidSize);

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

    PD_TRACE("resolved head of '"PRI_SV"' to '"PRI_SV"'",
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
        PD_WARN("could not read git config from '%s' to determine object ID size. "
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
        if(pdSVFind(idxIdent, fileBuf[i]))
        {
            char tmpBuf[4096] = {0};
            StringView idxPath = pdSVConcat(gitPACK, fileBuf[i], tmpBuf);
            readPackedCommits(idxPath, commitTable, oidSize);
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

    StringView readHead = pdCstrSV(headBuf);
    const char *refLoc  = pdSVFind(refIdent, readHead);

    if(refLoc && !(refLoc == readHead.data))
    {
        PD_TRACE("identified HEAD: '"PRI_SV"'", ARG_SV(readHead));
        gfGetCommitInfo(absolute, readHead, head, 0);
        goto closefile;
    }

    readHead.data += 5;
    readHead.size -= 5;
    PD_TRACE("identified HEAD ref: '"PRI_SV"'", ARG_SV(readHead));

    StringView ref = pdCstrSV("/.git/");
    char refBuf[4096] = {0};

    ref      = pdSVConcat(ref, readHead, refBuf);
    readHead = pdSVConcat(repo->path, ref, headBuf);

    fclose(file);

    PD_TRACE("opening ref under '%s'...", headBuf);

    char hashBuf[64]   = {0};

    StringView hash = {0};

    file = fopen(headBuf, "r");
    if(file)
    {
        for(uint8_t i = 0; i < oidSize * 2; ++i)
        {
            if(fread(&hashBuf[i], 1, 1, file) != 1)
            {
                PD_ERROR("could not read HEAD commit from '"PRI_SV"'.",
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
            PD_ERROR("found ref neither under '"PRI_SV"' nor in '"PRI_SV"'.",
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

    PD_TRACE("identified HEAD: '"PRI_SV"'", ARG_SV(hash));
    gfGetCommitInfo(repo->path, pdSVCpy(hash), head, commitTable);

closefile:
    fclose(file);
}
