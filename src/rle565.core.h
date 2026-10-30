// no-capture, no-xClip, no-alphaBlend, smallPalette, opaquePixels
static void drawcore_00000(DRAWARGS* args)
{
    uint16_t* buffer = args->buffer;
    const uint32_t* palette = args->palette->colors;
    for (uint32_t iy = args->iy_start; iy < args->iy_end; iy++)
    {
        uint16_t* rowBuffer = buffer;
        const uint8_t* runs = (uint8_t*)args->header + args->rowOffsets[iy];
        uint32_t runCount = ReadVarLen(&runs);
        for (uint32_t ri = 0; ri < runCount; ri++)
        {
            uint32_t pixCount = ReadVarLen(&runs);
            bool is_rle = pixCount & 0x01;
            pixCount >>= 1;
            if (is_rle)
            {
                uint32_t pi = *runs++;
                uint32_t pixel = palette[pi];
                if (pixel != 0)
                {
                    for (uint32_t i=0; i<pixCount; i++)
                    {
                        *rowBuffer++ = pixel;
                    }
                }
                else
                {
                    rowBuffer += pixCount;
                }
            }
            else
            {
                for (uint32_t i=0; i<pixCount; i++)
                {
                    uint32_t pi = *runs++;
                    uint32_t pixel = palette[pi];
                    *rowBuffer = pixel;
                    rowBuffer++;
                }
            }
        }
        buffer += args->stride;
    }
}

// no-capture, no-xClip, no-alphaBlend, smallPalette, alphaPixels
static void drawcore_00001(DRAWARGS* args)
{
    uint16_t* buffer = args->buffer;
    const uint32_t* palette = args->palette->colors;
    for (uint32_t iy = args->iy_start; iy < args->iy_end; iy++)
    {
        uint16_t* rowBuffer = buffer;
        const uint8_t* runs = (uint8_t*)args->header + args->rowOffsets[iy];
        uint32_t runCount = ReadVarLen(&runs);
        for (uint32_t ri = 0; ri < runCount; ri++)
        {
            uint32_t pixCount = ReadVarLen(&runs);
            bool is_rle = pixCount & 0x01;
            pixCount >>= 1;
            bool is_alpha = false;
            if (!is_rle)
            {
                is_alpha = pixCount & 0x01;
                pixCount >>= 1;
            }
            if (is_rle)
            {
                uint32_t pi = *runs++;
                uint32_t pixel = palette[pi];
                uint32_t pixAlpha = pixel & 0xFF000000;
                if (pixAlpha == 0xFF000000)
                {
                    for (uint32_t i=0; i<pixCount; i++)
                    {
                        *rowBuffer++ = pixel;
                    }
                }
                else if (pixAlpha == 0)
                {
                    rowBuffer += pixCount;
                }
                else
                {
                    for (uint32_t i=0; i<pixCount; i++)
                    {
                        *rowBuffer = alpha_blend_premul_rgba8888_over_rgb565(*rowBuffer, pixel);
                        rowBuffer++;
                    }
                }
            }
            else
            {
                if (is_alpha)
                {
                    for (uint32_t i=0; i<pixCount; i++)
                    {
                        uint32_t pi = *runs++;
                        uint32_t pixel = palette[pi];
                        uint32_t pixAlpha = pixel & 0xFF000000;
                        if (pixAlpha == 0xFF000000)
                        {
                            *rowBuffer = pixel;
                        }
                        else if (pixAlpha == 0)
                        {
                        }
                        else
                        {
                            *rowBuffer = alpha_blend_premul_rgba8888_over_rgb565(*rowBuffer, pixel);
                        }
                        rowBuffer++;
                    }
                }
                else
                {
                    for (uint32_t i=0; i<pixCount; i++)
                    {
                        uint32_t pi = *runs++;
                        uint32_t pixel = palette[pi];
                        *rowBuffer = pixel;
                        rowBuffer++;
                    }
                }
            }
        }
        buffer += args->stride;
    }
}

// no-capture, no-xClip, no-alphaBlend, largePalette, opaquePixels
static void drawcore_00010(DRAWARGS* args)
{
    uint16_t* buffer = args->buffer;
    const uint32_t* palette = args->palette->colors;
    for (uint32_t iy = args->iy_start; iy < args->iy_end; iy++)
    {
        uint16_t* rowBuffer = buffer;
        const uint8_t* runs = (uint8_t*)args->header + args->rowOffsets[iy];
        uint32_t runCount = ReadVarLen(&runs);
        for (uint32_t ri = 0; ri < runCount; ri++)
        {
            uint32_t pixCount = ReadVarLen(&runs);
            bool is_rle = pixCount & 0x01;
            pixCount >>= 1;
            if (is_rle)
            {
                uint32_t pi = ReadVarLen(&runs);
                uint32_t pixel = palette[pi];
                if (pixel != 0)
                {
                    for (uint32_t i=0; i<pixCount; i++)
                    {
                        *rowBuffer++ = pixel;
                    }
                }
                else
                {
                    rowBuffer += pixCount;
                }
            }
            else
            {
                for (uint32_t i=0; i<pixCount; i++)
                {
                    uint32_t pi = ReadVarLen(&runs);
                    uint32_t pixel = palette[pi];
                    *rowBuffer = pixel;
                    rowBuffer++;
                }
            }
        }
        buffer += args->stride;
    }
}

// no-capture, no-xClip, no-alphaBlend, largePalette, alphaPixels
static void drawcore_00011(DRAWARGS* args)
{
    uint16_t* buffer = args->buffer;
    const uint32_t* palette = args->palette->colors;
    for (uint32_t iy = args->iy_start; iy < args->iy_end; iy++)
    {
        uint16_t* rowBuffer = buffer;
        const uint8_t* runs = (uint8_t*)args->header + args->rowOffsets[iy];
        uint32_t runCount = ReadVarLen(&runs);
        for (uint32_t ri = 0; ri < runCount; ri++)
        {
            uint32_t pixCount = ReadVarLen(&runs);
            bool is_rle = pixCount & 0x01;
            pixCount >>= 1;
            bool is_alpha = false;
            if (!is_rle)
            {
                is_alpha = pixCount & 0x01;
                pixCount >>= 1;
            }
            if (is_rle)
            {
                uint32_t pi = ReadVarLen(&runs);
                uint32_t pixel = palette[pi];
                uint32_t pixAlpha = pixel & 0xFF000000;
                if (pixAlpha == 0xFF000000)
                {
                    for (uint32_t i=0; i<pixCount; i++)
                    {
                        *rowBuffer++ = pixel;
                    }
                }
                else if (pixAlpha == 0)
                {
                    rowBuffer += pixCount;
                }
                else
                {
                    for (uint32_t i=0; i<pixCount; i++)
                    {
                        *rowBuffer = alpha_blend_premul_rgba8888_over_rgb565(*rowBuffer, pixel);
                        rowBuffer++;
                    }
                }
            }
            else
            {
                if (is_alpha)
                {
                    for (uint32_t i=0; i<pixCount; i++)
                    {
                        uint32_t pi = ReadVarLen(&runs);
                        uint32_t pixel = palette[pi];
                        uint32_t pixAlpha = pixel & 0xFF000000;
                        if (pixAlpha == 0xFF000000)
                        {
                            *rowBuffer = pixel;
                        }
                        else if (pixAlpha == 0)
                        {
                        }
                        else
                        {
                            *rowBuffer = alpha_blend_premul_rgba8888_over_rgb565(*rowBuffer, pixel);
                        }
                        rowBuffer++;
                    }
                }
                else
                {
                    for (uint32_t i=0; i<pixCount; i++)
                    {
                        uint32_t pi = ReadVarLen(&runs);
                        uint32_t pixel = palette[pi];
                        *rowBuffer = pixel;
                        rowBuffer++;
                    }
                }
            }
        }
        buffer += args->stride;
    }
}

// no-capture, no-xClip, alphaBlend, smallPalette, opaquePixels
static void drawcore_00100(DRAWARGS* args)
{
    uint16_t* buffer = args->buffer;
    const uint32_t* palette = args->palette->colors;
    for (uint32_t iy = args->iy_start; iy < args->iy_end; iy++)
    {
        uint16_t* rowBuffer = buffer;
        const uint8_t* runs = (uint8_t*)args->header + args->rowOffsets[iy];
        uint32_t runCount = ReadVarLen(&runs);
        for (uint32_t ri = 0; ri < runCount; ri++)
        {
            uint32_t pixCount = ReadVarLen(&runs);
            bool is_rle = pixCount & 0x01;
            pixCount >>= 1;
            if (is_rle)
            {
                uint32_t pi = *runs++;
                uint32_t pixel = palette[pi];
                if (pixel != 0)
                {
                    for (uint32_t i=0; i<pixCount; i++)
                    {
                        *rowBuffer = alpha_blend_rgb565_over_rgb565_with_alpha(*rowBuffer, pixel, args->alpha);
                        rowBuffer++;
                    }
                }
                else
                {
                    rowBuffer += pixCount;
                }
            }
            else
            {
                for (uint32_t i=0; i<pixCount; i++)
                {
                    uint32_t pi = *runs++;
                    uint32_t pixel = palette[pi];
                    *rowBuffer = alpha_blend_rgb565_over_rgb565_with_alpha(*rowBuffer, pixel, args->alpha);
                    rowBuffer++;
                }
            }
        }
        buffer += args->stride;
    }
}

