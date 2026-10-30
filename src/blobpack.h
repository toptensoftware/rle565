#pragma once

#include <stdint.h>
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

#pragma pack(push, 1)
typedef struct tagBLOBPACK
{
    uint32_t version;
    uint32_t count;
    uint32_t reserved1;
    uint32_t reserved2;
    uint32_t blobOffsets[];
} BLOBPACK;
#pragma pack(pop)

// Get a blob pointer
static inline void* get_blob(const BLOBPACK* bp, uint32_t index)
{
    assert(index < bp->count);
    return ((char*)bp) + bp->blobOffsets[index];
}

// Get size of a blob
static inline uint32_t get_blob_size(const BLOBPACK* bp, uint32_t index)
{
    assert(index < bp->count);
    return bp->blobOffsets[index + 1] - bp->blobOffsets[index];
}

#ifdef __cplusplus
}
#endif
