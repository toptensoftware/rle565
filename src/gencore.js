import fs from "node:fs";

const drawFlags = {
    ALPHA_PIXELS: 0x01,        // Some pixels require blending
    LARGE_PALETTE: 0x02,        // Paletted < 256
    ALPHA_BLEND: 0x04,        // Alpha component needs blending
    XCLIP: 0x08,        // Needs clipping in x-direction
    CAPTURE: 0x10,        // Capture pixels as drawn
};


let o = "";
let h = "static PFNDRAWCORE drawcores[] = {\n";

for (let i=0; i<32; i++)
{
    let fnname = `drawcore_${i.toString(2).padStart(5, '0')}`;

    h += `    ${fnname},\n`;

    let alphaPixels = (i & drawFlags.ALPHA_PIXELS) != 0;
    let largePalette = (i & drawFlags.LARGE_PALETTE) != 0;
    let alphaBlend = (i & drawFlags.ALPHA_BLEND) != 0;
    let xClip = (i & drawFlags.XCLIP) != 0;
    let capture = (i & drawFlags.CAPTURE) != 0;

    let flags = [];
    flags.push(capture ? "capture" : "no-capture")
    flags.push(xClip ? "xClip" : "no-xClip");
    flags.push(alphaBlend ? "alphaBlend" : "no-alphaBlend");
    flags.push(largePalette ? "largePalette" : "smallPalette");
    flags.push(alphaPixels ? "alphaPixels" : "opaquePixels");

    o += `// ${flags.join(", ")}\n`;
    o += `static void ${fnname}(DRAWARGS* args)\n`;
    o += "{\n";
    o += "    uint16_t* buffer = args->buffer;\n"
    o += "    const uint32_t* palette = args->palette->colors;\n";
    if (xClip)
    {
    o += "    uint32_t endX   = args->src_x + args->width;\n";
    }
    o += "    for (uint32_t iy = args->iy_start; iy < args->iy_end; iy++)\n";
    o += "    {\n";
    o += "        uint16_t* rowBuffer = buffer;\n";
    o += "        const uint8_t* runs = (uint8_t*)args->header + args->rowOffsets[iy];\n";
    o += "        uint32_t runCount = ReadVarLen(&runs);\n";
    if (xClip)
    {
    o += "        uint32_t x = 0;\n";
    }
    o += "        for (uint32_t ri = 0; ri < runCount; ri++)\n";
    o += "        {\n";
    o += "            uint32_t pixCount = ReadVarLen(&runs);\n";
    o += "            bool is_rle = pixCount & 0x01;\n";
    o += "            pixCount >>= 1;\n";
    if (alphaPixels)
    {
    if (alphaBlend)
    {
    o += "            if (!is_rle)\n";
    o += "            {\n";
    o += "                pixCount >>= 1;\n";
    o += "            }\n";
    }
    else
    {
    o += "            bool is_alpha = false;\n";
    o += "            if (!is_rle)\n";
    o += "            {\n";
    o += "                is_alpha = pixCount & 0x01;\n";
    o += "                pixCount >>= 1;\n";
    o += "            }\n";
    }
    }
    if (xClip)
    {
    o += "            uint32_t runStart = x;\n";
    o += "            uint32_t runEnd   = x + pixCount;\n";
    o += "            x = runEnd;\n"
    o += "            if (runEnd <= (uint32_t)args->src_x)\n";
    o += "            {\n";
    o += "                if (is_rle)\n";
    o += "                {\n";
    if (largePalette)
    {
    o += "                    ReadVarLen(&runs);\n";
    }
    else
    {
    o += "                    runs++;\n";
    }
    o += "                }\n";
    o += "                else\n";
    o += "                {\n";
    if (largePalette)
    {
    o += "                    SkipVarLen(&runs, pixCount);\n";
    }
    else
    {
    o += "                    runs += pixCount;\n";
    } // !largePalette
    o += "                }\n";
    o += "                continue;\n"
    o += "            }\n";
    o += "            if (runStart >= endX)\n";
    o += "            {\n";
    o += "                break;\n"
    o += "            }\n";
    o += "            uint32_t drawStart = (runStart < (uint32_t)args->src_x) ? args->src_x : runStart;\n";
    o += "            uint32_t drawEnd   = (runEnd   > endX) ? endX  : runEnd;\n";
    o += "            pixCount = drawEnd - drawStart;\n";
    } // !xClip

    o += "            if (is_rle)\n";
    o += "            {\n";
    o += `                uint32_t pi = ${largePalette ? "ReadVarLen(&runs)" : "*runs++"};\n`;
    o += "                uint32_t pixel = palette[pi];\n";

    if (alphaBlend && alphaPixels)
    {
    o += "                uint32_t pixAlpha = pixel & 0xFF000000;\n";
    o += "                if (pixAlpha == 0)\n";
    o += "                {\n";
    o += "                    rowBuffer += pixCount;\n";
    o += "                }\n";
    o += "                else\n";
    o += "                {\n";
    if (capture)
    {
    o += "                    capture_helper(args, rowBuffer, pixCount);\n";
    }
    o += "                    if (pixAlpha == 0xFF000000)\n"
    o += "                    {\n";
    o += "                        for (uint32_t i=0; i<pixCount; i++)\n";
    o += "                        {\n";
    o += "                            *rowBuffer = alpha_blend_rgb565_over_rgb565_with_alpha(*rowBuffer, pixel, args->alpha);\n";
    o += "                            rowBuffer++;\n";
    o += "                        }\n";
    o += "                    }\n";
    o += "                    else\n";
    o += "                    {\n";
    o += "                        for (uint32_t i=0; i<pixCount; i++)\n";
    o += "                        {\n";
    o += "                            *rowBuffer = alpha_blend_premul_rgba8888_over_rgb565_with_alpha(*rowBuffer, pixel, args->alpha);\n";
    o += "                            rowBuffer++;\n";
    o += "                        }\n";
    o += "                    }\n";
    o += "                }\n";
    }
    else if (alphaBlend)
    {
    o += "                if (pixel != 0)\n";
    o += "                {\n";
    if (capture)
    {
    // Only capture visible runs - encoder's captureSize excludes transparent runs
    o += "                    capture_helper(args, rowBuffer, pixCount);\n";
    }
    o += "                    for (uint32_t i=0; i<pixCount; i++)\n";
    o += "                    {\n";
    o += "                        *rowBuffer = alpha_blend_rgb565_over_rgb565_with_alpha(*rowBuffer, pixel, args->alpha);\n";
    o += "                        rowBuffer++;\n";
    o += "                    }\n";
    o += "                }\n";
    o += "                else\n";
    o += "                {\n";
    o += "                    rowBuffer += pixCount;\n";
    o += "                }\n";
    }
    else if (alphaPixels)
    {
    o += "                uint32_t pixAlpha = pixel & 0xFF000000;\n";
    o += "                if (pixAlpha == 0xFF000000)\n";
    o += "                {\n";
    if (capture)
    {
    o += "                    capture_helper(args, rowBuffer, pixCount);\n";
    }
    o += "                    for (uint32_t i=0; i<pixCount; i++)\n";
    o += "                    {\n";
    o += "                        *rowBuffer++ = pixel;\n";
    o += "                    }\n";
    o += "                }\n";
    o += "                else if (pixAlpha == 0)\n";
    o += "                {\n";
    o += "                    rowBuffer += pixCount;\n";
    o += "                }\n";
    o += "                else\n";
    o += "                {\n";
    if (capture)
    {
    o += "                    capture_helper(args, rowBuffer, pixCount);\n";
    }
    o += "                    for (uint32_t i=0; i<pixCount; i++)\n";
    o += "                    {\n";
    o += "                        *rowBuffer = alpha_blend_premul_rgba8888_over_rgb565(*rowBuffer, pixel);\n";
    o += "                        rowBuffer++;\n";
    o += "                    }\n";
    o += "                }\n";
    }
    else
    {
    o += "                if (pixel != 0)\n";
    o += "                {\n";
    if (capture)
    {
    o += "                    capture_helper(args, rowBuffer, pixCount);\n";
    }
    o += "                    for (uint32_t i=0; i<pixCount; i++)\n";
    o += "                    {\n";
    o += "                        *rowBuffer++ = pixel;\n";
    o += "                    }\n";
    o += "                }\n";
    o += "                else\n";
    o += "                {\n";
    o += "                    rowBuffer += pixCount;\n";
    o += "                }\n";
    }

    o += "            }\n";
    o += "            else\n";
    o += "            {\n";

    if (xClip)
    {
    if (largePalette)
    {
    o += "                SkipVarLen(&runs, drawStart - runStart);\n";
    }
    else
    {
    o += "                runs += drawStart - runStart;\n";
    }
    }

    if (capture)
    {
    o += "                capture_helper(args, rowBuffer, pixCount);\n";
    }

    function renderPixelLoop(ap, ab, indent)
    {
    o += `${indent}                for (uint32_t i=0; i<pixCount; i++)\n`;
    o += `${indent}                {\n`;
    o += `${indent}                    uint32_t pi = ${largePalette ? "ReadVarLen(&runs)" : "*runs++"};\n`;
    o += `${indent}                    uint32_t pixel = palette[pi];\n`;
    if (ab && ap)
    {
    o += `${indent}                    uint32_t pixAlpha = pixel & 0xFF000000;\n`;
    o += `${indent}                    if (pixAlpha == 0)\n`;
    o += `${indent}                    {\n`;
    o += `${indent}                    }\n`;
    o += `${indent}                    else\n`;
    o += `${indent}                    {\n`;
    o += `${indent}                        if (pixAlpha == 0xFF000000)\n`
    o += `${indent}                        {\n`;
    o += `${indent}                            *rowBuffer = alpha_blend_rgb565_over_rgb565_with_alpha(*rowBuffer, pixel, args->alpha);\n`;
    o += `${indent}                        }\n`;
    o += `${indent}                        else\n`;
    o += `${indent}                        {\n`;
    o += `${indent}                            *rowBuffer = alpha_blend_premul_rgba8888_over_rgb565_with_alpha(*rowBuffer, pixel, args->alpha);\n`;
    o += `${indent}                        }\n`;
    o += `${indent}                    }\n`;
    }
    else if (ab)
    {
    o += `${indent}                    *rowBuffer = alpha_blend_rgb565_over_rgb565_with_alpha(*rowBuffer, pixel, args->alpha);\n`;
    }
    else if (ap)
    {
    o += `${indent}                    uint32_t pixAlpha = pixel & 0xFF000000;\n`;
    o += `${indent}                    if (pixAlpha == 0xFF000000)\n`;
    o += `${indent}                    {\n`;
    o += `${indent}                        *rowBuffer = pixel;\n`;
    o += `${indent}                    }\n`;
    o += `${indent}                    else if (pixAlpha == 0)\n`;
    o += `${indent}                    {\n`;
    o += `${indent}                    }\n`;
    o += `${indent}                    else\n`;
    o += `${indent}                    {\n`;
    o += `${indent}                        *rowBuffer = alpha_blend_premul_rgba8888_over_rgb565(*rowBuffer, pixel);\n`
    o += `${indent}                    }\n`;
    }
    else
    {
    o += `${indent}                    *rowBuffer = pixel;\n`;
    }
    o += `${indent}                    rowBuffer++;\n`;
    o += `${indent}                }\n`;
    } // RenderPixelLoop


    if (alphaPixels && !alphaBlend)
    {
    o += "                if (is_alpha)\n";
    o += "                {\n";
    renderPixelLoop(true, false, "    ");
    o += "                }\n";
    o += "                else\n";
    o += "                {\n";
    renderPixelLoop(false, false, "    ");
    o += "                }\n";
    }
    else
    {
        renderPixelLoop(alphaPixels, alphaBlend, "");
    }

    o += "            }\n";
    o += "        }\n";
    o += "        buffer += args->stride;\n"
    o += "    }\n";
    if (capture)
    {
    o += "    capture_finalize(args);\n";
    }
    o += "}\n\n";
}

h += "};\n";
o += "\n\n" + h;

fs.writeFileSync("src/rle565.core.h", o, "utf8");