// no-capture, no-xClip, alphaBlend, smallPalette, alphaPixels
static void drawcore_00101(DRAWARGS* args)
{
    uint16_t* buffer = args->buffer;
    const uint32_t* palette = args->palette->colors;
    for (uint32_t iy = args->iy_start; iy < args->iy_end; iy++)
    {
        uint16_t* rowBuffer = buffer;
        const uint8_t* runs = (uint8_t*)args->header + args->rowOffsets[iy];
        uint32_t runCount = ReadVarLen(&runs);
        for (uint32_t ri = 0; ri < runCount; ri++)
        {
            uint32_t pixCount = ReadVarLen(&runs);
            bool is_rle = pixCount & 0x01;
            pixCount >>= 1;
            if (!is_rle)
            {
                pixCount >>= 1;
            }
            if (is_rle)
            {
                uint32_t pi = *runs++;
                uint32_t pixel = palette[pi];
                uint32_t pixAlpha = pixel & 0xFF000000;
                if (pixAlpha == 0)
                {
                    rowBuffer += pixCount;
                }
                else
                {
                    if (pixAlpha == 0xFF000000)
                    {
                        for (uint32_t i=0; i<pixCount; i++)
                        {
                            *rowBuffer = alpha_blend_rgb565_over_rgb565_with_alpha(*rowBuffer, pixel, args->alpha);
                            rowBuffer++;
                        }
                    }
                    else
                    {
                        for (uint32_t i=0; i<pixCount; i++)
                        {
                            *rowBuffer = alpha_blend_premul_rgba8888_over_rgb565_with_alpha(*rowBuffer, pixel, args->alpha);
                            rowBuffer++;
                        }
                    }
                }
            }
            else
            {
                for (uint32_t i=0; i<pixCount; i++)
                {
                    uint32_t pi = *runs++;
                    uint32_t pixel = palette[pi];
                    uint32_t pixAlpha = pixel & 0xFF000000;
                    if (pixAlpha == 0)
                    {
                    }
                    else
                    {
                        if (pixAlpha == 0xFF000000)
                        {
                            *rowBuffer = alpha_blend_rgb565_over_rgb565_with_alpha(*rowBuffer, pixel, args->alpha);
                        }
                        else
                        {
                            *rowBuffer = alpha_blend_premul_rgba8888_over_rgb565_with_alpha(*rowBuffer, pixel, args->alpha);
                        }
                    }
                    rowBuffer++;
                }
            }
        }
        buffer += args->stride;
    }
}

// no-capture, no-xClip, alphaBlend, largePalette, opaquePixels
static void drawcore_00110(DRAWARGS* args)
{
    uint16_t* buffer = args->buffer;
    const uint32_t* palette = args->palette->colors;
    for (uint32_t iy = args->iy_start; iy < args->iy_end; iy++)
    {
        uint16_t* rowBuffer = buffer;
        const uint8_t* runs = (uint8_t*)args->header + args->rowOffsets[iy];
        uint32_t runCount = ReadVarLen(&runs);
        for (uint32_t ri = 0; ri < runCount; ri++)
        {
            uint32_t pixCount = ReadVarLen(&runs);
            bool is_rle = pixCount & 0x01;
            pixCount >>= 1;
            if (is_rle)
            {
                uint32_t pi = ReadVarLen(&runs);
                uint32_t pixel = palette[pi];
                if (pixel != 0)
                {
                    for (uint32_t i=0; i<pixCount; i++)
                    {
                        *rowBuffer = alpha_blend_rgb565_over_rgb565_with_alpha(*rowBuffer, pixel, args->alpha);
                        rowBuffer++;
                    }
                }
                else
                {
                    rowBuffer += pixCount;
                }
            }
            else
            {
                for (uint32_t i=0; i<pixCount; i++)
                {
                    uint32_t pi = ReadVarLen(&runs);
                    uint32_t pixel = palette[pi];
                    *rowBuffer = alpha_blend_rgb565_over_rgb565_with_alpha(*rowBuffer, pixel, args->alpha);
                    rowBuffer++;
                }
            }
        }
        buffer += args->stride;
    }
}

// no-capture, no-xClip, alphaBlend, largePalette, alphaPixels
static void drawcore_00111(DRAWARGS* args)
{
    uint16_t* buffer = args->buffer;
    const uint32_t* palette = args->palette->colors;
    for (uint32_t iy = args->iy_start; iy < args->iy_end; iy++)
    {
        uint16_t* rowBuffer = buffer;
        const uint8_t* runs = (uint8_t*)args->header + args->rowOffsets[iy];
        uint32_t runCount = ReadVarLen(&runs);
        for (uint32_t ri = 0; ri < runCount; ri++)
        {
            uint32_t pixCount = ReadVarLen(&runs);
            bool is_rle = pixCount & 0x01;
            pixCount >>= 1;
            if (!is_rle)
            {
                pixCount >>= 1;
            }
            if (is_rle)
            {
                uint32_t pi = ReadVarLen(&runs);
                uint32_t pixel = palette[pi];
                uint32_t pixAlpha = pixel & 0xFF000000;
                if (pixAlpha == 0)
                {
                    rowBuffer += pixCount;
                }
                else
                {
                    if (pixAlpha == 0xFF000000)
                    {
                        for (uint32_t i=0; i<pixCount; i++)
                        {
                            *rowBuffer = alpha_blend_rgb565_over_rgb565_with_alpha(*rowBuffer, pixel, args->alpha);
                            rowBuffer++;
                        }
                    }
                    else
                    {
                        for (uint32_t i=0; i<pixCount; i++)
                        {
                            *rowBuffer = alpha_blend_premul_rgba8888_over_rgb565_with_alpha(*rowBuffer, pixel, args->alpha);
                            rowBuffer++;
                        }
                    }
                }
            }
            else
            {
                for (uint32_t i=0; i<pixCount; i++)
                {
                    uint32_t pi = ReadVarLen(&runs);
                    uint32_t pixel = palette[pi];
                    uint32_t pixAlpha = pixel & 0xFF000000;
                    if (pixAlpha == 0)
                    {
                    }
                    else
                    {
                        if (pixAlpha == 0xFF000000)
                        {
                            *rowBuffer = alpha_blend_rgb565_over_rgb565_with_alpha(*rowBuffer, pixel, args->alpha);
                        }
                        else
                        {
                            *rowBuffer = alpha_blend_premul_rgba8888_over_rgb565_with_alpha(*rowBuffer, pixel, args->alpha);
                        }
                    }
                    rowBuffer++;
                }
            }
        }
        buffer += args->stride;
    }
}

// no-capture, xClip, no-alphaBlend, smallPalette, opaquePixels
static void drawcore_01000(DRAWARGS* args)
{
    uint16_t* buffer = args->buffer;
    const uint32_t* palette = args->palette->colors;
    uint32_t endX   = args->src_x + args->width;
    for (uint32_t iy = args->iy_start; iy < args->iy_end; iy++)
    {
        uint16_t* rowBuffer = buffer;
        const uint8_t* runs = (uint8_t*)args->header + args->rowOffsets[iy];
        uint32_t runCount = ReadVarLen(&runs);
        uint32_t x = 0;
        for (uint32_t ri = 0; ri < runCount; ri++)
        {
            uint32_t pixCount = ReadVarLen(&runs);
            bool is_rle = pixCount & 0x01;
            pixCount >>= 1;
            uint32_t runStart = x;
            uint32_t runEnd   = x + pixCount;
            x = runEnd;
            if (runEnd <= (uint32_t)args->src_x)
            {
                if (is_rle)
                {
                    runs++;
                }
                else
                {
                    runs += pixCount;
                }
                continue;
            }
            if (runStart >= endX)
            {
                break;
            }
            uint32_t drawStart = (runStart < (uint32_t)args->src_x) ? args->src_x : runStart;
            uint32_t drawEnd   = (runEnd   > endX) ? endX  : runEnd;
            pixCount = drawEnd - drawStart;
            if (is_rle)
            {
                uint32_t pi = *runs++;
                uint32_t pixel = palette[pi];
                if (pixel != 0)
                {
                    for (uint32_t i=0; i<pixCount; i++)
                    {
                        *rowBuffer++ = pixel;
                    }
                }
                else
                {
                    rowBuffer += pixCount;
                }
            }
            else
            {
                runs += drawStart - runStart;
                for (uint32_t i=0; i<pixCount; i++)
                {
                    uint32_t pi = *runs++;
                    uint32_t pixel = palette[pi];
                    *rowBuffer = pixel;
                    rowBuffer++;
                }
            }
        }
        buffer += args->stride;
    }
}

// no-capture, xClip, no-alphaBlend, smallPalette, alphaPixels
static void drawcore_01001(DRAWARGS* args)
{
    uint16_t* buffer = args->buffer;
    const uint32_t* palette = args->palette->colors;
    uint32_t endX   = args->src_x + args->width;
    for (uint32_t iy = args->iy_start; iy < args->iy_end; iy++)
    {
        uint16_t* rowBuffer = buffer;
        const uint8_t* runs = (uint8_t*)args->header + args->rowOffsets[iy];
        uint32_t runCount = ReadVarLen(&runs);
        uint32_t x = 0;
        for (uint32_t ri = 0; ri < runCount; ri++)
        {
            uint32_t pixCount = ReadVarLen(&runs);
            bool is_rle = pixCount & 0x01;
            pixCount >>= 1;
            bool is_alpha = false;
            if (!is_rle)
            {
                is_alpha = pixCount & 0x01;
                pixCount >>= 1;
            }
            uint32_t runStart = x;
            uint32_t runEnd   = x + pixCount;
            x = runEnd;
            if (runEnd <= (uint32_t)args->src_x)
            {
                if (is_rle)
                {
                    runs++;
                }
                else
                {
                    runs += pixCount;
                }
                continue;
            }
            if (runStart >= endX)
            {
                break;
            }
            uint32_t drawStart = (runStart < (uint32_t)args->src_x) ? args->src_x : runStart;
            uint32_t drawEnd   = (runEnd   > endX) ? endX  : runEnd;
            pixCount = drawEnd - drawStart;
            if (is_rle)
            {
                uint32_t pi = *runs++;
                uint32_t pixel = palette[pi];
                uint32_t pixAlpha = pixel & 0xFF000000;
                if (pixAlpha == 0xFF000000)
                {
                    for (uint32_t i=0; i<pixCount; i++)
                    {
                        *rowBuffer++ = pixel;
                    }
                }
                else if (pixAlpha == 0)
                {
                    rowBuffer += pixCount;
                }
                else
                {
                    for (uint32_t i=0; i<pixCount; i++)
                    {
                        *rowBuffer = alpha_blend_premul_rgba8888_over_rgb565(*rowBuffer, pixel);
                        rowBuffer++;
                    }
                }
            }
            else
            {
                runs += drawStart - runStart;
                if (is_alpha)
                {
                    for (uint32_t i=0; i<pixCount; i++)
                    {
                        uint32_t pi = *runs++;
                        uint32_t pixel = palette[pi];
                        uint32_t pixAlpha = pixel & 0xFF000000;
                        if (pixAlpha == 0xFF000000)
                        {
                            *rowBuffer = pixel;
                        }
                        else if (pixAlpha == 0)
                        {
                        }
                        else
                        {
                            *rowBuffer = alpha_blend_premul_rgba8888_over_rgb565(*rowBuffer, pixel);
                        }
                        rowBuffer++;
                    }
                }
                else
                {
                    for (uint32_t i=0; i<pixCount; i++)
                    {
                        uint32_t pi = *runs++;
                        uint32_t pixel = palette[pi];
                        *rowBuffer = pixel;
                        rowBuffer++;
                    }
                }
            }
        }
        buffer += args->stride;
    }
}

