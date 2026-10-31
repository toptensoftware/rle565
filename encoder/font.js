import * as fontkit from "fontkit";

// Packs TrueType / OpenType fonts into the rle565 font format: a single
// blob with metrics, glyph info, kerning, a codepoint hash table and 8-bit
// coverage bitmaps ready for draw_alpha_mask().  See FONT in src/rle565.h
// for the layout.
//
// Glyphs are rasterized here with exact area coverage (no hinting).
// Kerning comes from the font's GPOS table or legacy kern table.

export const FONT_SIGNATURE = 0x544E4652;      // "RFNT" little endian
export const FONT_VERSION = 1;
export const FONT_NO_GLYPH = 0xFFFF;
export const FONT_SUBPIXEL_VALUES = [1, 2, 4, 8];

const FONT_HEADER_SIZE = 52;
const FONT_GLYPH_SIZE = 24;
const FONT_KERNING_SIZE = 4;
const MAX_HASH_BITS = 17;

// Hash table slot for a codepoint (must match font_find_glyph() in rle565.c)
export function fontHash(codepoint, hashBits)
{
    return Math.imul(codepoint, 0x9E3779B1) >>> (32 - hashBits);
}

// Parse a character set like "32-126,0xB0,U+20AC" into sorted codepoints.
// Items are single codepoints or ranges, in decimal, 0x hex or U+ hex.
export function parseCharset(spec)
{
    const parseOne = (s) =>
    {
        s = s.trim();
        let m;
        if ((m = /^(?:0x|u\+)([0-9a-f]+)$/i.exec(s)))
            return parseInt(m[1], 16);
        if (/^\d+$/.test(s))
            return parseInt(s, 10);
        throw new Error(`invalid character '${s}' in charset (use decimal, 0x hex or U+ hex)`);
    };

    const set = new Set();
    for (const item of spec.split(/[,\s]+/).filter(s => s.length))
    {
        const parts = item.split("-");
        if (parts.length > 2)
            throw new Error(`invalid range '${item}' in charset`);
        const from = parseOne(parts[0]);
        const to = parts.length == 2 ? parseOne(parts[1]) : from;
        if (from > to || to > 0x10FFFF)
            throw new Error(`invalid range '${item}' in charset`);
        for (let cp = from; cp <= to; cp++)
            set.add(cp);
    }
    return [...set].sort((a, b) => a - b);
}

function clamp(value, min, max, what)
{
    if (value < min || value > max)
        throw new Error(`${what} ${value} out of range (${min}..${max})`);
    return value;
}

// ---------------------------------------------------------------------------
// Rasterizer
//
// Signed area accumulation (as used by font-rs): each line segment adds the
// area it covers to an accumulation buffer, and a running sum along each
// row gives exact coverage.  abs() and clamping to 1 gives nonzero winding
// for overlapping contours.

// Flatten a glyph path (font units, y up) to line segments in pixels (y
// down) with the origin shifted right by dx pixels
function flattenPath(commands, scale, dx)
{
    const lines = [];
    const TOLERANCE = 0.02;     // max deviation from the curve in pixels
    let sx = 0, sy = 0, cx = 0, cy = 0;
    const X = (x) => x * scale + dx;
    const Y = (y) => -y * scale;
    const line = (x0, y0, x1, y1) => { if (y0 != y1) lines.push([x0, y0, x1, y1]); };

    for (const c of commands)
    {
        const a = c.args;
        switch (c.command)
        {
            case "moveTo":
                if (cx != sx || cy != sy)
                    line(cx, cy, sx, sy);
                sx = cx = X(a[0]);
                sy = cy = Y(a[1]);
                break;

            case "lineTo":
            {
                const x = X(a[0]), y = Y(a[1]);
                line(cx, cy, x, y);
                cx = x; cy = y;
                break;
            }

            case "quadraticCurveTo":
            {
                const x1 = X(a[0]), y1 = Y(a[1]), x2 = X(a[2]), y2 = Y(a[3]);
                const dev = Math.hypot(cx - 2 * x1 + x2, cy - 2 * y1 + y2) / 4;
                const n = Math.min(64, Math.max(1, Math.ceil(Math.sqrt(dev / TOLERANCE))));
                let px = cx, py = cy;
                for (let i = 1; i <= n; i++)
                {
                    const t = i / n, u = 1 - t;
                    const x = u * u * cx + 2 * u * t * x1 + t * t * x2;
                    const y = u * u * cy + 2 * u * t * y1 + t * t * y2;
                    line(px, py, x, y);
                    px = x; py = y;
                }
                cx = x2; cy = y2;
                break;
            }

            case "bezierCurveTo":
            {
                const x1 = X(a[0]), y1 = Y(a[1]), x2 = X(a[2]), y2 = Y(a[3]), x3 = X(a[4]), y3 = Y(a[5]);
                const dev = 0.75 * Math.max(
                    Math.hypot(cx - 2 * x1 + x2, cy - 2 * y1 + y2),
                    Math.hypot(x1 - 2 * x2 + x3, y1 - 2 * y2 + y3));
                const n = Math.min(64, Math.max(1, Math.ceil(Math.sqrt(dev / TOLERANCE))));
                let px = cx, py = cy;
                for (let i = 1; i <= n; i++)
                {
                    const t = i / n, u = 1 - t;
                    const x = u * u * u * cx + 3 * u * u * t * x1 + 3 * u * t * t * x2 + t * t * t * x3;
                    const y = u * u * u * cy + 3 * u * u * t * y1 + 3 * u * t * t * y2 + t * t * t * y3;
                    line(px, py, x, y);
                    px = x; py = y;
                }
                cx = x3; cy = y3;
                break;
            }

            case "closePath":
                line(cx, cy, sx, sy);
                cx = sx; cy = sy;
                break;
        }
    }
    if (cx != sx || cy != sy)
        line(cx, cy, sx, sy);
    return lines;
}

