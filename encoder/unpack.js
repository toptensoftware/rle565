import sharp from "sharp";
import {
    IMAGE_SIGNATURE,
    IMAGE_FORMAT_RLE,
    DRAW_FLAGS_ALPHA_PIXELS,
    DRAW_FLAGS_LARGE_PALETTE,
} from "./pack.js";

// Decoder for .rle565 images.  A port of decode_image_argb8888() in
// src/rle565.c; every read is bounds checked and bad data throws.

const HEADER_SIZE = 40;

function invalid(msg)
{
    return new Error(`invalid rle565 image: ${msg}`);
}

// Read the fixed header fields
export function readRleHeader(buf)
{
    if (buf.length < HEADER_SIZE)
        throw invalid("too short");
    if (buf.readUInt32LE(0) !== IMAGE_SIGNATURE)
        throw invalid("bad signature");
    if (buf.readUInt32LE(4) !== IMAGE_FORMAT_RLE)
        throw invalid("unsupported format");

    return {
        width: buf.readUInt32LE(8),
        height: buf.readUInt32LE(12),
        flags: buf.readUInt32LE(16),
        packedSize: buf.readUInt32LE(20),
        captureSize: buf.readUInt32LE(24),
        paletteOffset: buf.readUInt32LE(28),
        firstRow: buf.readUInt32LE(32),
        rowCount: buf.readUInt32LE(36),
    };
}

// Read a palette (count + uint32 entries) as written by packPalette(), or
// embedded in an image at offset
export function readPalette(buf, offset = 0, limit = buf.length)
{
    if (offset + 4 > limit)
        throw invalid("palette out of range");
    const count = buf.readUInt32LE(offset);
    if (offset + 4 + count * 4 > limit)
        throw invalid("palette out of range");

    const colors = new Uint32Array(count);
    for (let i = 0; i < count; i++)
        colors[i] = buf.readUInt32LE(offset + 4 + i * 4);
    return colors;
}

// Convert a palette entry to straight alpha [r, g, b, a]
function paletteEntryToRgba(c)
{
    const a = c >>> 24;

    // Fully transparent
    if (a === 0)
        return [0, 0, 0, 0];

    // Opaque entries are 0xFF00 + RGB565, expanded with bit replication
    if (a === 0xFF)
    {
        const r = (c >> 11) & 0x1F;
        const g = (c >> 5) & 0x3F;
        const b = c & 0x1F;
        return [(r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2), 255];
    }

    // Partial alpha entries are premultiplied ARGB8888
    const unpremul = (v) => Math.min(255, Math.floor((v * 255 + (a >> 1)) / a));
    return [unpremul((c >> 16) & 0xFF), unpremul((c >> 8) & 0xFF), unpremul(c & 0xFF), a];
}

// Decode an .rle565 image to straight alpha RGBA.
//
//   buf      Buffer holding the image (may have trailing data)
//   options  { palette }  Uint32Array palette for images packed against a
//            shared palette (see readPalette)
//
// Returns { width, height, data } where data is a width * height * 4 RGBA Buffer
export function unpackRle(buf, options = {})
{
    const header = readRleHeader(buf);
    const { width, height, flags, packedSize, paletteOffset, firstRow, rowCount } = header;

    // Only read within the packed image
    if (packedSize < HEADER_SIZE || packedSize > buf.length)
        throw invalid("packed size out of range");
    const limit = packedSize;

    if (firstRow + rowCount > height)
        throw invalid("rows out of range");
    if (HEADER_SIZE + rowCount * 4 > limit)
        throw invalid("row table out of range");

    let palette = options.palette;
    if (!palette)
    {
        if (paletteOffset === 0)
            throw new Error("image uses a shared palette; supply one to decode it");
        palette = readPalette(buf, paletteOffset, limit);
    }

    // Convert palette once up front
    const rgba = Array.from(palette, paletteEntryToRgba);

    const alphaPixels = (flags & DRAW_FLAGS_ALPHA_PIXELS) !== 0;
    const largePalette = (flags & DRAW_FLAGS_LARGE_PALETTE) !== 0;

    // Output starts fully transparent, which also covers rows outside the
    // stored range and anything not covered by runs
    const out = Buffer.alloc(width * height * 4);

    for (let iy = 0; iy < rowCount; iy++)
    {
        let pos = buf.readUInt32LE(HEADER_SIZE + iy * 4);
        if (pos >= limit)
            throw invalid("row offset out of range");

        const readByte = () =>
        {
            if (pos >= limit)
                throw invalid("row data out of range");
            return buf[pos++];
        };

        const readVarLen = () =>
        {
            let val = 0;
            for (let i = 0; i < 5; i++)
            {
                const b = readByte();
                val = (val * 128) + (b & 0x7F);
                if ((b & 0x80) === 0)
                    return val;
            }
            throw invalid("bad varlen value");
        };

        const readIndex = () =>
        {
            const pi = largePalette ? readVarLen() : readByte();
            if (pi >= rgba.length)
                throw invalid("palette index out of range");
            return pi;
        };

        let x = 0;
        let o = (firstRow + iy) * width * 4;
        const runCount = readVarLen();
        for (let ri = 0; ri < runCount; ri++)
        {
            let pixCount = readVarLen();
            const isRle = (pixCount & 1) !== 0;
            pixCount = Math.floor(pixCount / 2);
            if (!isRle && alphaPixels)
                pixCount = Math.floor(pixCount / 2);    // drop the alpha-run bit

            if (pixCount > width - x)
                throw invalid("run too long");

            if (isRle)
            {
                const px = rgba[readIndex()];
                for (let i = 0; i < pixCount; i++, o += 4)
                    out.set(px, o);
            }
            else
            {
                for (let i = 0; i < pixCount; i++, o += 4)
                    out.set(rgba[readIndex()], o);
            }
            x += pixCount;
        }
    }

    return { width, height, data: out };
}

// Decode an .rle565 image and write it as a PNG file
export async function unpackRleToPng(buf, filename, options = {})
{
    const image = unpackRle(buf, options);
    await sharp(image.data, { raw: { width: image.width, height: image.height, channels: 4 } })
        .png()
        .toFile(filename);
    return image;
}