// no-capture, xClip, no-alphaBlend, largePalette, opaquePixels
static void drawcore_01010(DRAWARGS* args)
{
    uint16_t* buffer = args->buffer;
    const uint32_t* palette = args->palette->colors;
    uint32_t endX   = args->src_x + args->width;
    for (uint32_t iy = args->iy_start; iy < args->iy_end; iy++)
    {
        uint16_t* rowBuffer = buffer;
        const uint8_t* runs = (uint8_t*)args->header + args->rowOffsets[iy];
        uint32_t runCount = ReadVarLen(&runs);
        uint32_t x = 0;
        for (uint32_t ri = 0; ri < runCount; ri++)
        {
            uint32_t pixCount = ReadVarLen(&runs);
            bool is_rle = pixCount & 0x01;
            pixCount >>= 1;
            uint32_t runStart = x;
            uint32_t runEnd   = x + pixCount;
            x = runEnd;
            if (runEnd <= (uint32_t)args->src_x)
            {
                if (is_rle)
                {
                    ReadVarLen(&runs);
                }
                else
                {
                    SkipVarLen(&runs, pixCount);
                }
                continue;
            }
            if (runStart >= endX)
            {
                break;
            }
            uint32_t drawStart = (runStart < (uint32_t)args->src_x) ? args->src_x : runStart;
            uint32_t drawEnd   = (runEnd   > endX) ? endX  : runEnd;
            pixCount = drawEnd - drawStart;
            if (is_rle)
            {
                uint32_t pi = ReadVarLen(&runs);
                uint32_t pixel = palette[pi];
                if (pixel != 0)
                {
                    for (uint32_t i=0; i<pixCount; i++)
                    {
                        *rowBuffer++ = pixel;
                    }
                }
                else
                {
                    rowBuffer += pixCount;
                }
            }
            else
            {
                SkipVarLen(&runs, drawStart - runStart);
                for (uint32_t i=0; i<pixCount; i++)
                {
                    uint32_t pi = ReadVarLen(&runs);
                    uint32_t pixel = palette[pi];
                    *rowBuffer = pixel;
                    rowBuffer++;
                }
            }
        }
        buffer += args->stride;
    }
}

// no-capture, xClip, no-alphaBlend, largePalette, alphaPixels
static void drawcore_01011(DRAWARGS* args)
{
    uint16_t* buffer = args->buffer;
    const uint32_t* palette = args->palette->colors;
    uint32_t endX   = args->src_x + args->width;
    for (uint32_t iy = args->iy_start; iy < args->iy_end; iy++)
    {
        uint16_t* rowBuffer = buffer;
        const uint8_t* runs = (uint8_t*)args->header + args->rowOffsets[iy];
        uint32_t runCount = ReadVarLen(&runs);
        uint32_t x = 0;
        for (uint32_t ri = 0; ri < runCount; ri++)
        {
            uint32_t pixCount = ReadVarLen(&runs);
            bool is_rle = pixCount & 0x01;
            pixCount >>= 1;
            bool is_alpha = false;
            if (!is_rle)
            {
                is_alpha = pixCount & 0x01;
                pixCount >>= 1;
            }
            uint32_t runStart = x;
            uint32_t runEnd   = x + pixCount;
            x = runEnd;
            if (runEnd <= (uint32_t)args->src_x)
            {
                if (is_rle)
                {
                    ReadVarLen(&runs);
                }
                else
                {
                    SkipVarLen(&runs, pixCount);
                }
                continue;
            }
            if (runStart >= endX)
            {
                break;
            }
            uint32_t drawStart = (runStart < (uint32_t)args->src_x) ? args->src_x : runStart;
            uint32_t drawEnd   = (runEnd   > endX) ? endX  : runEnd;
            pixCount = drawEnd - drawStart;
            if (is_rle)
            {
                uint32_t pi = ReadVarLen(&runs);
                uint32_t pixel = palette[pi];
                uint32_t pixAlpha = pixel & 0xFF000000;
                if (pixAlpha == 0xFF000000)
                {
                    for (uint32_t i=0; i<pixCount; i++)
                    {
                        *rowBuffer++ = pixel;
                    }
                }
                else if (pixAlpha == 0)
                {
                    rowBuffer += pixCount;
                }
                else
                {
                    for (uint32_t i=0; i<pixCount; i++)
                    {
                        *rowBuffer = alpha_blend_premul_rgba8888_over_rgb565(*rowBuffer, pixel);
                        rowBuffer++;
                    }
                }
            }
            else
            {
                SkipVarLen(&runs, drawStart - runStart);
                if (is_alpha)
                {
                    for (uint32_t i=0; i<pixCount; i++)
                    {
                        uint32_t pi = ReadVarLen(&runs);
                        uint32_t pixel = palette[pi];
                        uint32_t pixAlpha = pixel & 0xFF000000;
                        if (pixAlpha == 0xFF000000)
                        {
                            *rowBuffer = pixel;
                        }
                        else if (pixAlpha == 0)
                        {
                        }
                        else
                        {
                            *rowBuffer = alpha_blend_premul_rgba8888_over_rgb565(*rowBuffer, pixel);
                        }
                        rowBuffer++;
                    }
                }
                else
                {
                    for (uint32_t i=0; i<pixCount; i++)
                    {
                        uint32_t pi = ReadVarLen(&runs);
                        uint32_t pixel = palette[pi];
                        *rowBuffer = pixel;
                        rowBuffer++;
                    }
                }
            }
        }
        buffer += args->stride;
    }
}

// no-capture, xClip, alphaBlend, smallPalette, opaquePixels
static void drawcore_01100(DRAWARGS* args)
{
    uint16_t* buffer = args->buffer;
    const uint32_t* palette = args->palette->colors;
    uint32_t endX   = args->src_x + args->width;
    for (uint32_t iy = args->iy_start; iy < args->iy_end; iy++)
    {
        uint16_t* rowBuffer = buffer;
        const uint8_t* runs = (uint8_t*)args->header + args->rowOffsets[iy];
        uint32_t runCount = ReadVarLen(&runs);
        uint32_t x = 0;
        for (uint32_t ri = 0; ri < runCount; ri++)
        {
            uint32_t pixCount = ReadVarLen(&runs);
            bool is_rle = pixCount & 0x01;
            pixCount >>= 1;
            uint32_t runStart = x;
            uint32_t runEnd   = x + pixCount;
            x = runEnd;
            if (runEnd <= (uint32_t)args->src_x)
            {
                if (is_rle)
                {
                    runs++;
                }
                else
                {
                    runs += pixCount;
                }
                continue;
            }
            if (runStart >= endX)
            {
                break;
            }
            uint32_t drawStart = (runStart < (uint32_t)args->src_x) ? args->src_x : runStart;
            uint32_t drawEnd   = (runEnd   > endX) ? endX  : runEnd;
            pixCount = drawEnd - drawStart;
            if (is_rle)
            {
                uint32_t pi = *runs++;
                uint32_t pixel = palette[pi];
                if (pixel != 0)
                {
                    for (uint32_t i=0; i<pixCount; i++)
                    {
                        *rowBuffer = alpha_blend_rgb565_over_rgb565_with_alpha(*rowBuffer, pixel, args->alpha);
                        rowBuffer++;
                    }
                }
                else
                {
                    rowBuffer += pixCount;
                }
            }
            else
            {
                runs += drawStart - runStart;
                for (uint32_t i=0; i<pixCount; i++)
                {
                    uint32_t pi = *runs++;
                    uint32_t pixel = palette[pi];
                    *rowBuffer = alpha_blend_rgb565_over_rgb565_with_alpha(*rowBuffer, pixel, args->alpha);
                    rowBuffer++;
                }
            }
        }
        buffer += args->stride;
    }
}

