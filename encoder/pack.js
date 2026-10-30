import sharp from "sharp";
import assert from "assert";
import { BufferBuilder } from "./BufferBuilder.js"

export const IMAGE_SIGNATURE = 0x35363552;     // "R565" little endian
export const IMAGE_FORMAT_RLE = 1;
export const IMAGE_FORMAT_BUFFER = 2;
export const DRAW_FLAGS_ALPHA_PIXELS = 0x01;
export const DRAW_FLAGS_LARGE_PALETTE = 0x02;


// Quantize a PNG's RGB channels to RGB565 precision (5/6/5 bits)
export async function quantizePngTo565(filename)
{
    const image = await sharp(filename)
        .ensureAlpha()
        .raw()
        .toBuffer({ resolveWithObject: true });

    for (let i = 0; i < image.data.length; i += 4)
    {
        image.data[i]   &= 0xF8;  // R
        image.data[i+1] &= 0xFC;  // G (6 bits)
        image.data[i+2] &= 0xF8;  // B
        // alpha unchanged
    }

    await sharp(image.data, {
        raw: { width: image.info.width, height: image.info.height, channels: 4 }
    }).toFile(filename);
}

// Helper to format a buffer
export function formatHex(buf)
{
    const rows = [];
    for (let i = 0; i < buf.length; i += 16) {
        const slice = buf.subarray(i, i + 16);
        rows.push('    ' + Array.from(slice, b => `0x${b.toString(16).padStart(2, '0')}`).join(', '));
    }
    return rows.join(',\n');
}

export function packPalette(palette)
{
    let b = Buffer.alloc(4 + palette.palette.length * 4);
    b.writeUInt32LE(palette.palette.length, 0);
    for (let i=0; i<palette.palette.length; i++)
    {
        b.writeUInt32LE(palette.palette[i], 4 + i * 4);
    }
    return b;
}


// Helper to read a pixel.
// Fully opaque pixels: returns 0xFF in the high byte + RGB565 in the low 16 bits.
// Partially transparent pixels: returns full 8-bit RGBA (a<<24 | r<<16 | g<<8 | b).
// Fully transparent pixels: returns 0.
function readPixel(image, x, y)
{
    let idx = (x + y * image.info.width) * 4;
    const [r, g, b, a] = [image.data[idx], image.data[idx+1], image.data[idx+2], image.data[idx+3]];
    if (a == 0)
        return 0;

    if (a == 255)
    {
        const r5 = (r >> 3) & 0x1F;
        const g6 = (g >> 2) & 0x3F;
        const b5 = (b >> 3) & 0x1F;
        return (0xFF000000 | (r5 << 11) | (g6 << 5) | b5) >>> 0;
    }

    // Premultiply using fast /255 approximation
    const pr = (r * a + 128) * 257 >> 16;
    const pg = (g * a + 128) * 257 >> 16;
    const pb = (b * a + 128) * 257 >> 16;

    return ((a << 24) | (pr << 16) | (pg << 8) | pb) >>> 0;
}

export class PaletteBuilder
{
    addColor(color)
    {
        // Track how often this color is used.
        this.#colorUsage.set(color, (this.#colorUsage.get(color) ?? 0) + 1);
    }

