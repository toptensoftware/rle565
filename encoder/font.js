import fs from "node:fs";
import sharp from "sharp";

// Packs fonts rendered by msdf-atlas-gen into the rle565 font format: a
// single blob with metrics, glyph info, kerning, a codepoint hash table and
// 8-bit coverage bitmaps ready for draw_alpha_mask().  See FONT in
// src/rle565.h for the layout.
//
// Generate the atlas with a coverage type and pixel aligned glyphs, eg:
//
//   msdf-atlas-gen -font MyFont.ttf -size 20 -type softmask -pxalign on
//                  -yorigin top -imageout myfont.png -json myfont.json

export const FONT_SIGNATURE = 0x544E4652;      // "RFNT" little endian
export const FONT_VERSION = 1;
export const FONT_NO_GLYPH = 0xFFFF;

const FONT_HEADER_SIZE = 52;
const FONT_GLYPH_SIZE = 24;
const FONT_KERNING_SIZE = 4;
const MAX_HASH_BITS = 17;

// Hash table slot for a codepoint (must match font_find_glyph() in rle565.c)
export function fontHash(codepoint, hashBits)
{
    return Math.imul(codepoint, 0x9E3779B1) >>> (32 - hashBits);
}

// Load msdf-atlas-gen output: the JSON layout file and atlas image
export async function loadMsdfAtlas(jsonFile, imageFile)
{
    const layout = JSON.parse(fs.readFileSync(jsonFile, "utf8"));
    const { data, info } = await sharp(imageFile)
        .extractChannel(0)
        .raw()
        .toBuffer({ resolveWithObject: true });
    return { layout, atlas: { data, width: info.width, height: info.height } };
}

function clamp(value, min, max, what)
{
    if (value < min || value > max)
        throw new Error(`${what} ${value} out of range (${min}..${max})`);
    return value;
}

// Copy a glyph's rectangle out of the atlas and trim fully transparent
// rows and columns.  Returns the bitmap and its offset within the rectangle.
function cropGlyph(atlas, x0, y0, x1, y1)
{
    let minX = x1, minY = y1, maxX = x0 - 1, maxY = y0 - 1;
    for (let y = y0; y < y1; y++)
    {
        for (let x = x0; x < x1; x++)
        {
            if (atlas.data[y * atlas.width + x] != 0)
            {
                if (x < minX) minX = x;
                if (x > maxX) maxX = x;
                if (y < minY) minY = y;
                if (y > maxY) maxY = y;
            }
        }
    }

    if (maxX < minX)
        return { width: 0, height: 0, dx: 0, dy: 0, bitmap: Buffer.alloc(0) };

    const width = maxX - minX + 1;
    const height = maxY - minY + 1;
    const bitmap = Buffer.alloc(width * height);
    for (let y = 0; y < height; y++)
    {
        const src = (minY + y) * atlas.width + minX;
        atlas.data.copy(bitmap, y * width, src, src + width);
    }
    return { width, height, dx: minX - x0, dy: minY - y0, bitmap };
}

