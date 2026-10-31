#!/usr/bin/env node
import fs from "node:fs";
import path from "node:path";
import { parseArgs } from "node:util";
import { packRle, packPalette, PaletteBuilder } from "./pack.js";
import { unpackRleToPng, readPalette } from "./unpack.js";
import { packFont, parseCharset, FONT_SUBPIXEL_VALUES } from "./font.js";

const FONT_EXTENSIONS = [".ttf", ".otf", ".woff", ".woff2", ".ttc", ".otc"];

const HELP = `Usage: rle565 [options] <input...>

Encodes PNG images to the rle565 run length encoded RGB565 format,
decodes .rle565 images back to PNG, and packs fonts.  Each input is
converted based on its extension: .rle565 files are decoded, font files
(.ttf, .otf, .woff, .woff2) are packed to .rfnt, and anything else is
encoded as an image.

Options:
  -o, --output <file>          Output file (single input only).
                               Default: <input-name>.rle565 when encoding,
                               <input-name>.png when decoding,
                               <input-name>.rfnt for fonts
  -d, --outdir <dir>           Output directory. Default: same directory as
                               each input. Relative --output and
                               --shared-palette paths are resolved against it.

Encoding options:
  -b, --background <bg>        Composite over a background before encoding.
                               Either a .png file, or a solid color as
                               #RRGGBB, 0xRRGGBB or RRGGBB.
  -a, --alpha-rect <x,y,w,h>   Area that keeps its own alpha instead of being
                               baked over the background. Repeatable.
  -p, --shared-palette <file>  Build one palette shared by all inputs, write
                               it to <file> and leave it out of each image.
                               Pass the palette to draw_image() at runtime.

Decoding options:
  -P, --palette <file>         Palette file (from --shared-palette) for
                               images encoded with a shared palette.

Font options:
  -s, --size <px>              Font size in pixels per em. Required for fonts.
  -c, --chars <ranges>         Characters to include, as codepoints and
                               ranges in decimal, 0x hex or U+ hex, eg:
                               "32-126,0xB0,U+20AC". Default: 32-126 (ASCII).
      --subpixel <n>           Render each glyph at n horizontal positions
                               per pixel (2, 4 or 8) for more accurate
                               spacing. Multiplies glyph bitmap size by n.
                               Default: 1 (whole pixels).
      --no-kerning             Leave out kerning.
  -D, --default-char <char>    Character drawn for characters missing from
                               the font. Default: U+FFFD or '?' if present.

General:
  -v, --verbose                Print encoding statistics.
  -h, --help                   Show this help.
`;

function fail(msg)
{
    console.error(`rle565: ${msg}`);
    console.error(`Run with --help for usage.`);
    process.exit(1);
}

// Parse "x,y,w,h" into an alpha rect
function parseAlphaRect(str)
{
    const parts = str.split(",").map(s => s.trim());
    if (parts.length != 4 || !parts.every(p => /^\d+$/.test(p)))
        fail(`invalid --alpha-rect '${str}', expected x,y,w,h`);
    const [left, top, width, height] = parts.map(Number);
    return { left, top, width, height };
}