    get hasPartialAlpha()
    {
        return [...this.#colorUsage.keys()].some(c => (c >>> 24) != 0xFF && (c >>> 24) != 0);
    }

    finalize()
    {
        // Generate palette with most commonly used colors at lower indicies
        const colorMap = new Map();
        const palette = [];
        const sorted = [...this.#colorUsage.entries()].sort((a, b) => b[1] - a[1]);
        for (let i = 0; i<sorted.length; i++)
        {
            colorMap.set(sorted[i][0], i);
            palette.push(sorted[i][0]);
        }

        // Return the color map and palette
        return {
            colorMap,
            palette,
            hasPartialAlpha: this.hasPartialAlpha,
        }
    }

    #colorUsage = new Map();
}

// Given a png image, packs it into RLE format
export async function packRle(image, options)
{
    // Normalize options
    options = Object.assign({
        background: null,
        alphaRects: [],
    }, options);
    if (options.alphaRect)
    {
        options.alphaRects = [options.alphaRect];
        delete options.alphaRect;
    }

    // Load image from file
    if (typeof(image) === 'string')
    {
        image = await sharp(image)
            .ensureAlpha()
            .raw()
            .toBuffer({ resolveWithObject: true });
    }
    else
    {
        // Otherwise assume image is already { data, info }
    }


    // Composite over background (if any)
    let bakedImage = null;
    if (typeof(options.background) === 'string')
    {
        // Background image file
        bakedImage = await sharp(options.background)
            .ensureAlpha()
            .composite([{ 
                input: image.data,
                raw: image.info, 
            }])
            .raw()
            .toBuffer({ resolveWithObject: true });
    }
    else if (typeof(options.background) === 'number')
    {
        // Solid color background
        const color = `#${options.background.toString(16).padStart(6, '0')}`;
        const svg = `<svg width="${image.info.width}" height="${image.info.height}" xmlns="http://www.w3.org/2000/svg">
                    <rect width="100%" height="100%" fill="${color}"/>
                    </svg>`;

        bakedImage = await sharp(Buffer.from(svg))
            .ensureAlpha()
            .composite([{ 
                input: image.data,
                raw: image.info,
            }])
            .raw()
            .toBuffer({ resolveWithObject: true });
    }
    else if (options.background)
    {
        // Raw imaege background
        bakedImage = await sharp(options.background.data, {raw: options.background.info,})
            .ensureAlpha()
            .composite([{ 
                input: image.data,
                raw: image.info,
            }])
            .raw()
            .toBuffer({ resolveWithObject: true });
    }

    // Find first/last used rows
    let firstRow = null;
    let lastRow = null;
    for (let y=0; y<image.info.height; y++)
    {
        for (let x=0; x<image.info.width; x++)
        {
            if (readPixel(image, x, y) !== 0)
            {
                if (firstRow == null)
                    firstRow = y;
                lastRow = y;
                break;
            }
        }
    }
    let rowCount = firstRow == null ? 0 : lastRow - firstRow + 1;
    if (firstRow == null)
        firstRow = 0;

    function readBakedPixel(x, y)
    {
        // Read original pixel
        let px = readPixel(image, x, y);
        if (px === 0)
            return px;

        // Read pre-baked pixel?
        if (bakedImage)
        {
            // If not in an alpha rect, use the baked pixel
            if (!options.alphaRects.some(r => y >= r.top && y < r.top + r.height && x >= r.left && x < r.left + r.width))
            {
                // Use baked pixel
                return readPixel(bakedImage, x, y);
            }
        }
        return px;
    }

    // If a shared palette builder was provided, accumulate colors into it and return
    if (options.paletteBuilder)
    {
        for (let y=firstRow; y<firstRow + rowCount; y++)
        {
            for (let x=0; x<image.info.width; x++)
            {
                options.paletteBuilder.addColor(readBakedPixel(x, y));
            }
        }
        return null;
    }

    // Use provided shared palette, or build a per-image one
    let palette;
    if (options.palette)
    {
        palette = options.palette;
    }
    else
    {
        let pb = new PaletteBuilder();
        for (let y=firstRow; y<firstRow + rowCount; y++)
        {
            for (let x=0; x<image.info.width; x++)
            {
                pb.addColor(readBakedPixel(x, y));
            }
        }
        palette = pb.finalize();
    }

    // Generate RLE runs
    let rows = [];
    let captureSize = 0;
    let captureRuns = 0;
    for (let y=firstRow; y<firstRow + rowCount; y++)
    {
        let runs = [];
        for (let x=0; x<image.info.width; )
        {
            // Read pixel
            let px = readBakedPixel(x, y);

            // Work out run length
            let count = 1;
            while (x + count < image.info.width)
            {
                if (readBakedPixel(x + count, y) == px)
                    count++;
                else
                    break;
            }
            runs.push({ 
                rle: true, 
                px, 
                pi: palette.colorMap.get(px), 
                count 
            });
            x += count;
        }

        // Coalesc small runs...
        for (let i=0; i<runs.length; i++)
        {
            // Don't allow transparent pixels in non-rle runs
            if (runs[i].px == 0)
                continue;

            // Don' t coalesce runs with different alpha types (partial vs fully opaque)
            let opaque = (runs[i].px >>> 24) == 0xFF;

            let runCount = 0;
            let pxCount = 0;
            while (i + runCount < runs.length)
            {
                if (runs[i + runCount].count <= 2 
                  && runs[i + runCount].px != 0 
                  && ((runs[i + runCount].px >>> 24) == 0xFF) == opaque
                    )
                {
                    pxCount += runs[i + runCount].count;
                    runCount++;
                    continue;
                }

                break;
            }

            if (runCount > 1)
            {
                let newrun = { rle: false, opaque, count: pxCount, px: [], pi: [] };
                for (let j=0; j<runCount; j++)
                {
                    let oldrun = runs[i + j];
                    newrun.px.push(...new Array(oldrun.count).fill(oldrun.px));
                    newrun.pi.push(...new Array(oldrun.count).fill(oldrun.pi));
                }

                runs.splice(i, runCount, newrun);
            }
        }

        let cont = false;
        for (let r of runs)
        {  
            if (r.px === 0)
            {   
                // Can't continue next run, don't need to capture this run
                cont = false;
                continue;
            }

            if (!cont)
            {
                // Need to start a new capture run
                captureRuns++;
                captureSize += 6;       // offset (uint32_t) and length (uint16_t)
                cont = true;
            }

            // Increase size of current capture run
            captureSize += 2 * r.count;
        }
        captureSize += 6;       // Terminator
        rows.push(runs);
    }

    let bb = new BufferBuilder();

    // Write header
    bb.writeUInt32(IMAGE_SIGNATURE);
    bb.writeUInt32(IMAGE_FORMAT_RLE);
    bb.writeUInt32(image.info.width);
    bb.writeUInt32(image.info.height);
    let imageFlags = 0;
    if (palette.hasPartialAlpha) imageFlags |= DRAW_FLAGS_ALPHA_PIXELS;
    if (palette.palette.length > 256) imageFlags |= DRAW_FLAGS_LARGE_PALETTE;
    bb.writeUInt32(imageFlags);
    let packedSizeOffset = bb.pos;
    bb.writeUInt32(0);              // Total size of all packed data
    bb.writeUInt32(captureSize);    // Size to capture
    let paletteOffsetOffset = bb.pos;
    bb.writeUInt32(0);
    bb.writeUInt32(firstRow);
    bb.writeUInt32(rowCount);

    // Reserve room for the row offset table
    let offsetTable = bb.pos;
    bb.reserve(rowCount * 4);

    // Write palette map (omitted when using a shared palette)
    if (!options.palette)
    {
        bb.buffer.writeUInt32LE(bb.pos, paletteOffsetOffset);
        bb.writeUInt32(palette.palette.length);
        for (let p of palette.palette)
        {
            bb.writeUInt32(p);
        }
    }

    // Write row runs
    let rleRuns = 0;
    let transparentRuns = 0;
    let transparentPixels = 0;
    let flatOpaqueRuns = 0;
    let flatAlphaRuns = 0;
    let rlePixels = 0;
    let flatOpaquePixels = 0;
    let flatAlphaPixels = 0;
    for (let iy = 0; iy < rowCount; iy++)
    {
        let runs = rows[iy];

        // Store offset to this row.
        bb.buffer.writeUint32LE(bb.pos, offsetTable + iy * 4);

        // Write number of runs
        bb.writeVarLen(runs.length);

        // Write runs
        for (let r of runs)
        {
            if (r.rle)
            {
                rleRuns++;
                rlePixels += r.count;

                if (r.px == 0)
                {
                    transparentRuns++;
                    transparentPixels += r.count;
                }   

                bb.writeVarLen((r.count << 1) | 1);

                if (palette.palette.length <= 256)
                    bb.writeUInt8(r.pi);
                else
                    bb.writeVarLen(r.pi);
            }
            else
            {
                if (palette.hasPartialAlpha)
                {
                    bb.writeVarLen((r.count << 2) | (r.opaque ? 0 : 2));
                }
                else
                {
                    bb.writeVarLen((r.count << 1) | 0);
                }

                if (r.opaque)
                {
                    flatOpaqueRuns++; 
                    flatOpaquePixels += r.count;
                }
                else
                {
                    flatAlphaRuns++;
                    flatAlphaPixels += r.count;
                }

                for (let i=0; i<r.count; i++)
                {
                    let px = r.px[i];
                    assert(px !== 0, "Transparent pixels should not be in non-rle runs"); 
                    if (r.opaque)
                    {
                        assert((px >>> 24) == 0xFF, "Expected opaque pixel");
                    }
                    else
                    {
                        assert((px >>> 24) != 0xFF, "Expected non-opaque pixel");
                    }

                    let pi = r.pi[i];
                    if (palette.palette.length <= 256)
                        bb.writeUInt8(pi);
                    else    
                        bb.writeVarLen(pi);
                }
            }
        }
    }

    // Write total size
    bb.buffer.writeUint32LE(bb.pos, packedSizeOffset);

    let totalPixels = image.info.width * image.info.height;
    return {
        data: bb.finalize(),
        width: image.info.width,
        height: image.info.height,
        totalPixels,
        paletteSize: palette.palette.length,
        hasPartialAlpha: palette.hasPartialAlpha,
        firstRow,
        rowCount,
        rleRuns,
        rlePixels,
        rlePercent: rlePixels / totalPixels,
        rleAvgRunLength: rlePixels / rleRuns,
        transparentRuns,
        transparentPixels,
        transparentPercent: transparentPixels / totalPixels,
        transparentAvgRunLength: transparentPixels / transparentRuns,
        flatOpaqueRuns,
        flatOpaquePixels,
        flatAlphaRuns,
        flatAlphaPixels,
        flatPercent: (flatOpaquePixels + flatAlphaPixels) / totalPixels,
        flatAvgRunLength: (flatOpaquePixels + flatAlphaPixels) / (flatOpaqueRuns + flatAlphaRuns),  
        packedSize: bb.pos,
        rawSizeRGBA888: totalPixels * 4,
        rawSizeRGB565: totalPixels * 2,
        compressionRatio: bb.pos / (totalPixels * 2),
        captureSize,
        captureRuns,
    }
}