// Pack an msdf-atlas-gen layout + atlas into a font blob.
//
//   layout   parsed msdf-atlas-gen JSON
//   atlas    { data, width, height } - one byte per pixel coverage
//   options  defaultChar: character (or codepoint) drawn for missing
//            characters.  Default: U+FFFD or '?' if the font has one.
//
// Returns { data, stats }
export function packFont(layout, atlas, options = {})
{
    const info = layout.atlas;
    if (info.type != "softmask" && info.type != "hardmask")
        throw new Error(`atlas type '${info.type}' isn't supported, use -type softmask (or hardmask)`);
    if (info.width != atlas.width || info.height != atlas.height)
        throw new Error(`atlas image is ${atlas.width}x${atlas.height} but the layout says ${info.width}x${info.height}`);

    // Everything is converted to pixels with y down, relative to the pen
    // position on the baseline
    const metrics = layout.metrics;
    const scale = info.size / (metrics.emSize || 1);
    const flipY = info.yOrigin != "top";
    const down = (y) => (flipY ? -y : y) * scale;

    // Glyphs, sorted by codepoint
    const source = new Map();
    for (const g of layout.glyphs)
    {
        if (g.unicode != null && !source.has(g.unicode))
            source.set(g.unicode, g);
    }
    const codepoints = [...source.keys()].sort((a, b) => a - b);
    if (codepoints.length >= FONT_NO_GLYPH)
        throw new Error(`too many glyphs (${codepoints.length})`);
    const glyphIndex = new Map(codepoints.map((cp, i) => [cp, i]));

    // Rasterized glyphs, sharing identical bitmaps
    const bitmaps = [];
    const bitmapOffsets = new Map();
    let bitmapBytes = 0;
    let sharedBitmaps = 0;
    let unaligned = 0;
    const glyphs = codepoints.map(cp =>
    {
        const g = source.get(cp);
        const glyph = {
            codepoint: cp,
            bitmap: 0, width: 0, height: 0, left: 0, top: 0,
            advance: clamp(Math.round(g.advance * scale * 64), 0, 0xFFFF, `advance of U+${cp.toString(16)}`),
            kerning: 0, kerningCount: 0, pairs: [],
        };
        const ab = g.atlasBounds, pb = g.planeBounds;
        if (!ab || !pb)
            return glyph;

        // Atlas rectangle in image coordinates.  atlasBounds are on pixel
        // centers, so the pixels touched extend half a pixel further.
        const aLeft = ab.left;
        const aTop = flipY ? info.height - ab.top : ab.top;
        const aRight = ab.right;
        const aBottom = flipY ? info.height - ab.bottom : ab.bottom;
        const x0 = Math.max(0, Math.floor(aLeft));
        const y0 = Math.max(0, Math.floor(aTop));
        const x1 = Math.min(info.width, Math.ceil(aRight));
        const y1 = Math.min(info.height, Math.ceil(aBottom));

        // Position of pixel (x0, y0)'s top left corner relative to the pen.
        // Whole pixels with -pxalign on, otherwise rounded.
        const left = pb.left * scale + (x0 - aLeft);
        const top = down(pb.top) + (y0 - aTop);
        if (Math.abs(left - Math.round(left)) > 0.01 || Math.abs(top - Math.round(top)) > 0.01)
            unaligned++;

        const crop = cropGlyph(atlas, x0, y0, x1, y1);
        if (crop.width == 0)
            return glyph;

        const key = `${crop.width}x${crop.height}:${crop.bitmap.toString("base64")}`;
        let offset = bitmapOffsets.get(key);
        if (offset === undefined)
        {
            offset = bitmapBytes;
            bitmapOffsets.set(key, offset);
            bitmaps.push(crop.bitmap);
            bitmapBytes += crop.bitmap.length;
        }
        else
        {
            sharedBitmaps++;
        }

        glyph.bitmap = offset;
        glyph.width = crop.width;
        glyph.height = crop.height;
        glyph.left = clamp(Math.round(left) + crop.dx, -0x8000, 0x7FFF, `left of U+${cp.toString(16)}`);
        glyph.top = clamp(Math.round(top) + crop.dy, -0x8000, 0x7FFF, `top of U+${cp.toString(16)}`);
        return glyph;
    });

    // Kerning pairs, grouped by left glyph and sorted by right glyph
    let kerningCount = 0;
    for (const k of layout.kerning ?? [])
    {
        const l = glyphIndex.get(k.unicode1);
        const r = glyphIndex.get(k.unicode2);
        if (l === undefined || r === undefined)
            continue;
        const adjust = Math.round(k.advance * scale * 64);
        if (adjust == 0)
            continue;
        glyphs[l].pairs.push({ right: r, adjust: clamp(adjust, -0x8000, 0x7FFF, "kerning") });
    }
    for (const g of glyphs)
    {
        g.pairs.sort((a, b) => a.right - b.right);
        g.pairs = g.pairs.filter((p, i) => i == 0 || p.right != g.pairs[i - 1].right);
        g.kerning = kerningCount;
        g.kerningCount = g.pairs.length;
        kerningCount += g.pairs.length;
    }

    // Hash table: power of two size, at most half full
    let hashBits = 1;
    while ((1 << hashBits) < glyphs.length * 2)
        hashBits++;
    if (hashBits > MAX_HASH_BITS)
        throw new Error("too many glyphs for hash table");
    const slots = new Uint16Array(1 << hashBits).fill(FONT_NO_GLYPH);
    let maxProbe = 0;
    glyphs.forEach((g, i) =>
    {
        let s = fontHash(g.codepoint, hashBits);
        let probe = 1;
        while (slots[s] != FONT_NO_GLYPH)
        {
            s = (s + 1) & (slots.length - 1);
            probe++;
        }
        slots[s] = i;
        maxProbe = Math.max(maxProbe, probe);
    });

    // Default glyph for missing characters
    let defaultGlyph = FONT_NO_GLYPH;
    if (options.defaultChar != null)
    {
        const cp = typeof options.defaultChar == "string" ? options.defaultChar.codePointAt(0) : options.defaultChar;
        if (!glyphIndex.has(cp))
            throw new Error(`default character U+${cp.toString(16)} isn't in the font`);
        defaultGlyph = glyphIndex.get(cp);
    }
    else
    {
        defaultGlyph = glyphIndex.get(0xFFFD) ?? glyphIndex.get(0x3F) ?? FONT_NO_GLYPH;
    }

    // Font metrics
    const thickness = metrics.underlineThickness > 0 ? Math.max(1, Math.round(metrics.underlineThickness * scale)) : 0;
    const underlineY = Math.round(down(metrics.underlineY) - thickness / 2);

    // Layout
    const glyphsOffset = FONT_HEADER_SIZE;
    const kerningOffset = glyphsOffset + glyphs.length * FONT_GLYPH_SIZE;
    const hashOffset = kerningOffset + kerningCount * FONT_KERNING_SIZE;
    const bitmapsOffset = hashOffset + slots.length * 2;
    const packedSize = (bitmapsOffset + bitmapBytes + 3) & ~3;

    const buf = Buffer.alloc(packedSize);
    buf.writeUInt32LE(FONT_SIGNATURE, 0);
    buf.writeUInt16LE(FONT_VERSION, 4);
    buf.writeUInt16LE(0, 6);                                    // flags
    buf.writeUInt32LE(packedSize, 8);
    buf.writeUInt16LE(Math.round(info.size), 12);
    buf.writeUInt16LE(Math.round(metrics.lineHeight * scale), 14);
    buf.writeUInt16LE(Math.round(-down(metrics.ascender)), 16);
    buf.writeUInt16LE(Math.round(down(metrics.descender)), 18);
    buf.writeInt16LE(underlineY, 20);
    buf.writeUInt16LE(thickness, 22);
    buf.writeUInt16LE(glyphs.length, 24);
    buf.writeUInt16LE(defaultGlyph, 26);
    buf.writeUInt16LE(hashBits, 28);
    buf.writeUInt16LE(0, 30);                                   // reserved
    buf.writeUInt32LE(kerningCount, 32);
    buf.writeUInt32LE(glyphsOffset, 36);
    buf.writeUInt32LE(kerningOffset, 40);
    buf.writeUInt32LE(hashOffset, 44);
    buf.writeUInt32LE(bitmapsOffset, 48);

    glyphs.forEach((g, i) =>
    {
        const o = glyphsOffset + i * FONT_GLYPH_SIZE;
        buf.writeUInt32LE(g.codepoint, o);
        buf.writeUInt32LE(g.bitmap, o + 4);
        buf.writeUInt32LE(g.kerning, o + 8);
        buf.writeUInt16LE(g.width, o + 12);
        buf.writeUInt16LE(g.height, o + 14);
        buf.writeInt16LE(g.left, o + 16);
        buf.writeInt16LE(g.top, o + 18);
        buf.writeUInt16LE(g.advance, o + 20);
        buf.writeUInt16LE(g.kerningCount, o + 22);

        g.pairs.forEach((p, j) =>
        {
            const ko = kerningOffset + (g.kerning + j) * FONT_KERNING_SIZE;
            buf.writeUInt16LE(p.right, ko);
            buf.writeInt16LE(p.adjust, ko + 2);
        });
    });

    for (let i = 0; i < slots.length; i++)
        buf.writeUInt16LE(slots[i], hashOffset + i * 2);

    let pos = bitmapsOffset;
    for (const b of bitmaps)
    {
        b.copy(buf, pos);
        pos += b.length;
    }

    return {
        data: buf,
        stats: {
            size: Math.round(info.size),
            glyphs: glyphs.length,
            kerningPairs: kerningCount,
            hashSlots: slots.length,
            maxProbe,
            bitmapBytes,
            sharedBitmaps,
            unaligned,
            packedSize,
        },
    };
}

// Load msdf-atlas-gen output files and pack them
export async function packFontFiles(jsonFile, imageFile, options)
{
    const { layout, atlas } = await loadMsdfAtlas(jsonFile, imageFile);
    return packFont(layout, atlas, options);
}