// Add one line's signed area to the accumulation buffer
function accumulateLine(acc, stride, height, x0, y0, x1, y1)
{
    let dir = 1;
    if (y0 > y1)
    {
        dir = -1;
        [x0, y0, x1, y1] = [x1, y1, x0, y0];
    }
    const dxdy = (x1 - x0) / (y1 - y0);
    let x = x0;
    let yStart = Math.floor(y0);
    if (yStart < 0)
    {
        x -= y0 * dxdy;
        yStart = 0;
    }
    const yEnd = Math.min(height, Math.ceil(y1));

    for (let y = yStart; y < yEnd; y++)
    {
        const row = y * stride;
        const dy = Math.min(y + 1, y1) - Math.max(y, y0);
        const xNext = x + dxdy * dy;
        const d = dy * dir;
        const xa = Math.min(x, xNext);
        const xb = Math.max(x, xNext);
        const xaFloor = Math.floor(xa);
        const xai = xaFloor;
        const xbCeil = Math.ceil(xb);
        const xbi = xbCeil;

        if (xbi <= xai + 1)
        {
            // Within one pixel column
            const xmf = 0.5 * (x + xNext) - xaFloor;
            acc[row + xai] += d - d * xmf;
            acc[row + xai + 1] += d * xmf;
        }
        else
        {
            // Spans columns: trapezoid areas at each end, constant between
            const s = 1 / (xb - xa);
            const xaf = xa - xaFloor;
            const a0 = 0.5 * s * (1 - xaf) * (1 - xaf);
            const xbf = xb - xbCeil + 1;
            const am = 0.5 * s * xbf * xbf;
            acc[row + xai] += d * a0;
            if (xbi == xai + 2)
            {
                acc[row + xai + 1] += d * (1 - a0 - am);
            }
            else
            {
                const a1 = s * (1.5 - xaf);
                acc[row + xai + 1] += d * (a1 - a0);
                for (let xi = xai + 2; xi < xbi - 1; xi++)
                    acc[row + xi] += d * s;
                const a2 = a1 + (xbi - xai - 3) * s;
                acc[row + xbi - 1] += d * (1 - a2 - am);
            }
            acc[row + xbi] += d * am;
        }
        x = xNext;
    }
}