// no-capture, xClip, alphaBlend, smallPalette, alphaPixels
static void drawcore_01101(DRAWARGS* args)
{
    uint16_t* buffer = args->buffer;
    const uint32_t* palette = args->palette->colors;
    uint32_t endX   = args->src_x + args->width;
    for (uint32_t iy = args->iy_start; iy < args->iy_end; iy++)
    {
        uint16_t* rowBuffer = buffer;
        const uint8_t* runs = (uint8_t*)args->header + args->rowOffsets[iy];
        uint32_t runCount = ReadVarLen(&runs);
        uint32_t x = 0;
        for (uint32_t ri = 0; ri < runCount; ri++)
        {
            uint32_t pixCount = ReadVarLen(&runs);
            bool is_rle = pixCount & 0x01;
            pixCount >>= 1;
            if (!is_rle)
            {
                pixCount >>= 1;
            }
            uint32_t runStart = x;
            uint32_t runEnd   = x + pixCount;
            x = runEnd;
            if (runEnd <= (uint32_t)args->src_x)
            {
                if (is_rle)
                {
                    runs++;
                }
                else
                {
                    runs += pixCount;
                }
                continue;
            }
            if (runStart >= endX)
            {
                break;
            }
            uint32_t drawStart = (runStart < (uint32_t)args->src_x) ? args->src_x : runStart;
            uint32_t drawEnd   = (runEnd   > endX) ? endX  : runEnd;
            pixCount = drawEnd - drawStart;
            if (is_rle)
            {
                uint32_t pi = *runs++;
                uint32_t pixel = palette[pi];
                uint32_t pixAlpha = pixel & 0xFF000000;
                if (pixAlpha == 0)
                {
                    rowBuffer += pixCount;
                }
                else
                {
                    if (pixAlpha == 0xFF000000)
                    {
                        for (uint32_t i=0; i<pixCount; i++)
                        {
                            *rowBuffer = alpha_blend_rgb565_over_rgb565_with_alpha(*rowBuffer, pixel, args->alpha);
                            rowBuffer++;
                        }
                    }
                    else
                    {
                        for (uint32_t i=0; i<pixCount; i++)
                        {
                            *rowBuffer = alpha_blend_premul_rgba8888_over_rgb565_with_alpha(*rowBuffer, pixel, args->alpha);
                            rowBuffer++;
                        }
                    }
                }
            }
            else
            {
                runs += drawStart - runStart;
                for (uint32_t i=0; i<pixCount; i++)
                {
                    uint32_t pi = *runs++;
                    uint32_t pixel = palette[pi];
                    uint32_t pixAlpha = pixel & 0xFF000000;
                    if (pixAlpha == 0)
                    {
                    }
                    else
                    {
                        if (pixAlpha == 0xFF000000)
                        {
                            *rowBuffer = alpha_blend_rgb565_over_rgb565_with_alpha(*rowBuffer, pixel, args->alpha);
                        }
                        else
                        {
                            *rowBuffer = alpha_blend_premul_rgba8888_over_rgb565_with_alpha(*rowBuffer, pixel, args->alpha);
                        }
                    }
                    rowBuffer++;
                }
            }
        }
        buffer += args->stride;
    }
}

// no-capture, xClip, alphaBlend, largePalette, opaquePixels
static void drawcore_01110(DRAWARGS* args)
{
    uint16_t* buffer = args->buffer;
    const uint32_t* palette = args->palette->colors;
    uint32_t endX   = args->src_x + args->width;
    for (uint32_t iy = args->iy_start; iy < args->iy_end; iy++)
    {
        uint16_t* rowBuffer = buffer;
        const uint8_t* runs = (uint8_t*)args->header + args->rowOffsets[iy];
        uint32_t runCount = ReadVarLen(&runs);
        uint32_t x = 0;
        for (uint32_t ri = 0; ri < runCount; ri++)
        {
            uint32_t pixCount = ReadVarLen(&runs);
            bool is_rle = pixCount & 0x01;
            pixCount >>= 1;
            uint32_t runStart = x;
            uint32_t runEnd   = x + pixCount;
            x = runEnd;
            if (runEnd <= (uint32_t)args->src_x)
            {
                if (is_rle)
                {
                    ReadVarLen(&runs);
                }
                else
                {
                    SkipVarLen(&runs, pixCount);
                }
                continue;
            }
            if (runStart >= endX)
            {
                break;
            }
            uint32_t drawStart = (runStart < (uint32_t)args->src_x) ? args->src_x : runStart;
            uint32_t drawEnd   = (runEnd   > endX) ? endX  : runEnd;
            pixCount = drawEnd - drawStart;
            if (is_rle)
            {
                uint32_t pi = ReadVarLen(&runs);
                uint32_t pixel = palette[pi];
                if (pixel != 0)
                {
                    for (uint32_t i=0; i<pixCount; i++)
                    {
                        *rowBuffer = alpha_blend_rgb565_over_rgb565_with_alpha(*rowBuffer, pixel, args->alpha);
                        rowBuffer++;
                    }
                }
                else
                {
                    rowBuffer += pixCount;
                }
            }
            else
            {
                SkipVarLen(&runs, drawStart - runStart);
                for (uint32_t i=0; i<pixCount; i++)
                {
                    uint32_t pi = ReadVarLen(&runs);
                    uint32_t pixel = palette[pi];
                    *rowBuffer = alpha_blend_rgb565_over_rgb565_with_alpha(*rowBuffer, pixel, args->alpha);
                    rowBuffer++;
                }
            }
        }
        buffer += args->stride;
    }
}

// no-capture, xClip, alphaBlend, largePalette, alphaPixels
static void drawcore_01111(DRAWARGS* args)
{
    uint16_t* buffer = args->buffer;
    const uint32_t* palette = args->palette->colors;
    uint32_t endX   = args->src_x + args->width;
    for (uint32_t iy = args->iy_start; iy < args->iy_end; iy++)
    {
        uint16_t* rowBuffer = buffer;
        const uint8_t* runs = (uint8_t*)args->header + args->rowOffsets[iy];
        uint32_t runCount = ReadVarLen(&runs);
        uint32_t x = 0;
        for (uint32_t ri = 0; ri < runCount; ri++)
        {
            uint32_t pixCount = ReadVarLen(&runs);
            bool is_rle = pixCount & 0x01;
            pixCount >>= 1;
            if (!is_rle)
            {
                pixCount >>= 1;
            }
            uint32_t runStart = x;
            uint32_t runEnd   = x + pixCount;
            x = runEnd;
            if (runEnd <= (uint32_t)args->src_x)
            {
                if (is_rle)
                {
                    ReadVarLen(&runs);
                }
                else
                {
                    SkipVarLen(&runs, pixCount);
                }
                continue;
            }
            if (runStart >= endX)
            {
                break;
            }
            uint32_t drawStart = (runStart < (uint32_t)args->src_x) ? args->src_x : runStart;
            uint32_t drawEnd   = (runEnd   > endX) ? endX  : runEnd;
            pixCount = drawEnd - drawStart;
            if (is_rle)
            {
                uint32_t pi = ReadVarLen(&runs);
                uint32_t pixel = palette[pi];
                uint32_t pixAlpha = pixel & 0xFF000000;
                if (pixAlpha == 0)
                {
                    rowBuffer += pixCount;
                }
                else
                {
                    if (pixAlpha == 0xFF000000)
                    {
                        for (uint32_t i=0; i<pixCount; i++)
                        {
                            *rowBuffer = alpha_blend_rgb565_over_rgb565_with_alpha(*rowBuffer, pixel, args->alpha);
                            rowBuffer++;
                        }
                    }
                    else
                    {
                        for (uint32_t i=0; i<pixCount; i++)
                        {
                            *rowBuffer = alpha_blend_premul_rgba8888_over_rgb565_with_alpha(*rowBuffer, pixel, args->alpha);
                            rowBuffer++;
                        }
                    }
                }
            }
            else
            {
                SkipVarLen(&runs, drawStart - runStart);
                for (uint32_t i=0; i<pixCount; i++)
                {
                    uint32_t pi = ReadVarLen(&runs);
                    uint32_t pixel = palette[pi];
                    uint32_t pixAlpha = pixel & 0xFF000000;
                    if (pixAlpha == 0)
                    {
                    }
                    else
                    {
                        if (pixAlpha == 0xFF000000)
                        {
                            *rowBuffer = alpha_blend_rgb565_over_rgb565_with_alpha(*rowBuffer, pixel, args->alpha);
                        }
                        else
                        {
                            *rowBuffer = alpha_blend_premul_rgba8888_over_rgb565_with_alpha(*rowBuffer, pixel, args->alpha);
                        }
                    }
                    rowBuffer++;
                }
            }
        }
        buffer += args->stride;
    }
}

// capture, no-xClip, no-alphaBlend, smallPalette, opaquePixels
static void drawcore_10000(DRAWARGS* args)
{
    uint16_t* buffer = args->buffer;
    const uint32_t* palette = args->palette->colors;
    for (uint32_t iy = args->iy_start; iy < args->iy_end; iy++)
    {
        uint16_t* rowBuffer = buffer;
        const uint8_t* runs = (uint8_t*)args->header + args->rowOffsets[iy];
        uint32_t runCount = ReadVarLen(&runs);
        for (uint32_t ri = 0; ri < runCount; ri++)
        {
            uint32_t pixCount = ReadVarLen(&runs);
            bool is_rle = pixCount & 0x01;
            pixCount >>= 1;
            if (is_rle)
            {
                uint32_t pi = *runs++;
                uint32_t pixel = palette[pi];
                if (pixel != 0)
                {
                    capture_helper(args, rowBuffer, pixCount);
                    for (uint32_t i=0; i<pixCount; i++)
                    {
                        *rowBuffer++ = pixel;
                    }
                }
                else
                {
                    rowBuffer += pixCount;
                }
            }
            else
            {
                capture_helper(args, rowBuffer, pixCount);
                for (uint32_t i=0; i<pixCount; i++)
                {
                    uint32_t pi = *runs++;
                    uint32_t pixel = palette[pi];
                    *rowBuffer = pixel;
                    rowBuffer++;
                }
            }
        }
        buffer += args->stride;
    }
    capture_finalize(args);
}

