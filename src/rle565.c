#include <assert.h>
#include <string.h>
#include <assert.h>

#include "rle565.h"

#if (!defined(__STDC_VERSION__) || __STDC_VERSION__ < 202311L)
    #include <stddef.h>
    #define nullptr ((void*)0)
    typedef void* nullptr_t;
#endif


// Alpha blend a premultiplied RGBA8888 pixel over a RGB565 background
uint16_t alpha_blend_premul_rgba8888_over_rgb565(uint16_t bg, uint32_t fg) 
{
    uint32_t alpha = fg >> 24;
    if (alpha == 0)
        return bg;
    uint32_t inv_a = 255 - alpha;

    uint32_t fg_r = (fg >> 16) & 0xFF;
    uint32_t fg_g = (fg >> 8)  & 0xFF;
    uint32_t fg_b =  fg        & 0xFF;

    uint32_t bg_r = ((bg >> 11) & 0x1F) << 3;
    uint32_t bg_g = ((bg >> 5)  & 0x3F) << 2;
    uint32_t bg_b = ( bg        & 0x1F) << 3;

    // FG is premultiplied
    uint32_t r = fg_r + (((bg_r * inv_a) * 257) >> 16);
    uint32_t g = fg_g + (((bg_g * inv_a) * 257) >> 16);
    uint32_t b = fg_b + (((bg_b * inv_a) * 257) >> 16);

    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}

// Alpha blend a premultiplied RGBA8888 pixel over a RGB565 background, with an 
// additional blend alpha
uint16_t alpha_blend_premul_rgba8888_over_rgb565_with_alpha(uint16_t bg, uint32_t fg, uint8_t alpha) 
{
    // fg is already premultiplied with it's own alpha component
    // but not the blend alpha
    uint32_t fg_r = (((fg >> 16) & 0xFF) * alpha * 257) >> 16;
    uint32_t fg_g = (((fg >> 8)  & 0xFF) * alpha * 257) >> 16;
    uint32_t fg_b = (((fg >> 0)  & 0xFF) * alpha * 257) >> 16;

    // Combine the component alpha with the blend alpha
    alpha = ((fg >> 24) * alpha * 257) >> 16;
    if (alpha == 0)
        return bg;
    uint32_t inv_a = 255 - alpha;

    uint32_t bg_r = (((bg >> 8) & 0xF8) * inv_a * 257) >> 16;
    uint32_t bg_g = (((bg >> 3) & 0xFC) * inv_a * 257) >> 16;
    uint32_t bg_b = (((bg << 3) & 0xF8) * inv_a * 257) >> 16;

    // Combine
    return (((fg_r + bg_r) >> 3) << 11) 
         | (((fg_g + bg_g) >> 2) << 5) 
         | (((fg_b + bg_b) >> 3) << 0);
}

// Alpha blend an opaque RGB565 pixel over a RGB565 background, with an 
// additional blend alpha
uint16_t alpha_blend_rgb565_over_rgb565_with_alpha(uint16_t bg, uint16_t fg, uint8_t alpha) 
{
    // Short cut
    if (alpha == 0)
        return bg;
    if (alpha == 0xFF)
        return fg;

    // Inv alpha
    uint32_t inv_a = 255 - alpha;

    // Calc fg components scaled by alpha
    uint32_t fg_r = (((fg >> 8) & 0xF8) * alpha * 257) >> 16;
    uint32_t fg_g = (((fg >> 3) & 0xFC) * alpha * 257) >> 16;
    uint32_t fg_b = (((fg << 3) & 0xF8) * alpha * 257) >> 16;

    // Calc bg components scaled by inv alpha
    uint32_t bg_r = (((bg >> 8) & 0xF8) * inv_a * 257) >> 16;
    uint32_t bg_g = (((bg >> 3) & 0xFC) * inv_a * 257) >> 16;
    uint32_t bg_b = (((bg << 3) & 0xF8) * inv_a * 257) >> 16;

    // Combine
    return (((fg_r + bg_r) >> 3) << 11) 
         | (((fg_g + bg_g) >> 2) << 5) 
         | (((fg_b + bg_b) >> 3) << 0);
}