// Rasterize a glyph path shifted right by dx pixels.  Returns the coverage
// bitmap and the pixel position of its top left corner relative to the
// pen on the baseline, or null if the glyph is empty.
export function rasterizeGlyph(commands, scale, dx)
{
    const lines = flattenPath(commands, scale, dx);
    if (lines.length == 0)
        return null;

    let minX = Infinity, minY = Infinity, maxX = -Infinity, maxY = -Infinity;
    for (const [x0, y0, x1, y1] of lines)
    {
        minX = Math.min(minX, x0, x1); maxX = Math.max(maxX, x0, x1);
        minY = Math.min(minY, y0, y1); maxY = Math.max(maxY, y0, y1);
    }
    const left = Math.floor(minX);
    const top = Math.floor(minY);
    const width = Math.ceil(maxX) - left + 1;
    const height = Math.ceil(maxY) - top;
    const stride = width + 2;
    const acc = new Float64Array(stride * height);
    for (const [x0, y0, x1, y1] of lines)
        accumulateLine(acc, stride, height, x0 - left, y0 - top, x1 - left, y1 - top);

    const bitmap = new Uint8Array(width * height);
    for (let y = 0; y < height; y++)
    {
        let sum = 0;
        for (let x = 0; x < width; x++)
        {
            sum += acc[y * stride + x];
            bitmap[y * width + x] = Math.round(Math.min(1, Math.abs(sum)) * 255);
        }
    }
    return { bitmap, width, height, left, top };
}

// ---------------------------------------------------------------------------
// Font packing

// Render a glyph at each subpixel offset and crop all the variants to the
// same rectangle (the union of their inked pixels).
function renderGlyphVariants(glyph, scale, subpixel)
{
    const renders = [];
    let minX = Infinity, minY = Infinity, maxX = -Infinity, maxY = -Infinity;
    for (let i = 0; i < subpixel; i++)
    {
        const r = rasterizeGlyph(glyph.path.commands, scale, i / subpixel);
        renders.push(r);
        if (!r)
            continue;
        for (let y = 0; y < r.height; y++)
        {
            for (let x = 0; x < r.width; x++)
            {
                if (r.bitmap[y * r.width + x])
                {
                    minX = Math.min(minX, r.left + x); maxX = Math.max(maxX, r.left + x);
                    minY = Math.min(minY, r.top + y); maxY = Math.max(maxY, r.top + y);
                }
            }
        }
    }
    if (maxX < minX)
        return null;

    const width = maxX - minX + 1;
    const height = maxY - minY + 1;
    const data = Buffer.alloc(width * height * subpixel);
    renders.forEach((r, i) =>
    {
        if (!r)
            return;
        for (let y = 0; y < height; y++)
        {
            const sy = minY + y - r.top;
            if (sy < 0 || sy >= r.height)
                continue;
            for (let x = 0; x < width; x++)
            {
                const sx = minX + x - r.left;
                if (sx >= 0 && sx < r.width)
                    data[i * width * height + y * width + x] = r.bitmap[sy * r.width + sx];
            }
        }
    });
    return { data, width, height, left: minX, top: minY };
}

// Kerning in font units between two glyphs, from the font's GPOS or kern
// table, with substitutions that could merge or change glyphs turned off
const LAYOUT_FEATURES = { liga: false, clig: false, dlig: false, rlig: false, calt: false, ccmp: false, kern: true };
function pairKerning(font, a, b)
{
    const run = font.layout(String.fromCodePoint(a.cp, b.cp), LAYOUT_FEATURES);
    if (run.glyphs.length != 2 || run.glyphs[0].id != a.glyph.id || run.glyphs[1].id != b.glyph.id)
        return 0;
    return run.positions[0].xAdvance - a.glyph.advanceWidth + run.positions[1].xOffset;
}