// capture, no-xClip, no-alphaBlend, smallPalette, alphaPixels
static void drawcore_10001(DRAWARGS* args)
{
    uint16_t* buffer = args->buffer;
    const uint32_t* palette = args->palette->colors;
    for (uint32_t iy = args->iy_start; iy < args->iy_end; iy++)
    {
        uint16_t* rowBuffer = buffer;
        const uint8_t* runs = (uint8_t*)args->header + args->rowOffsets[iy];
        uint32_t runCount = ReadVarLen(&runs);
        for (uint32_t ri = 0; ri < runCount; ri++)
        {
            uint32_t pixCount = ReadVarLen(&runs);
            bool is_rle = pixCount & 0x01;
            pixCount >>= 1;
            bool is_alpha = false;
            if (!is_rle)
            {
                is_alpha = pixCount & 0x01;
                pixCount >>= 1;
            }
            if (is_rle)
            {
                uint32_t pi = *runs++;
                uint32_t pixel = palette[pi];
                uint32_t pixAlpha = pixel & 0xFF000000;
                if (pixAlpha == 0xFF000000)
                {
                    capture_helper(args, rowBuffer, pixCount);
                    for (uint32_t i=0; i<pixCount; i++)
                    {
                        *rowBuffer++ = pixel;
                    }
                }
                else if (pixAlpha == 0)
                {
                    rowBuffer += pixCount;
                }
                else
                {
                    capture_helper(args, rowBuffer, pixCount);
                    for (uint32_t i=0; i<pixCount; i++)
                    {
                        *rowBuffer = alpha_blend_premul_rgba8888_over_rgb565(*rowBuffer, pixel);
                        rowBuffer++;
                    }
                }
            }
            else
            {
                capture_helper(args, rowBuffer, pixCount);
                if (is_alpha)
                {
                    for (uint32_t i=0; i<pixCount; i++)
                    {
                        uint32_t pi = *runs++;
                        uint32_t pixel = palette[pi];
                        uint32_t pixAlpha = pixel & 0xFF000000;
                        if (pixAlpha == 0xFF000000)
                        {
                            *rowBuffer = pixel;
                        }
                        else if (pixAlpha == 0)
                        {
                        }
                        else
                        {
                            *rowBuffer = alpha_blend_premul_rgba8888_over_rgb565(*rowBuffer, pixel);
                        }
                        rowBuffer++;
                    }
                }
                else
                {
                    for (uint32_t i=0; i<pixCount; i++)
                    {
                        uint32_t pi = *runs++;
                        uint32_t pixel = palette[pi];
                        *rowBuffer = pixel;
                        rowBuffer++;
                    }
                }
            }
        }
        buffer += args->stride;
    }
    capture_finalize(args);
}

// capture, no-xClip, no-alphaBlend, largePalette, opaquePixels
static void drawcore_10010(DRAWARGS* args)
{
    uint16_t* buffer = args->buffer;
    const uint32_t* palette = args->palette->colors;
    for (uint32_t iy = args->iy_start; iy < args->iy_end; iy++)
    {
        uint16_t* rowBuffer = buffer;
        const uint8_t* runs = (uint8_t*)args->header + args->rowOffsets[iy];
        uint32_t runCount = ReadVarLen(&runs);
        for (uint32_t ri = 0; ri < runCount; ri++)
        {
            uint32_t pixCount = ReadVarLen(&runs);
            bool is_rle = pixCount & 0x01;
            pixCount >>= 1;
            if (is_rle)
            {
                uint32_t pi = ReadVarLen(&runs);
                uint32_t pixel = palette[pi];
                if (pixel != 0)
                {
                    capture_helper(args, rowBuffer, pixCount);
                    for (uint32_t i=0; i<pixCount; i++)
                    {
                        *rowBuffer++ = pixel;
                    }
                }
                else
                {
                    rowBuffer += pixCount;
                }
            }
            else
            {
                capture_helper(args, rowBuffer, pixCount);
                for (uint32_t i=0; i<pixCount; i++)
                {
                    uint32_t pi = ReadVarLen(&runs);
                    uint32_t pixel = palette[pi];
                    *rowBuffer = pixel;
                    rowBuffer++;
                }
            }
        }
        buffer += args->stride;
    }
    capture_finalize(args);
}

// capture, no-xClip, no-alphaBlend, largePalette, alphaPixels
static void drawcore_10011(DRAWARGS* args)
{
    uint16_t* buffer = args->buffer;
    const uint32_t* palette = args->palette->colors;
    for (uint32_t iy = args->iy_start; iy < args->iy_end; iy++)
    {
        uint16_t* rowBuffer = buffer;
        const uint8_t* runs = (uint8_t*)args->header + args->rowOffsets[iy];
        uint32_t runCount = ReadVarLen(&runs);
        for (uint32_t ri = 0; ri < runCount; ri++)
        {
            uint32_t pixCount = ReadVarLen(&runs);
            bool is_rle = pixCount & 0x01;
            pixCount >>= 1;
            bool is_alpha = false;
            if (!is_rle)
            {
                is_alpha = pixCount & 0x01;
                pixCount >>= 1;
            }
            if (is_rle)
            {
                uint32_t pi = ReadVarLen(&runs);
                uint32_t pixel = palette[pi];
                uint32_t pixAlpha = pixel & 0xFF000000;
                if (pixAlpha == 0xFF000000)
                {
                    capture_helper(args, rowBuffer, pixCount);
                    for (uint32_t i=0; i<pixCount; i++)
                    {
                        *rowBuffer++ = pixel;
                    }
                }
                else if (pixAlpha == 0)
                {
                    rowBuffer += pixCount;
                }
                else
                {
                    capture_helper(args, rowBuffer, pixCount);
                    for (uint32_t i=0; i<pixCount; i++)
                    {
                        *rowBuffer = alpha_blend_premul_rgba8888_over_rgb565(*rowBuffer, pixel);
                        rowBuffer++;
                    }
                }
            }
            else
            {
                capture_helper(args, rowBuffer, pixCount);
                if (is_alpha)
                {
                    for (uint32_t i=0; i<pixCount; i++)
                    {
                        uint32_t pi = ReadVarLen(&runs);
                        uint32_t pixel = palette[pi];
                        uint32_t pixAlpha = pixel & 0xFF000000;
                        if (pixAlpha == 0xFF000000)
                        {
                            *rowBuffer = pixel;
                        }
                        else if (pixAlpha == 0)
                        {
                        }
                        else
                        {
                            *rowBuffer = alpha_blend_premul_rgba8888_over_rgb565(*rowBuffer, pixel);
                        }
                        rowBuffer++;
                    }
                }
                else
                {
                    for (uint32_t i=0; i<pixCount; i++)
                    {
                        uint32_t pi = ReadVarLen(&runs);
                        uint32_t pixel = palette[pi];
                        *rowBuffer = pixel;
                        rowBuffer++;
                    }
                }
            }
        }
        buffer += args->stride;
    }
    capture_finalize(args);
}

// capture, no-xClip, alphaBlend, smallPalette, opaquePixels
static void drawcore_10100(DRAWARGS* args)
{
    uint16_t* buffer = args->buffer;
    const uint32_t* palette = args->palette->colors;
    for (uint32_t iy = args->iy_start; iy < args->iy_end; iy++)
    {
        uint16_t* rowBuffer = buffer;
        const uint8_t* runs = (uint8_t*)args->header + args->rowOffsets[iy];
        uint32_t runCount = ReadVarLen(&runs);
        for (uint32_t ri = 0; ri < runCount; ri++)
        {
            uint32_t pixCount = ReadVarLen(&runs);
            bool is_rle = pixCount & 0x01;
            pixCount >>= 1;
            if (is_rle)
            {
                uint32_t pi = *runs++;
                uint32_t pixel = palette[pi];
                if (pixel != 0)
                {
                    capture_helper(args, rowBuffer, pixCount);
                    for (uint32_t i=0; i<pixCount; i++)
                    {
                        *rowBuffer = alpha_blend_rgb565_over_rgb565_with_alpha(*rowBuffer, pixel, args->alpha);
                        rowBuffer++;
                    }
                }
                else
                {
                    rowBuffer += pixCount;
                }
            }
            else
            {
                capture_helper(args, rowBuffer, pixCount);
                for (uint32_t i=0; i<pixCount; i++)
                {
                    uint32_t pi = *runs++;
                    uint32_t pixel = palette[pi];
                    *rowBuffer = alpha_blend_rgb565_over_rgb565_with_alpha(*rowBuffer, pixel, args->alpha);
                    rowBuffer++;
                }
            }
        }
        buffer += args->stride;
    }
    capture_finalize(args);
}

