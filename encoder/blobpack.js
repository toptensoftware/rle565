// Packs an array of buffers sequentially with a header and 
// offset table so blobs can be extracted by index
export function blobPack(blobs)
{
    const headerSize = 16;
    const tableSize = (blobs.length + 1) * 4;
    let header = Buffer.alloc(headerSize + tableSize);

    // Header
    header.writeUInt32LE(1, 0);             // version
    header.writeUInt32LE(blobs.length, 4);  // count
    header.writeUInt32LE(0, 8);             // reserved
    header.writeUInt32LE(0, 12);            // reserved

    // Offset table
    let pos = header.length;
    for (let i=0; i<blobs.length; i++)
    {
        header.writeUInt32LE(pos, headerSize + i * 4);
        pos += blobs[i].length;
    }

    // Total length in last slot so length of last blob can be calculated at runtime
    header.writeUInt32LE(pos, headerSize + blobs.length * 4);

    // Combine everything
    return Buffer.concat([header, ...blobs]);
}