// Pack a font.
//
//   fontFile   path to a .ttf, .otf, .woff or .woff2 file
//   options    size:        pixels per em (required)
//              charset:     codepoints to include (array), or a charset
//                           string for parseCharset().  Default 32-126.
//              subpixel:    horizontal positions per pixel to render each
//                           glyph at: 1 (default), 2, 4 or 8
//              defaultChar: character (or codepoint) drawn for missing
//                           characters.  Default: U+FFFD or '?' if present.
//              kerning:     false to leave out kerning
//
// Returns { data, stats }
export function packFont(fontFile, options = {})
{
    const size = Number(options.size);
    if (!(size > 0 && size <= 1000))
        throw new Error("font size must be given, in pixels per em");
    const subpixel = options.subpixel ?? 1;
    if (!FONT_SUBPIXEL_VALUES.includes(subpixel))
        throw new Error(`subpixel must be one of ${FONT_SUBPIXEL_VALUES.join(", ")}`);
    let charset = options.charset ?? "32-126";
    if (typeof charset == "string")
        charset = parseCharset(charset);

    let font = fontkit.openSync(fontFile);
    if (font.fonts)
        throw new Error("font collections aren't supported");
    const scale = size / font.unitsPerEm;

    // Glyphs for the requested characters that the font has
    const missing = [];
    const chars = [];
    for (const cp of charset)
    {
        if (font.hasGlyphForCodePoint(cp))
            chars.push({ cp, glyph: font.glyphForCodePoint(cp) });
        else
            missing.push(cp);
    }
    if (chars.length >= FONT_NO_GLYPH)
        throw new Error(`too many glyphs (${chars.length})`);
    const glyphIndex = new Map(chars.map((c, i) => [c.cp, i]));

    // Rasterize, sharing identical bitmaps
    const bitmaps = [];
    const bitmapOffsets = new Map();
    let bitmapBytes = 0;
    let sharedBitmaps = 0;
    const glyphs = chars.map(c =>
    {
        const glyph = {
            codepoint: c.cp,
            bitmap: 0, width: 0, height: 0, left: 0, top: 0,
            advance: clamp(Math.round(c.glyph.advanceWidth * scale * 64), 0, 0xFFFF, `advance of U+${c.cp.toString(16)}`),
            kerning: 0, kerningCount: 0, pairs: [],
        };

        const r = renderGlyphVariants(c.glyph, scale, subpixel);
        if (!r)
            return glyph;

        const key = `${r.width}x${r.height}:${r.data.toString("base64")}`;
        let offset = bitmapOffsets.get(key);
        if (offset === undefined)
        {
            offset = bitmapBytes;
            bitmapOffsets.set(key, offset);
            bitmaps.push(r.data);
            bitmapBytes += r.data.length;
        }
        else
        {
            sharedBitmaps++;
        }

        glyph.bitmap = offset;
        glyph.width = clamp(r.width, 0, 0xFFFF, `width of U+${c.cp.toString(16)}`);
        glyph.height = clamp(r.height, 0, 0xFFFF, `height of U+${c.cp.toString(16)}`);
        glyph.left = clamp(r.left, -0x8000, 0x7FFF, `left of U+${c.cp.toString(16)}`);
        glyph.top = clamp(r.top, -0x8000, 0x7FFF, `top of U+${c.cp.toString(16)}`);
        return glyph;
    });

    // Kerning for every pair of included characters, grouped by left glyph
    // and sorted by right glyph
    let kerningCount = 0;
    if (options.kerning !== false)
    {
        for (let l = 0; l < chars.length; l++)
        {
            for (let r = 0; r < chars.length; r++)
            {
                const adjust = Math.round(pairKerning(font, chars[l], chars[r]) * scale * 64);
                if (adjust != 0)
                    glyphs[l].pairs.push({ right: r, adjust: clamp(adjust, -0x8000, 0x7FFF, "kerning") });
            }
        }
    }
    for (const g of glyphs)
    {
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
    let defaultGlyph;
    if (options.defaultChar != null)
    {
        const cp = typeof options.defaultChar == "string" ? options.defaultChar.codePointAt(0) : options.defaultChar;
        if (!glyphIndex.has(cp))
            throw new Error(`default character U+${cp.toString(16).toUpperCase()} isn't in the packed font`);
        defaultGlyph = glyphIndex.get(cp);
    }
    else
    {
        defaultGlyph = glyphIndex.get(0xFFFD) ?? glyphIndex.get(0x3F) ?? FONT_NO_GLYPH;
    }

    // Font metrics (post table underline position is the top of the line)
    const thickness = font.underlineThickness > 0 ? Math.max(1, Math.round(font.underlineThickness * scale)) : 0;
    const underlineY = Math.round(-font.underlinePosition * scale);

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
    buf.writeUInt16LE(Math.round(size), 12);
    buf.writeUInt16LE(Math.round((font.ascent - font.descent + font.lineGap) * scale), 14);
    buf.writeUInt16LE(Math.round(font.ascent * scale), 16);
    buf.writeUInt16LE(Math.round(-font.descent * scale), 18);
    buf.writeInt16LE(underlineY, 20);
    buf.writeUInt16LE(thickness, 22);
    buf.writeUInt16LE(glyphs.length, 24);
    buf.writeUInt16LE(defaultGlyph, 26);
    buf.writeUInt16LE(hashBits, 28);
    buf.writeUInt16LE(subpixel, 30);
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
            name: font.fullName ?? font.postscriptName,
            size: Math.round(size),
            subpixel,
            glyphs: glyphs.length,
            missing,
            kerningPairs: kerningCount,
            hashSlots: slots.length,
            maxProbe,
            bitmapBytes,
            sharedBitmaps,
            packedSize,
        },
    };
}