// capture, no-xClip, alphaBlend, smallPalette, alphaPixels
static void drawcore_10101(DRAWARGS* args)
{
    uint16_t* buffer = args->buffer;
    const uint32_t* palette = args->palette->colors;
    for (uint32_t iy = args->iy_start; iy < args->iy_end; iy++)
    {
        uint16_t* rowBuffer = buffer;
        const uint8_t* runs = (uint8_t*)args->header + args->rowOffsets[iy];
        uint32_t runCount = ReadVarLen(&runs);
        for (uint32_t ri = 0; ri < runCount; ri++)
        {
            uint32_t pixCount = ReadVarLen(&runs);
            bool is_rle = pixCount & 0x01;
            pixCount >>= 1;
            if (!is_rle)
            {
                pixCount >>= 1;
            }
            if (is_rle)
            {
                uint32_t pi = *runs++;
                uint32_t pixel = palette[pi];
                uint32_t pixAlpha = pixel & 0xFF000000;
                if (pixAlpha == 0)
                {
                    rowBuffer += pixCount;
                }
                else
                {
                    capture_helper(args, rowBuffer, pixCount);
                    if (pixAlpha == 0xFF000000)
                    {
                        for (uint32_t i=0; i<pixCount; i++)
                        {
                            *rowBuffer = alpha_blend_rgb565_over_rgb565_with_alpha(*rowBuffer, pixel, args->alpha);
                            rowBuffer++;
                        }
                    }
                    else
                    {
                        for (uint32_t i=0; i<pixCount; i++)
                        {
                            *rowBuffer = alpha_blend_premul_rgba8888_over_rgb565_with_alpha(*rowBuffer, pixel, args->alpha);
                            rowBuffer++;
                        }
                    }
                }
            }
            else
            {
                capture_helper(args, rowBuffer, pixCount);
                for (uint32_t i=0; i<pixCount; i++)
                {
                    uint32_t pi = *runs++;
                    uint32_t pixel = palette[pi];
                    uint32_t pixAlpha = pixel & 0xFF000000;
                    if (pixAlpha == 0)
                    {
                    }
                    else
                    {
                        if (pixAlpha == 0xFF000000)
                        {
                            *rowBuffer = alpha_blend_rgb565_over_rgb565_with_alpha(*rowBuffer, pixel, args->alpha);
                        }
                        else
                        {
                            *rowBuffer = alpha_blend_premul_rgba8888_over_rgb565_with_alpha(*rowBuffer, pixel, args->alpha);
                        }
                    }
                    rowBuffer++;
                }
            }
        }
        buffer += args->stride;
    }
    capture_finalize(args);
}

// capture, no-xClip, alphaBlend, largePalette, opaquePixels
static void drawcore_10110(DRAWARGS* args)
{
    uint16_t* buffer = args->buffer;
    const uint32_t* palette = args->palette->colors;
    for (uint32_t iy = args->iy_start; iy < args->iy_end; iy++)
    {
        uint16_t* rowBuffer = buffer;
        const uint8_t* runs = (uint8_t*)args->header + args->rowOffsets[iy];
        uint32_t runCount = ReadVarLen(&runs);
        for (uint32_t ri = 0; ri < runCount; ri++)
        {
            uint32_t pixCount = ReadVarLen(&runs);
            bool is_rle = pixCount & 0x01;
            pixCount >>= 1;
            if (is_rle)
            {
                uint32_t pi = ReadVarLen(&runs);
                uint32_t pixel = palette[pi];
                if (pixel != 0)
                {
                    capture_helper(args, rowBuffer, pixCount);
                    for (uint32_t i=0; i<pixCount; i++)
                    {
                        *rowBuffer = alpha_blend_rgb565_over_rgb565_with_alpha(*rowBuffer, pixel, args->alpha);
                        rowBuffer++;
                    }
                }
                else
                {
                    rowBuffer += pixCount;
                }
            }
            else
            {
                capture_helper(args, rowBuffer, pixCount);
                for (uint32_t i=0; i<pixCount; i++)
                {
                    uint32_t pi = ReadVarLen(&runs);
                    uint32_t pixel = palette[pi];
                    *rowBuffer = alpha_blend_rgb565_over_rgb565_with_alpha(*rowBuffer, pixel, args->alpha);
                    rowBuffer++;
                }
            }
        }
        buffer += args->stride;
    }
    capture_finalize(args);
}

// capture, no-xClip, alphaBlend, largePalette, alphaPixels
static void drawcore_10111(DRAWARGS* args)
{
    uint16_t* buffer = args->buffer;
    const uint32_t* palette = args->palette->colors;
    for (uint32_t iy = args->iy_start; iy < args->iy_end; iy++)
    {
        uint16_t* rowBuffer = buffer;
        const uint8_t* runs = (uint8_t*)args->header + args->rowOffsets[iy];
        uint32_t runCount = ReadVarLen(&runs);
        for (uint32_t ri = 0; ri < runCount; ri++)
        {
            uint32_t pixCount = ReadVarLen(&runs);
            bool is_rle = pixCount & 0x01;
            pixCount >>= 1;
            if (!is_rle)
            {
                pixCount >>= 1;
            }
            if (is_rle)
            {
                uint32_t pi = ReadVarLen(&runs);
                uint32_t pixel = palette[pi];
                uint32_t pixAlpha = pixel & 0xFF000000;
                if (pixAlpha == 0)
                {
                    rowBuffer += pixCount;
                }
                else
                {
                    capture_helper(args, rowBuffer, pixCount);
                    if (pixAlpha == 0xFF000000)
                    {
                        for (uint32_t i=0; i<pixCount; i++)
                        {
                            *rowBuffer = alpha_blend_rgb565_over_rgb565_with_alpha(*rowBuffer, pixel, args->alpha);
                            rowBuffer++;
                        }
                    }
                    else
                    {
                        for (uint32_t i=0; i<pixCount; i++)
                        {
                            *rowBuffer = alpha_blend_premul_rgba8888_over_rgb565_with_alpha(*rowBuffer, pixel, args->alpha);
                            rowBuffer++;
                        }
                    }
                }
            }
            else
            {
                capture_helper(args, rowBuffer, pixCount);
                for (uint32_t i=0; i<pixCount; i++)
                {
                    uint32_t pi = ReadVarLen(&runs);
                    uint32_t pixel = palette[pi];
                    uint32_t pixAlpha = pixel & 0xFF000000;
                    if (pixAlpha == 0)
                    {
                    }
                    else
                    {
                        if (pixAlpha == 0xFF000000)
                        {
                            *rowBuffer = alpha_blend_rgb565_over_rgb565_with_alpha(*rowBuffer, pixel, args->alpha);
                        }
                        else
                        {
                            *rowBuffer = alpha_blend_premul_rgba8888_over_rgb565_with_alpha(*rowBuffer, pixel, args->alpha);
                        }
                    }
                    rowBuffer++;
                }
            }
        }
        buffer += args->stride;
    }
    capture_finalize(args);
}

// capture, xClip, no-alphaBlend, smallPalette, opaquePixels
static void drawcore_11000(DRAWARGS* args)
{
    uint16_t* buffer = args->buffer;
    const uint32_t* palette = args->palette->colors;
    uint32_t endX   = args->src_x + args->width;
    for (uint32_t iy = args->iy_start; iy < args->iy_end; iy++)
    {
        uint16_t* rowBuffer = buffer;
        const uint8_t* runs = (uint8_t*)args->header + args->rowOffsets[iy];
        uint32_t runCount = ReadVarLen(&runs);
        uint32_t x = 0;
        for (uint32_t ri = 0; ri < runCount; ri++)
        {
            uint32_t pixCount = ReadVarLen(&runs);
            bool is_rle = pixCount & 0x01;
            pixCount >>= 1;
            uint32_t runStart = x;
            uint32_t runEnd   = x + pixCount;
            x = runEnd;
            if (runEnd <= (uint32_t)args->src_x)
            {
                if (is_rle)
                {
                    runs++;
                }
                else
                {
                    runs += pixCount;
                }
                continue;
            }
            if (runStart >= endX)
            {
                break;
            }
            uint32_t drawStart = (runStart < (uint32_t)args->src_x) ? args->src_x : runStart;
            uint32_t drawEnd   = (runEnd   > endX) ? endX  : runEnd;
            pixCount = drawEnd - drawStart;
            if (is_rle)
            {
                uint32_t pi = *runs++;
                uint32_t pixel = palette[pi];
                if (pixel != 0)
                {
                    capture_helper(args, rowBuffer, pixCount);
                    for (uint32_t i=0; i<pixCount; i++)
                    {
                        *rowBuffer++ = pixel;
                    }
                }
                else
                {
                    rowBuffer += pixCount;
                }
            }
            else
            {
                runs += drawStart - runStart;
                capture_helper(args, rowBuffer, pixCount);
                for (uint32_t i=0; i<pixCount; i++)
                {
                    uint32_t pi = *runs++;
                    uint32_t pixel = palette[pi];
                    *rowBuffer = pixel;
                    rowBuffer++;
                }
            }
        }
        buffer += args->stride;
    }
    capture_finalize(args);
}

// capture, xClip, no-alphaBlend, smallPalette, alphaPixels
static void drawcore_11001(DRAWARGS* args)
{
    uint16_t* buffer = args->buffer;
    const uint32_t* palette = args->palette->colors;
    uint32_t endX   = args->src_x + args->width;
    for (uint32_t iy = args->iy_start; iy < args->iy_end; iy++)
    {
        uint16_t* rowBuffer = buffer;
        const uint8_t* runs = (uint8_t*)args->header + args->rowOffsets[iy];
        uint32_t runCount = ReadVarLen(&runs);
        uint32_t x = 0;
        for (uint32_t ri = 0; ri < runCount; ri++)
        {
            uint32_t pixCount = ReadVarLen(&runs);
            bool is_rle = pixCount & 0x01;
            pixCount >>= 1;
            bool is_alpha = false;
            if (!is_rle)
            {
                is_alpha = pixCount & 0x01;
                pixCount >>= 1;
            }
            uint32_t runStart = x;
            uint32_t runEnd   = x + pixCount;
            x = runEnd;
            if (runEnd <= (uint32_t)args->src_x)
            {
                if (is_rle)
                {
                    runs++;
                }
                else
                {
                    runs += pixCount;
                }
                continue;
            }
            if (runStart >= endX)
            {
                break;
            }
            uint32_t drawStart = (runStart < (uint32_t)args->src_x) ? args->src_x : runStart;
            uint32_t drawEnd   = (runEnd   > endX) ? endX  : runEnd;
            pixCount = drawEnd - drawStart;
            if (is_rle)
            {
                uint32_t pi = *runs++;
                uint32_t pixel = palette[pi];
                uint32_t pixAlpha = pixel & 0xFF000000;
                if (pixAlpha == 0xFF000000)
                {
                    capture_helper(args, rowBuffer, pixCount);
                    for (uint32_t i=0; i<pixCount; i++)
                    {
                        *rowBuffer++ = pixel;
                    }
                }
                else if (pixAlpha == 0)
                {
                    rowBuffer += pixCount;
                }
                else
                {
                    capture_helper(args, rowBuffer, pixCount);
                    for (uint32_t i=0; i<pixCount; i++)
                    {
                        *rowBuffer = alpha_blend_premul_rgba8888_over_rgb565(*rowBuffer, pixel);
                        rowBuffer++;
                    }
                }
            }
            else
            {
                runs += drawStart - runStart;
                capture_helper(args, rowBuffer, pixCount);
                if (is_alpha)
                {
                    for (uint32_t i=0; i<pixCount; i++)
                    {
                        uint32_t pi = *runs++;
                        uint32_t pixel = palette[pi];
                        uint32_t pixAlpha = pixel & 0xFF000000;
                        if (pixAlpha == 0xFF000000)
                        {
                            *rowBuffer = pixel;
                        }
                        else if (pixAlpha == 0)
                        {
                        }
                        else
                        {
                            *rowBuffer = alpha_blend_premul_rgba8888_over_rgb565(*rowBuffer, pixel);
                        }
                        rowBuffer++;
                    }
                }
                else
                {
                    for (uint32_t i=0; i<pixCount; i++)
                    {
                        uint32_t pi = *runs++;
                        uint32_t pixel = palette[pi];
                        *rowBuffer = pixel;
                        rowBuffer++;
                    }
                }
            }
        }
        buffer += args->stride;
    }
    capture_finalize(args);
}