uint32_t get_image_packed_size(const void* image)
{
    const IMAGE* header = (const IMAGE*)image;
    switch (header->format)
    {
        case IMAGE_FORMAT_RLE:
            return ((const IMAGE_RLE*)image)->packedSize;
    }

    assert(false);
    return 0;
}

// Read a variable length dword from current position
uint32_t ReadVarLen(const uint8_t** p)
{
	// Read first byte
	uint32_t val = 0;
    while (1)
    {
        uint8_t b = *p[0];
        p[0]++;
        val = (val << 7) | (b & 0x7F);
        if ((b & 0x80)== 0)
            return val;
    }
}

void SkipVarLen(const uint8_t** p, uint32_t count)
{
    while (count)
    {
        while (*p[0] & 0x80)
            p[0]++;
        p[0]++;
        count--;
    }
}

// Get the total number of bytes required to capture
// the pixels that would be overwritten by drawing a image
size_t get_capture_size(const void* image)
{
    const IMAGE* header = (const IMAGE*)image;
    switch (header->format)
    {
        case IMAGE_FORMAT_RLE:
            return ((const IMAGE_RLE*)image)->captureSize;
    }

    assert(false);
    return 0;
}

const PALETTE* get_image_palette(const void* image)
{
    const IMAGE* header = (const IMAGE*)image;
    switch (header->format)
    {
        case IMAGE_FORMAT_RLE:
        {
            const IMAGE_RLE* rle = (const IMAGE_RLE*)image;
            if (rle->paletteOffset == 0)
                return NULL;

            return (const PALETTE*)(((const uint8_t*)image) + rle->paletteOffset);
        }
    }

    assert(false);
    return 0;
}



uint32_t get_image_width(const void* image)
{
    return ((const IMAGE*)image)->width;
}

uint32_t get_image_height(const void* image)
{
    return ((const IMAGE*)image)->height;
}




void init_image_buffer(IMAGE_BUFFER* buffer, uint32_t width, uint32_t height, uint16_t* mem, uint32_t stride)
{
    buffer->base.format = IMAGE_FORMAT_BUFFER;
    buffer->base.width = width;
    buffer->base.height = height;
    buffer->buffer = mem;
    buffer->stride = stride == 0 ? width : stride;
    buffer->user = NULL;
}

void clip_image_buffer(IMAGE_BUFFER* buffer, IMAGE_BUFFER* underlying, uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    assert(x + width <= underlying->base.width);
    assert(y + height <= underlying->base.height);

    buffer->base.format = IMAGE_FORMAT_BUFFER;
    buffer->buffer = underlying->buffer + y * underlying->stride + x;
    buffer->stride = underlying->stride;
    buffer->base.width = width;
    buffer->base.height = height;
}

#pragma pack(push, 1)

typedef struct
{
    uint32_t    offset;
    uint16_t    pixelCount;
    uint16_t    pixels[];
} CAPTURERUN;

#pragma pack(pop)


typedef struct 
{
    uint16_t* buffer;
    uint16_t* caproot;
    uint32_t stride;
    const IMAGE_RLE* header;
    const uint32_t* rowOffsets;
    const PALETTE* palette;
    int32_t src_x;
    int32_t src_y;
    uint32_t width;
    uint32_t height;
    uint8_t alpha;
    uint32_t iy_start;
    uint32_t iy_end;
    void* captureBuffer;
    CAPTURERUN* captureRun;
} DRAWARGS;

typedef void (*PFNDRAWCORE)(DRAWARGS* args);

static void* end_of_caprun(CAPTURERUN* run)
{
    return (char*)run + sizeof(CAPTURERUN) + sizeof(uint16_t) * run->pixelCount;
}

