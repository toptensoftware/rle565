#!/usr/bin/env node
import fs from "node:fs";
import path from "node:path";
import { parseArgs } from "node:util";
import { packRle, packPalette, PaletteBuilder } from "./pack.js";

const HELP = `Usage: rle565 [options] <input.png...>

Encodes PNG images to the rle565 run length encoded RGB565 format.

Options:
  -o, --output <file>          Output file (single input only).
                               Default: <input-name>.rle565
  -d, --outdir <dir>           Output directory. Default: same directory as
                               each input. Relative --output and
                               --shared-palette paths are resolved against it.
  -b, --background <bg>        Composite over a background before encoding.
                               Either a .png file, or a solid color as
                               #RRGGBB, 0xRRGGBB or RRGGBB.
  -a, --alpha-rect <x,y,w,h>   Area that keeps its own alpha instead of being
                               baked over the background. Repeatable.
  -p, --shared-palette <file>  Build one palette shared by all inputs, write
                               it to <file> and leave it out of each image.
                               Pass the palette to draw_image() at runtime.
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

// Resolve a path against --outdir (if given)
function inOutdir(file)
{
    return opts.outdir ? path.resolve(opts.outdir, file) : file;
}

function outputFor(input)
{
    if (opts.output)
        return inOutdir(opts.output);
    const name = path.basename(input, path.extname(input)) + ".rle565";
    return path.join(opts.outdir ?? path.dirname(input), name);
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
        for (const input of inputs)
            await packRle(input, { ...packOptions, paletteBuilder: builder });
        packOptions.palette = builder.finalize();

        const paletteFile = inOutdir(opts["shared-palette"]);
        fs.mkdirSync(path.dirname(path.resolve(paletteFile)), { recursive: true });
        fs.writeFileSync(paletteFile, packPalette(packOptions.palette));
        console.log(`${paletteFile} (${packOptions.palette.palette.length} colors)`);
    }

    for (const input of inputs)
    {
        const output = outputFor(input);
        const packed = await packRle(input, packOptions);

        fs.mkdirSync(path.dirname(path.resolve(output)), { recursive: true });
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