// capture, xClip, no-alphaBlend, largePalette, opaquePixels
static void drawcore_11010(DRAWARGS* args)
{
    uint16_t* buffer = args->buffer;
    const uint32_t* palette = args->palette->colors;
    uint32_t endX   = args->src_x + args->width;
    for (uint32_t iy = args->iy_start; iy < args->iy_end; iy++)
    {
        uint16_t* rowBuffer = buffer;
        const uint8_t* runs = (uint8_t*)args->header + args->rowOffsets[iy];
        uint32_t runCount = ReadVarLen(&runs);
        uint32_t x = 0;
        for (uint32_t ri = 0; ri < runCount; ri++)
        {
            uint32_t pixCount = ReadVarLen(&runs);
            bool is_rle = pixCount & 0x01;
            pixCount >>= 1;
            uint32_t runStart = x;
            uint32_t runEnd   = x + pixCount;
            x = runEnd;
            if (runEnd <= (uint32_t)args->src_x)
            {
                if (is_rle)
                {
                    ReadVarLen(&runs);
                }
                else
                {
                    SkipVarLen(&runs, pixCount);
                }
                continue;
            }
            if (runStart >= endX)
            {
                break;
            }
            uint32_t drawStart = (runStart < (uint32_t)args->src_x) ? args->src_x : runStart;
            uint32_t drawEnd   = (runEnd   > endX) ? endX  : runEnd;
            pixCount = drawEnd - drawStart;
            if (is_rle)
            {
                uint32_t pi = ReadVarLen(&runs);
                uint32_t pixel = palette[pi];
                if (pixel != 0)
                {
                    capture_helper(args, rowBuffer, pixCount);
                    for (uint32_t i=0; i<pixCount; i++)
                    {
                        *rowBuffer++ = pixel;
                    }
                }
                else
                {
                    rowBuffer += pixCount;
                }
            }
            else
            {
                SkipVarLen(&runs, drawStart - runStart);
                capture_helper(args, rowBuffer, pixCount);
                for (uint32_t i=0; i<pixCount; i++)
                {
                    uint32_t pi = ReadVarLen(&runs);
                    uint32_t pixel = palette[pi];
                    *rowBuffer = pixel;
                    rowBuffer++;
                }
            }
        }
        buffer += args->stride;
    }
    capture_finalize(args);
}

// capture, xClip, no-alphaBlend, largePalette, alphaPixels
static void drawcore_11011(DRAWARGS* args)
{
    uint16_t* buffer = args->buffer;
    const uint32_t* palette = args->palette->colors;
    uint32_t endX   = args->src_x + args->width;
    for (uint32_t iy = args->iy_start; iy < args->iy_end; iy++)
    {
        uint16_t* rowBuffer = buffer;
        const uint8_t* runs = (uint8_t*)args->header + args->rowOffsets[iy];
        uint32_t runCount = ReadVarLen(&runs);
        uint32_t x = 0;
        for (uint32_t ri = 0; ri < runCount; ri++)
        {
            uint32_t pixCount = ReadVarLen(&runs);
            bool is_rle = pixCount & 0x01;
            pixCount >>= 1;
            bool is_alpha = false;
            if (!is_rle)
            {
                is_alpha = pixCount & 0x01;
                pixCount >>= 1;
            }
            uint32_t runStart = x;
            uint32_t runEnd   = x + pixCount;
            x = runEnd;
            if (runEnd <= (uint32_t)args->src_x)
            {
                if (is_rle)
                {
                    ReadVarLen(&runs);
                }
                else
                {
                    SkipVarLen(&runs, pixCount);
                }
                continue;
            }
            if (runStart >= endX)
            {
                break;
            }
            uint32_t drawStart = (runStart < (uint32_t)args->src_x) ? args->src_x : runStart;
            uint32_t drawEnd   = (runEnd   > endX) ? endX  : runEnd;
            pixCount = drawEnd - drawStart;
            if (is_rle)
            {
                uint32_t pi = ReadVarLen(&runs);
                uint32_t pixel = palette[pi];
                uint32_t pixAlpha = pixel & 0xFF000000;
                if (pixAlpha == 0xFF000000)
                {
                    capture_helper(args, rowBuffer, pixCount);
                    for (uint32_t i=0; i<pixCount; i++)
                    {
                        *rowBuffer++ = pixel;
                    }
                }
                else if (pixAlpha == 0)
                {
                    rowBuffer += pixCount;
                }
                else
                {
                    capture_helper(args, rowBuffer, pixCount);
                    for (uint32_t i=0; i<pixCount; i++)
                    {
                        *rowBuffer = alpha_blend_premul_rgba8888_over_rgb565(*rowBuffer, pixel);
                        rowBuffer++;
                    }
                }
            }
            else
            {
                SkipVarLen(&runs, drawStart - runStart);
                capture_helper(args, rowBuffer, pixCount);
                if (is_alpha)
                {
                    for (uint32_t i=0; i<pixCount; i++)
                    {
                        uint32_t pi = ReadVarLen(&runs);
                        uint32_t pixel = palette[pi];
                        uint32_t pixAlpha = pixel & 0xFF000000;
                        if (pixAlpha == 0xFF000000)
                        {
                            *rowBuffer = pixel;
                        }
                        else if (pixAlpha == 0)
                        {
                        }
                        else
                        {
                            *rowBuffer = alpha_blend_premul_rgba8888_over_rgb565(*rowBuffer, pixel);
                        }
                        rowBuffer++;
                    }
                }
                else
                {
                    for (uint32_t i=0; i<pixCount; i++)
                    {
                        uint32_t pi = ReadVarLen(&runs);
                        uint32_t pixel = palette[pi];
                        *rowBuffer = pixel;
                        rowBuffer++;
                    }
                }
            }
        }
        buffer += args->stride;
    }
    capture_finalize(args);
}

// capture, xClip, alphaBlend, smallPalette, opaquePixels
static void drawcore_11100(DRAWARGS* args)
{
    uint16_t* buffer = args->buffer;
    const uint32_t* palette = args->palette->colors;
    uint32_t endX   = args->src_x + args->width;
    for (uint32_t iy = args->iy_start; iy < args->iy_end; iy++)
    {
        uint16_t* rowBuffer = buffer;
        const uint8_t* runs = (uint8_t*)args->header + args->rowOffsets[iy];
        uint32_t runCount = ReadVarLen(&runs);
        uint32_t x = 0;
        for (uint32_t ri = 0; ri < runCount; ri++)
        {
            uint32_t pixCount = ReadVarLen(&runs);
            bool is_rle = pixCount & 0x01;
            pixCount >>= 1;
            uint32_t runStart = x;
            uint32_t runEnd   = x + pixCount;
            x = runEnd;
            if (runEnd <= (uint32_t)args->src_x)
            {
                if (is_rle)
                {
                    runs++;
                }
                else
                {
                    runs += pixCount;
                }
                continue;
            }
            if (runStart >= endX)
            {
                break;
            }
            uint32_t drawStart = (runStart < (uint32_t)args->src_x) ? args->src_x : runStart;
            uint32_t drawEnd   = (runEnd   > endX) ? endX  : runEnd;
            pixCount = drawEnd - drawStart;
            if (is_rle)
            {
                uint32_t pi = *runs++;
                uint32_t pixel = palette[pi];
                if (pixel != 0)
                {
                    capture_helper(args, rowBuffer, pixCount);
                    for (uint32_t i=0; i<pixCount; i++)
                    {
                        *rowBuffer = alpha_blend_rgb565_over_rgb565_with_alpha(*rowBuffer, pixel, args->alpha);
                        rowBuffer++;
                    }
                }
                else
                {
                    rowBuffer += pixCount;
                }
            }
            else
            {
                runs += drawStart - runStart;
                capture_helper(args, rowBuffer, pixCount);
                for (uint32_t i=0; i<pixCount; i++)
                {
                    uint32_t pi = *runs++;
                    uint32_t pixel = palette[pi];
                    *rowBuffer = alpha_blend_rgb565_over_rgb565_with_alpha(*rowBuffer, pixel, args->alpha);
                    rowBuffer++;
                }
            }
        }
        buffer += args->stride;
    }
    capture_finalize(args);
}