static void capture_helper(DRAWARGS* args, uint16_t* pixels, uint32_t pixelCount)
{
    // Work out offset
    uint32_t capOffset = (uint32_t)((uint8_t*)pixels - (uint8_t*)args->caproot);

    if (!args->captureRun)
    {
        // First captured run
        args->captureRun = (CAPTURERUN*)args->captureBuffer;
        args->captureRun->offset = capOffset;
        args->captureRun->pixelCount = 0;
    }
    else if (args->captureRun->offset + args->captureRun->pixelCount * sizeof(uint16_t) != capOffset)
    {
        // Start a new capture run
        args->captureRun = (CAPTURERUN*)end_of_caprun(args->captureRun);
        args->captureRun->offset = capOffset;
        args->captureRun->pixelCount = 0;
    }

    memcpy(end_of_caprun(args->captureRun), pixels, pixelCount * sizeof(uint16_t));
    args->captureRun->pixelCount += pixelCount;
}

static void capture_finalize(DRAWARGS* args)
{
    CAPTURERUN* last = (CAPTURERUN*)(args->captureRun ? end_of_caprun(args->captureRun) : args->captureBuffer);
    last->offset = 0;
    last->pixelCount = 0;
}

void restore_image(IMAGE_BUFFER* dest_buffer, const void* capture)
{
    const CAPTURERUN* cap = (const CAPTURERUN*)capture;

    uint8_t* buffer = (uint8_t*)dest_buffer->buffer;

    while (true)
    {
        if (cap->offset == 0 && cap->pixelCount == 0)
            break;

        memcpy((uint8_t*)buffer + cap->offset, &cap->pixels, cap->pixelCount * 2);
        cap = (const CAPTURERUN*)end_of_caprun((CAPTURERUN*)cap);
    }
}



#include "rle565.core.h"

void draw_image(IMAGE_BUFFER* dest_buffer, const void* image, int32_t dest_x, int32_t dest_y, int32_t src_x, int32_t src_y, uint32_t width, uint32_t height, void* cap, uint8_t alpha, const PALETTE* palette)
{
    const IMAGE_RLE* header = (const IMAGE_RLE*)image;
    assert(header->base.format == IMAGE_FORMAT_RLE);

    src_x -= dest_x;
    width += dest_x;
    src_y -= dest_y;
    height += dest_y;

    // Quit if transparent
    if (alpha == 0)
        return;

    // Out of range
    if (src_y + (int32_t)height < 0 || src_y >= (int32_t)header->base.height)
        return;
    if (src_x + (int32_t)width < 0 || src_x >= (int32_t)header->base.width)
        return;

    uint16_t* buffer = dest_buffer->buffer;

    // Clip top
    if (src_y < 0)
    {
        buffer += -src_y * dest_buffer->stride;
        height -= -src_y;
        src_y = 0;
    }

    // Clip left
    if (src_x < 0)
    {
        buffer += -src_x;
        width -= -src_x;
        src_x = 0;
    }

    // Clip right
    if (width > dest_buffer->base.width)
        width = dest_buffer->base.width;

    // Clip bottom
    if (height > dest_buffer->base.height)
        height = dest_buffer->base.height;

    // Move to first row
    if (src_y < (int32_t)header->firstRow)
    {
        buffer += (header->firstRow - src_y) * dest_buffer->stride;
        height -= header->firstRow - src_y;
        src_y = header->firstRow;
    }

    // Stop end last row
    if (src_y + height > header->firstRow + header->rowCount)
        height = header->firstRow + header->rowCount - src_y;

    // Setup drawing args
    DRAWARGS args;
    args.buffer = buffer;
    args.caproot = dest_buffer->buffer;
    args.stride = dest_buffer->stride;
    args.header = header;
    args.src_x = src_x;
    args.src_y = src_y;
    args.width = width;
    args.height = height;
    args.alpha = alpha;
    args.captureBuffer = cap;
    args.captureRun = nullptr;
    args.iy_start = src_y - header->firstRow;
    args.iy_end = src_y + height - header->firstRow;
    args.rowOffsets = (const uint32_t*)(((uint8_t*)image) + sizeof(*header));
    args.palette = palette ? palette : (const PALETTE*)((char*)image + header->paletteOffset);

    // Work out draw flags
    uint32_t flags = header->base.flags;
    if (alpha != 255)
        flags |= DRAW_FLAGS_ALPHA_BLEND;
    if (src_x != 0 || width != header->base.width)
        flags |= DRAW_FLAGS_XCLIP;
    if (cap != nullptr)
        flags |= DRAW_FLAGS_CAPTURE;

    // Call core
    drawcores[flags](&args);
}