// A background is either an existing file or a solid color
function parseBackground(str)
{
    if (fs.existsSync(str))
        return str;

    const m = /^(?:#|0x)?([0-9a-f]{6})$/i.exec(str);
    if (m)
        return parseInt(m[1], 16);

    fail(`background '${str}' is not an existing file or a color (#RRGGBB)`);
}

function formatStats(file, stats)
{
    const pct = (v) => `${(v * 100).toFixed(1)}%`;
    const avg = (v) => Number.isFinite(v) ? v.toFixed(1) : "-";
    return [
        `${file}`,
        `  size:        ${stats.width}x${stats.height}, rows ${stats.firstRow}..${stats.firstRow + stats.rowCount - 1}`,
        `  palette:     ${stats.paletteSize} colors${stats.hasPartialAlpha ? ", partial alpha" : ""}`,
        `  rle runs:    ${stats.rleRuns} (${pct(stats.rlePercent)} of pixels, avg ${avg(stats.rleAvgRunLength)})`,
        `  transparent: ${stats.transparentRuns} runs (${pct(stats.transparentPercent)} of pixels)`,
        `  flat runs:   ${stats.flatOpaqueRuns} opaque, ${stats.flatAlphaRuns} alpha (${pct(stats.flatPercent)} of pixels, avg ${avg(stats.flatAvgRunLength)})`,
        `  packed:      ${stats.packedSize} bytes (${pct(stats.compressionRatio)} of raw RGB565)`,
        `  capture:     ${stats.captureSize} bytes`,
    ].join("\n");
}

let args;
try
{
    args = parseArgs({
        allowPositionals: true,
        options: {
            "output":         { type: "string",  short: "o" },
            "outdir":         { type: "string",  short: "d" },
            "background":     { type: "string",  short: "b" },
            "alpha-rect":     { type: "string",  short: "a", multiple: true },
            "shared-palette": { type: "string",  short: "p" },
            "palette":        { type: "string",  short: "P" },
            "size":           { type: "string",  short: "s" },
            "chars":          { type: "string",  short: "c" },
            "subpixel":       { type: "string" },
            "no-kerning":     { type: "boolean" },
            "default-char":   { type: "string",  short: "D" },
            "verbose":        { type: "boolean", short: "v" },
            "help":           { type: "boolean", short: "h" },
        },
    });
}
catch (err)
{
    fail(err.message);
}

const opts = args.values;
const inputs = args.positionals;

if (opts.help)
{
    process.stdout.write(HELP);
    process.exit(0);
}

if (inputs.length == 0)
    fail("no input files");
if (opts.output && inputs.length > 1)
    fail("--output can only be used with a single input; use --outdir instead");
for (const input of inputs)
{
    if (!fs.existsSync(input))
        fail(`input file '${input}' not found`);
}
if (opts.palette && !fs.existsSync(opts.palette))
    fail(`palette file '${opts.palette}' not found`);

// Resolve a path against --outdir (if given)
function inOutdir(file)
{
    return opts.outdir ? path.resolve(opts.outdir, file) : file;
}

function isRle565(file)
{
    return path.extname(file).toLowerCase() == ".rle565";
}

function isFont(file)
{
    return FONT_EXTENSIONS.includes(path.extname(file).toLowerCase());
}

function outputFor(input)
{
    if (opts.output)
        return inOutdir(opts.output);
    const ext = isRle565(input) ? ".png" : isFont(input) ? ".rfnt" : ".rle565";
    const name = path.basename(input, path.extname(input)) + ext;
    return path.join(opts.outdir ?? path.dirname(input), name);
}

const encodeInputs = inputs.filter(f => !isRle565(f) && !isFont(f));
if (opts["shared-palette"] && encodeInputs.length == 0)
    fail("--shared-palette needs at least one image to encode");

// Font options
const fontOptions = {};
if (inputs.some(isFont))
{
    if (opts.size == null)
        fail("--size is required for fonts");
    fontOptions.size = Number(opts.size);
    if (!(fontOptions.size > 0 && fontOptions.size <= 1000))
        fail(`invalid --size '${opts.size}'`);

    fontOptions.subpixel = opts.subpixel != null ? Number(opts.subpixel) : 1;
    if (!FONT_SUBPIXEL_VALUES.includes(fontOptions.subpixel))
        fail(`invalid --subpixel '${opts.subpixel}', expected ${FONT_SUBPIXEL_VALUES.join(", ")}`);

    try
    {
        fontOptions.charset = parseCharset(opts.chars ?? "32-126");
    }
    catch (err)
    {
        fail(err.message);
    }
    fontOptions.kerning = !opts["no-kerning"];
    fontOptions.defaultChar = opts["default-char"];
}

function formatCodepoints(cps)
{
    const list = cps.slice(0, 10).map(cp => "U+" + cp.toString(16).toUpperCase().padStart(4, "0")).join(", ");
    return cps.length > 10 ? `${list} and ${cps.length - 10} more` : list;
}

const packOptions = {
    background: opts.background != null ? parseBackground(opts.background) : null,
    alphaRects: (opts["alpha-rect"] ?? []).map(parseAlphaRect),
};

try
{
    // Shared palette: first pass collects colors from every input
    if (opts["shared-palette"])
    {
        const builder = new PaletteBuilder();
        for (const input of encodeInputs)
            await packRle(input, { ...packOptions, paletteBuilder: builder });
        packOptions.palette = builder.finalize();

        const paletteFile = inOutdir(opts["shared-palette"]);
        fs.mkdirSync(path.dirname(path.resolve(paletteFile)), { recursive: true });
        fs.writeFileSync(paletteFile, packPalette(packOptions.palette));
        console.log(`${paletteFile} (${packOptions.palette.palette.length} colors)`);
    }

    // Palette for decoding shared palette images
    const decodeOptions = {};
    if (opts.palette)
        decodeOptions.palette = readPalette(fs.readFileSync(opts.palette));

    for (const input of inputs)
    {
        const output = outputFor(input);
        fs.mkdirSync(path.dirname(path.resolve(output)), { recursive: true });

        if (isRle565(input))
        {
            let image;
            try
            {
                image = await unpackRleToPng(fs.readFileSync(input), output, decodeOptions);
            }
            catch (err)
            {
                const hint = opts.palette ? "" : / shared palette/.test(err.message) ? " (use --palette)" : "";
                throw new Error(`${input}: ${err.message}${hint}`);
            }
            console.log(`${output} (${image.width}x${image.height})`);
            continue;
        }

        if (isFont(input))
        {
            let font;
            try
            {
                font = packFont(input, fontOptions);
            }
            catch (err)
            {
                throw new Error(`${input}: ${err.message}`);
            }
            fs.writeFileSync(output, font.data);

            const st = font.stats;
            if (opts.verbose)
            {
                console.log([
                    `${output}`,
                    `  font:     ${st.name}, ${st.size}px${st.subpixel > 1 ? `, ${st.subpixel} subpixel positions` : ""}`,
                    `  glyphs:   ${st.glyphs}, ${st.kerningPairs} kerning pairs`,
                    `  bitmaps:  ${st.bitmapBytes} bytes (${st.sharedBitmaps} shared)`,
                    `  hash:     ${st.hashSlots} slots, longest probe ${st.maxProbe}`,
                    `  packed:   ${st.packedSize} bytes`,
                ].join("\n"));
            }
            else
            {
                console.log(`${output} (${st.glyphs} glyphs, ${st.packedSize} bytes)`);
            }
            if (st.missing.length)
                console.warn(`warning: ${st.missing.length} characters aren't in the font: ${formatCodepoints(st.missing)}`);
            continue;
        }

        const packed = await packRle(input, packOptions);
        fs.writeFileSync(output, packed.data);

        if (opts.verbose)
            console.log(formatStats(output, packed));
        else
            console.log(`${output} (${packed.packedSize} bytes)`);
    }
}
catch (err)
{
    fail(err.message);
}