// capture, xClip, alphaBlend, smallPalette, alphaPixels
static void drawcore_11101(DRAWARGS* args)
{
    uint16_t* buffer = args->buffer;
    const uint32_t* palette = args->palette->colors;
    uint32_t endX   = args->src_x + args->width;
    for (uint32_t iy = args->iy_start; iy < args->iy_end; iy++)
    {
        uint16_t* rowBuffer = buffer;
        const uint8_t* runs = (uint8_t*)args->header + args->rowOffsets[iy];
        uint32_t runCount = ReadVarLen(&runs);
        uint32_t x = 0;
        for (uint32_t ri = 0; ri < runCount; ri++)
        {
            uint32_t pixCount = ReadVarLen(&runs);
            bool is_rle = pixCount & 0x01;
            pixCount >>= 1;
            if (!is_rle)
            {
                pixCount >>= 1;
            }
            uint32_t runStart = x;
            uint32_t runEnd   = x + pixCount;
            x = runEnd;
            if (runEnd <= (uint32_t)args->src_x)
            {
                if (is_rle)
                {
                    runs++;
                }
                else
                {
                    runs += pixCount;
                }
                continue;
            }
            if (runStart >= endX)
            {
                break;
            }
            uint32_t drawStart = (runStart < (uint32_t)args->src_x) ? args->src_x : runStart;
            uint32_t drawEnd   = (runEnd   > endX) ? endX  : runEnd;
            pixCount = drawEnd - drawStart;
            if (is_rle)
            {
                uint32_t pi = *runs++;
                uint32_t pixel = palette[pi];
                uint32_t pixAlpha = pixel & 0xFF000000;
                if (pixAlpha == 0)
                {
                    rowBuffer += pixCount;
                }
                else
                {
                    capture_helper(args, rowBuffer, pixCount);
                    if (pixAlpha == 0xFF000000)
                    {
                        for (uint32_t i=0; i<pixCount; i++)
                        {
                            *rowBuffer = alpha_blend_rgb565_over_rgb565_with_alpha(*rowBuffer, pixel, args->alpha);
                            rowBuffer++;
                        }
                    }
                    else
                    {
                        for (uint32_t i=0; i<pixCount; i++)
                        {
                            *rowBuffer = alpha_blend_premul_rgba8888_over_rgb565_with_alpha(*rowBuffer, pixel, args->alpha);
                            rowBuffer++;
                        }
                    }
                }
            }
            else
            {
                runs += drawStart - runStart;
                capture_helper(args, rowBuffer, pixCount);
                for (uint32_t i=0; i<pixCount; i++)
                {
                    uint32_t pi = *runs++;
                    uint32_t pixel = palette[pi];
                    uint32_t pixAlpha = pixel & 0xFF000000;
                    if (pixAlpha == 0)
                    {
                    }
                    else
                    {
                        if (pixAlpha == 0xFF000000)
                        {
                            *rowBuffer = alpha_blend_rgb565_over_rgb565_with_alpha(*rowBuffer, pixel, args->alpha);
                        }
                        else
                        {
                            *rowBuffer = alpha_blend_premul_rgba8888_over_rgb565_with_alpha(*rowBuffer, pixel, args->alpha);
                        }
                    }
                    rowBuffer++;
                }
            }
        }
        buffer += args->stride;
    }
    capture_finalize(args);
}

// capture, xClip, alphaBlend, largePalette, opaquePixels
static void drawcore_11110(DRAWARGS* args)
{
    uint16_t* buffer = args->buffer;
    const uint32_t* palette = args->palette->colors;
    uint32_t endX   = args->src_x + args->width;
    for (uint32_t iy = args->iy_start; iy < args->iy_end; iy++)
    {
        uint16_t* rowBuffer = buffer;
        const uint8_t* runs = (uint8_t*)args->header + args->rowOffsets[iy];
        uint32_t runCount = ReadVarLen(&runs);
        uint32_t x = 0;
        for (uint32_t ri = 0; ri < runCount; ri++)
        {
            uint32_t pixCount = ReadVarLen(&runs);
            bool is_rle = pixCount & 0x01;
            pixCount >>= 1;
            uint32_t runStart = x;
            uint32_t runEnd   = x + pixCount;
            x = runEnd;
            if (runEnd <= (uint32_t)args->src_x)
            {
                if (is_rle)
                {
                    ReadVarLen(&runs);
                }
                else
                {
                    SkipVarLen(&runs, pixCount);
                }
                continue;
            }
            if (runStart >= endX)
            {
                break;
            }
            uint32_t drawStart = (runStart < (uint32_t)args->src_x) ? args->src_x : runStart;
            uint32_t drawEnd   = (runEnd   > endX) ? endX  : runEnd;
            pixCount = drawEnd - drawStart;
            if (is_rle)
            {
                uint32_t pi = ReadVarLen(&runs);
                uint32_t pixel = palette[pi];
                if (pixel != 0)
                {
                    capture_helper(args, rowBuffer, pixCount);
                    for (uint32_t i=0; i<pixCount; i++)
                    {
                        *rowBuffer = alpha_blend_rgb565_over_rgb565_with_alpha(*rowBuffer, pixel, args->alpha);
                        rowBuffer++;
                    }
                }
                else
                {
                    rowBuffer += pixCount;
                }
            }
            else
            {
                SkipVarLen(&runs, drawStart - runStart);
                capture_helper(args, rowBuffer, pixCount);
                for (uint32_t i=0; i<pixCount; i++)
                {
                    uint32_t pi = ReadVarLen(&runs);
                    uint32_t pixel = palette[pi];
                    *rowBuffer = alpha_blend_rgb565_over_rgb565_with_alpha(*rowBuffer, pixel, args->alpha);
                    rowBuffer++;
                }
            }
        }
        buffer += args->stride;
    }
    capture_finalize(args);
}

// capture, xClip, alphaBlend, largePalette, alphaPixels
static void drawcore_11111(DRAWARGS* args)
{
    uint16_t* buffer = args->buffer;
    const uint32_t* palette = args->palette->colors;
    uint32_t endX   = args->src_x + args->width;
    for (uint32_t iy = args->iy_start; iy < args->iy_end; iy++)
    {
        uint16_t* rowBuffer = buffer;
        const uint8_t* runs = (uint8_t*)args->header + args->rowOffsets[iy];
        uint32_t runCount = ReadVarLen(&runs);
        uint32_t x = 0;
        for (uint32_t ri = 0; ri < runCount; ri++)
        {
            uint32_t pixCount = ReadVarLen(&runs);
            bool is_rle = pixCount & 0x01;
            pixCount >>= 1;
            if (!is_rle)
            {
                pixCount >>= 1;
            }
            uint32_t runStart = x;
            uint32_t runEnd   = x + pixCount;
            x = runEnd;
            if (runEnd <= (uint32_t)args->src_x)
            {
                if (is_rle)
                {
                    ReadVarLen(&runs);
                }
                else
                {
                    SkipVarLen(&runs, pixCount);
                }
                continue;
            }
            if (runStart >= endX)
            {
                break;
            }
            uint32_t drawStart = (runStart < (uint32_t)args->src_x) ? args->src_x : runStart;
            uint32_t drawEnd   = (runEnd   > endX) ? endX  : runEnd;
            pixCount = drawEnd - drawStart;
            if (is_rle)
            {
                uint32_t pi = ReadVarLen(&runs);
                uint32_t pixel = palette[pi];
                uint32_t pixAlpha = pixel & 0xFF000000;
                if (pixAlpha == 0)
                {
                    rowBuffer += pixCount;
                }
                else
                {
                    capture_helper(args, rowBuffer, pixCount);
                    if (pixAlpha == 0xFF000000)
                    {
                        for (uint32_t i=0; i<pixCount; i++)
                        {
                            *rowBuffer = alpha_blend_rgb565_over_rgb565_with_alpha(*rowBuffer, pixel, args->alpha);
                            rowBuffer++;
                        }
                    }
                    else
                    {
                        for (uint32_t i=0; i<pixCount; i++)
                        {
                            *rowBuffer = alpha_blend_premul_rgba8888_over_rgb565_with_alpha(*rowBuffer, pixel, args->alpha);
                            rowBuffer++;
                        }
                    }
                }
            }
            else
            {
                SkipVarLen(&runs, drawStart - runStart);
                capture_helper(args, rowBuffer, pixCount);
                for (uint32_t i=0; i<pixCount; i++)
                {
                    uint32_t pi = ReadVarLen(&runs);
                    uint32_t pixel = palette[pi];
                    uint32_t pixAlpha = pixel & 0xFF000000;
                    if (pixAlpha == 0)
                    {
                    }
                    else
                    {
                        if (pixAlpha == 0xFF000000)
                        {
                            *rowBuffer = alpha_blend_rgb565_over_rgb565_with_alpha(*rowBuffer, pixel, args->alpha);
                        }
                        else
                        {
                            *rowBuffer = alpha_blend_premul_rgba8888_over_rgb565_with_alpha(*rowBuffer, pixel, args->alpha);
                        }
                    }
                    rowBuffer++;
                }
            }
        }
        buffer += args->stride;
    }
    capture_finalize(args);
}



static PFNDRAWCORE drawcores[] = {
    drawcore_00000,
    drawcore_00001,
    drawcore_00010,
    drawcore_00011,
    drawcore_00100,
    drawcore_00101,
    drawcore_00110,
    drawcore_00111,
    drawcore_01000,
    drawcore_01001,
    drawcore_01010,
    drawcore_01011,
    drawcore_01100,
    drawcore_01101,
    drawcore_01110,
    drawcore_01111,
    drawcore_10000,
    drawcore_10001,
    drawcore_10010,
    drawcore_10011,
    drawcore_10100,
    drawcore_10101,
    drawcore_10110,
    drawcore_10111,
    drawcore_11000,
    drawcore_11001,
    drawcore_11010,
    drawcore_11011,
    drawcore_11100,
    drawcore_11101,
    drawcore_11110,
    drawcore_11111,
};