uint32_t unpremultiply_argb8888(uint32_t c) 
{
    uint8_t a = (c >> 24) & 0xFF;
    uint8_t r = (c >> 16) & 0xFF;
    uint8_t g = (c >> 8)  & 0xFF;
    uint8_t b = (c >> 0)  & 0xFF;

    if (a == 0) {
        return 0; // or (0x00000000)
    }

    // scale back up
    r = (uint16_t)r * 255 / a;
    g = (uint16_t)g * 255 / a;
    b = (uint16_t)b * 255 / a;

    return (a << 24) | (r << 16) | (g << 8) | b;
}

uint32_t premultiply_argb888(uint32_t c) 
{
    uint8_t a = (c >> 24) & 0xFF;
    uint8_t r = (c >> 16) & 0xFF;
    uint8_t g = (c >> 8)  & 0xFF;
    uint8_t b = (c >> 0)  & 0xFF;

    // fully transparent → force RGB = 0
    if (a == 0) {
        return 0;
    }

    r = (uint16_t)r * a / 255;
    g = (uint16_t)g * a / 255;
    b = (uint16_t)b * a / 255;

    return (a << 24) | (r << 16) | (g << 8) | b;
}

void normalize_palette(PALETTE* palette)
{
    for (uint32_t i = 0; i < palette->count; i++) 
    {
        uint32_t color = palette->colors[i];
        if ((color & 0xFF000000) == 0xFF000000)
        {
            palette->colors[i] = 0xFF000000 | rgb565_to_rgb888(color);
        }
        else
        {
            palette->colors[i] = unpremultiply_argb8888(color);
        }
    }
}

void prepare_palette(PALETTE* palette)
{
    for (uint32_t i = 0; i < palette->count; i++) 
    {
        uint32_t px = palette->colors[i];
        if ((px & 0xFF000000) == 0xFF000000)
        {
            palette->colors[i] =  0xFF000000 | rgb888_to_rgb565(px);
        }
        else
        {
            palette->colors[i] = premultiply_argb888(px);
        }
    }
}


void recolor_palette(PALETTE* palette, uint32_t colorA, uint32_t colorB, int channelShift)
{
    normalize_palette(palette);

    for (uint32_t i=0; i<palette->count; i++)
    {
        uint32_t color = palette->colors[i];
        uint8_t t = (color >> channelShift) & 0xFF;

        uint32_t r = (((255 - t) * ((colorA >> 16) & 0xFF) + t * ((colorB >> 16) & 0xFF)) * 257 ) >> 16;
        uint32_t b = (((255 - t) * ((colorA >> 0) & 0xFF) + t * ((colorB >> 0) & 0xFF)) * 257 ) >> 16;
        uint32_t g = (((255 - t) * ((colorA >> 8) & 0xFF) + t * ((colorB >> 8) & 0xFF)) * 257 ) >> 16;
        palette->colors[i] = (color & 0xFF000000) | (r << 16) | (g << 8) | b;
    }

    prepare_palette(palette);
}
