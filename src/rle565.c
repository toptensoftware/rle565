#include <assert.h>
#include <string.h>
#include <assert.h>

#include "rle565.h"

#if (!defined(__STDC_VERSION__) || __STDC_VERSION__ < 202311L)
    #include <stddef.h>
    #define nullptr ((void*)0)
    typedef void* nullptr_t;
#endif


// Divide by 255, rounding to nearest.  Exact for 0 <= x <= 255 * 255.
static inline uint32_t div255(uint32_t x)
{
    x += 128;
    return (x + (x >> 8)) >> 8;
}

// Expand 5 and 6 bit channels to 8 bits by bit replication, so full
// intensity maps to 255 (not 248/252)
static inline uint32_t expand5(uint32_t c) { return (c << 3) | (c >> 2); }
static inline uint32_t expand6(uint32_t c) { return (c << 2) | (c >> 4); }

// Blend a premultiplied 8 bit color (alpha a) over an RGB565 background and
// round the result to RGB565
static inline uint16_t blend_premul_over_rgb565(uint16_t bg, uint32_t fg_r, uint32_t fg_g, uint32_t fg_b, uint32_t a)
{
    uint32_t inv_a = 255 - a;

    uint32_t r = fg_r + div255(expand5((bg >> 11) & 0x1F) * inv_a);
    uint32_t g = fg_g + div255(expand6((bg >> 5)  & 0x3F) * inv_a);
    uint32_t b = fg_b + div255(expand5( bg        & 0x1F) * inv_a);

    return (uint16_t)((div255(r * 31) << 11) | (div255(g * 63) << 5) | div255(b * 31));
}

// Alpha blend a premultiplied RGBA8888 pixel over a RGB565 background
uint16_t alpha_blend_premul_rgba8888_over_rgb565(uint16_t bg, uint32_t fg)
{
    uint32_t alpha = fg >> 24;
    if (alpha == 0)
        return bg;

    return blend_premul_over_rgb565(bg, (fg >> 16) & 0xFF, (fg >> 8) & 0xFF, fg & 0xFF, alpha);
}

// Alpha blend a premultiplied RGBA8888 pixel over a RGB565 background, with an
// additional blend alpha
uint16_t alpha_blend_premul_rgba8888_over_rgb565_with_alpha(uint16_t bg, uint32_t fg, uint8_t alpha)
{
    // Combine the component alpha with the blend alpha
    uint32_t a = div255((fg >> 24) * alpha);
    if (a == 0)
        return bg;

    // fg is already premultiplied with its own alpha but not the blend alpha
    return blend_premul_over_rgb565(bg,
        div255(((fg >> 16) & 0xFF) * alpha),
        div255(((fg >> 8)  & 0xFF) * alpha),
        div255(( fg        & 0xFF) * alpha),
        a);
}

// Alpha blend an opaque RGB565 pixel over a RGB565 background, with an
// additional blend alpha.  Interpolates in each channel's native precision
// so blending a color over itself leaves it unchanged.
uint16_t alpha_blend_rgb565_over_rgb565_with_alpha(uint16_t bg, uint16_t fg, uint8_t alpha)
{
    // Short cut
    if (alpha == 0)
        return bg;
    if (alpha == 0xFF)
        return fg;

    uint32_t inv_a = 255 - alpha;

    uint32_t r = div255(((fg >> 11) & 0x1F) * alpha + ((bg >> 11) & 0x1F) * inv_a);
    uint32_t g = div255(((fg >> 5)  & 0x3F) * alpha + ((bg >> 5)  & 0x3F) * inv_a);
    uint32_t b = div255(( fg        & 0x1F) * alpha + ( bg        & 0x1F) * inv_a);

    return (uint16_t)((r << 11) | (g << 5) | b);
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
    buffer->base.signature = IMAGE_SIGNATURE;
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

    buffer->base.signature = IMAGE_SIGNATURE;
    buffer->base.format = IMAGE_FORMAT_BUFFER;
    buffer->buffer = underlying->buffer + y * underlying->stride + x;
    buffer->stride = underlying->stride;
    buffer->base.width = width;
    buffer->base.height = height;
}

void fill_rect(IMAGE_BUFFER* buffer, int32_t x, int32_t y, uint32_t width, uint32_t height, uint16_t color)
{
    assert(buffer->base.signature == IMAGE_SIGNATURE);
    assert(buffer->base.format == IMAGE_FORMAT_BUFFER);

    // Clip to the buffer, in 64-bit signed so nothing can wrap
    int64_t x0 = x;
    int64_t y0 = y;
    int64_t x1 = x0 + width;
    int64_t y1 = y0 + height;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > (int64_t)buffer->base.width) x1 = buffer->base.width;
    if (y1 > (int64_t)buffer->base.height) y1 = buffer->base.height;
    if (x0 >= x1 || y0 >= y1)
        return;

    uint32_t w = (uint32_t)(x1 - x0);
    uint16_t* row = buffer->buffer + (size_t)y0 * buffer->stride + (size_t)x0;
    for (int64_t r = y0; r < y1; r++, row += buffer->stride)
    {
        for (uint32_t i = 0; i < w; i++)
            row[i] = color;
    }
}

// Fast RGB565 blending: spread a pixel to 0x07E0F81F layout (green moved to
// the top half) so all three channels can be scaled by a 0-32 alpha with one
// multiply without overflowing into each other.
#define SPREAD565_MASK 0x07E0F81Fu
#define SPREAD565_HALF 0x02008010u      // 16 in each channel, for rounding

static inline uint32_t spread565(uint16_t c)
{
    return (c | ((uint32_t)c << 16)) & SPREAD565_MASK;
}

// Blend spread color fg over bg with a5 in 0-32
static inline uint16_t blend565_a5(uint16_t bg, uint32_t fg, uint32_t a5)
{
    uint32_t r = ((fg * a5 + spread565(bg) * (32 - a5) + SPREAD565_HALF) >> 5) & SPREAD565_MASK;
    return (uint16_t)(r | (r >> 16));
}

// Draw one row of mask with coverage scaled by k (see draw_alpha_mask)
static inline void draw_alpha_mask_row(uint16_t* dst, const uint8_t* m, uint32_t w, uint16_t color, uint32_t fg, uint32_t k)
{
    // k == 0 means full opacity: a5 is just the coverage reduced to 0-32
    #define MASK_PIXEL(j) \
        { \
            uint32_t a5 = k ? (m[j] * k + 0x8000) >> 16 : (m[j] + 4u) >> 3; \
            if (a5 == 32) dst[j] = color; \
            else if (a5 != 0) dst[j] = blend565_a5(dst[j], fg, a5); \
        }

    uint32_t i = 0;
    for (; i + 4 <= w; i += 4)
    {
        // Glyphs are mostly empty, so skip (or fill) four pixels at a time
        uint32_t quad;
        memcpy(&quad, m + i, 4);
        if (quad == 0)
            continue;
        if (quad == 0xFFFFFFFFu && k == 0)
        {
            dst[i] = dst[i + 1] = dst[i + 2] = dst[i + 3] = color;
            continue;
        }
        MASK_PIXEL(i); MASK_PIXEL(i + 1); MASK_PIXEL(i + 2); MASK_PIXEL(i + 3);
    }
    for (; i < w; i++)
        MASK_PIXEL(i);

    #undef MASK_PIXEL
}

void draw_alpha_mask(IMAGE_BUFFER* buffer, int32_t dest_x, int32_t dest_y, const uint8_t* mask, uint32_t mask_stride, uint32_t width, uint32_t height, uint16_t color, uint8_t alpha)
{
    assert(buffer->base.signature == IMAGE_SIGNATURE);
    assert(buffer->base.format == IMAGE_FORMAT_BUFFER);

    if (alpha == 0)
        return;

    // Clip to the buffer, in 64-bit signed so nothing can wrap
    int64_t x0 = dest_x;
    int64_t y0 = dest_y;
    int64_t x1 = x0 + width;
    int64_t y1 = y0 + height;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > (int64_t)buffer->base.width) x1 = buffer->base.width;
    if (y1 > (int64_t)buffer->base.height) y1 = buffer->base.height;
    if (x0 >= x1 || y0 >= y1)
        return;

    mask += (size_t)(y0 - dest_y) * mask_stride + (size_t)(x0 - dest_x);
    uint16_t* dst = buffer->buffer + (size_t)y0 * buffer->stride + (size_t)x0;
    uint32_t w = (uint32_t)(x1 - x0);
    uint32_t fg = spread565(color);

    // With partial opacity, coverage * alpha / 255 reduced to 0-32 is
    // (coverage * k) >> 16, rounded.  k == 0 selects the opaque path.
    uint32_t k = alpha == 255 ? 0 : (alpha * 32u * 65536u + 255u * 255u / 2) / (255u * 255u);

    // Separate loops so the opaque one is compiled with k as a constant
    if (k == 0)
    {
        for (int64_t r = y0; r < y1; r++, dst += buffer->stride, mask += mask_stride)
            draw_alpha_mask_row(dst, mask, w, color, fg, 0);
    }
    else
    {
        for (int64_t r = y0; r < y1; r++, dst += buffer->stride, mask += mask_stride)
            draw_alpha_mask_row(dst, mask, w, color, fg, k);
    }
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

    while (pixelCount)
    {
        if (!args->captureRun)
        {
            // First captured run
            args->captureRun = (CAPTURERUN*)args->captureBuffer;
            args->captureRun->offset = capOffset;
            args->captureRun->pixelCount = 0;
        }
        else if (args->captureRun->offset + args->captureRun->pixelCount * sizeof(uint16_t) != capOffset
              || args->captureRun->pixelCount == UINT16_MAX)
        {
            // Start a new capture run (not contiguous, or current run is full)
            args->captureRun = (CAPTURERUN*)end_of_caprun(args->captureRun);
            args->captureRun->offset = capOffset;
            args->captureRun->pixelCount = 0;
        }

        // Append as many pixels as fit in the current run's 16-bit count.  Runs
        // only get this long when spans merge across rows (stride == width) so
        // splitting never needs more run headers than the encoder allowed for.
        uint32_t n = UINT16_MAX - args->captureRun->pixelCount;
        if (n > pixelCount)
            n = pixelCount;

        memcpy(end_of_caprun(args->captureRun), pixels, n * sizeof(uint16_t));
        args->captureRun->pixelCount += (uint16_t)n;

        pixels += n;
        pixelCount -= n;
        capOffset += n * sizeof(uint16_t);
    }
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
    assert(header->base.signature == IMAGE_SIGNATURE);
    assert(header->base.format == IMAGE_FORMAT_RLE);

    // Images packed against a shared palette don't carry their own
    assert(palette != nullptr || header->paletteOffset != 0);

    // Start with an empty capture so restore_image is safe even if
    // nothing gets drawn
    if (cap != nullptr)
    {
        CAPTURERUN* term = (CAPTURERUN*)cap;
        term->offset = 0;
        term->pixelCount = 0;
    }

    // Quit if transparent
    if (alpha == 0)
        return;

    // Work in 64-bit signed so nothing can wrap
    int64_t sx = src_x;
    int64_t sy = src_y;
    int64_t dx = dest_x;
    int64_t dy = dest_y;
    int64_t w = width;
    int64_t h = height;

    // Clip source rectangle to the image (negative source coords
    // shift the drawn area right/down in the destination)
    if (sx < 0) { dx -= sx; w += sx; sx = 0; }
    if (sy < 0) { dy -= sy; h += sy; sy = 0; }
    if (sx + w > (int64_t)header->base.width)
        w = (int64_t)header->base.width - sx;
    if (sy + h > (int64_t)header->base.height)
        h = (int64_t)header->base.height - sy;

    // Clip to the destination buffer
    if (dx < 0) { sx -= dx; w += dx; dx = 0; }
    if (dy < 0) { sy -= dy; h += dy; dy = 0; }
    if (dx + w > (int64_t)dest_buffer->base.width)
        w = (int64_t)dest_buffer->base.width - dx;
    if (dy + h > (int64_t)dest_buffer->base.height)
        h = (int64_t)dest_buffer->base.height - dy;

    // Clip to the rows actually stored in the image
    int64_t firstRow = header->firstRow;
    int64_t endRow = firstRow + header->rowCount;
    if (sy < firstRow)
    {
        dy += firstRow - sy;
        h -= firstRow - sy;
        sy = firstRow;
    }
    if (sy + h > endRow)
        h = endRow - sy;

    // Nothing left to draw?
    if (w <= 0 || h <= 0)
        return;

    // Setup drawing args
    DRAWARGS args;
    args.buffer = dest_buffer->buffer + dy * dest_buffer->stride + dx;
    args.caproot = dest_buffer->buffer;
    args.stride = dest_buffer->stride;
    args.header = header;
    args.src_x = (int32_t)sx;
    args.src_y = (int32_t)sy;
    args.width = (uint32_t)w;
    args.height = (uint32_t)h;
    args.alpha = alpha;
    args.captureBuffer = cap;
    args.captureRun = nullptr;
    args.iy_start = (uint32_t)(sy - firstRow);
    args.iy_end = (uint32_t)(sy + h - firstRow);
    args.rowOffsets = (const uint32_t*)(((uint8_t*)image) + sizeof(*header));
    args.palette = palette ? palette : (const PALETTE*)((char*)image + header->paletteOffset);

    // Work out draw flags
    uint32_t flags = header->base.flags;
    if (alpha != 255)
        flags |= DRAW_FLAGS_ALPHA_BLEND;
    if (sx != 0 || w != (int64_t)header->base.width)
        flags |= DRAW_FLAGS_XCLIP;
    if (cap != nullptr)
        flags |= DRAW_FLAGS_CAPTURE;

    // Call core
    drawcores[flags](&args);
}

uint32_t unpremultiply_argb8888(uint32_t c)
{
    uint32_t a = c >> 24;
    if (a == 0)
        return 0;

    // Scale back up, rounding to nearest and clamping bad (color > alpha) input
    uint32_t r = (((c >> 16) & 0xFF) * 255 + a / 2) / a;
    uint32_t g = (((c >> 8)  & 0xFF) * 255 + a / 2) / a;
    uint32_t b = (( c        & 0xFF) * 255 + a / 2) / a;
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;

    return (a << 24) | (r << 16) | (g << 8) | b;
}

uint32_t premultiply_argb8888(uint32_t c)
{
    uint32_t a = c >> 24;

    // Fully transparent: force RGB = 0
    if (a == 0)
        return 0;

    // Round to nearest (matches the encoder)
    uint32_t r = div255(((c >> 16) & 0xFF) * a);
    uint32_t g = div255(((c >> 8)  & 0xFF) * a);
    uint32_t b = div255(( c        & 0xFF) * a);

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
            palette->colors[i] = premultiply_argb8888(px);
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


// ---- Decoding ----

// Bounds checked byte reader used by the decoder
typedef struct
{
    const uint8_t* p;
    const uint8_t* end;
    bool ok;
} SAFEREADER;

static uint32_t safe_read_byte(SAFEREADER* r)
{
    if (r->p >= r->end)
    {
        r->ok = false;
        return 0;
    }
    return *r->p++;
}

static uint32_t safe_read_varlen(SAFEREADER* r)
{
    uint32_t val = 0;
    for (int i = 0; i < 5; i++)
    {
        uint32_t b = safe_read_byte(r);
        val = (val << 7) | (b & 0x7F);
        if ((b & 0x80) == 0)
            return val;
    }

    // More than 5 bytes can't be a valid 32-bit value
    r->ok = false;
    return 0;
}

// Convert a prepared palette entry to 0xAARRGGBB
static uint32_t palette_entry_to_argb8888(uint32_t c, bool premultiplied)
{
    uint32_t a = c >> 24;

    // Fully transparent
    if (a == 0)
        return 0;

    // Opaque entries are 0xFF00 + RGB565.  Expand with bit replication
    // so full intensity maps to 0xFF.
    if (a == 0xFF)
    {
        return 0xFF000000 | rgb565_to_rgb888(c);
    }

    // Partially transparent entries are already premultiplied ARGB8888
    if (premultiplied)
        return c;

    return unpremultiply_argb8888(c);
}

bool check_image_header(const void* image, size_t image_size)
{
    const IMAGE_RLE* header = (const IMAGE_RLE*)image;

    if (image == nullptr || image_size < sizeof(IMAGE_RLE))
        return false;
    if (header->base.signature != IMAGE_SIGNATURE)
        return false;
    if (header->base.format != IMAGE_FORMAT_RLE)
        return false;

    // Packed size covers the whole image and must be available
    uint64_t size = header->packedSize;
    if (size < sizeof(IMAGE_RLE) || size > image_size)
        return false;

    // Stored rows within the image, row table within the data
    if ((uint64_t)header->firstRow + header->rowCount > header->base.height)
        return false;
    if (sizeof(IMAGE_RLE) + (uint64_t)header->rowCount * sizeof(uint32_t) > size)
        return false;

    // Embedded palette (optional) within the data
    if (header->paletteOffset != 0)
    {
        uint64_t offset = header->paletteOffset;
        if (offset + sizeof(PALETTE) > size)
            return false;
        const PALETTE* palette = (const PALETTE*)((const uint8_t*)image + offset);
        if (offset + sizeof(PALETTE) + (uint64_t)palette->count * sizeof(uint32_t) > size)
            return false;
    }

    return true;
}

// Walk every row of an image, bounds checking everything read. Decodes into
// `out` if it isn't NULL (see decode_image_argb8888), and sets `*opaque` (if
// not NULL) to whether every pixel is fully opaque.
static bool walk_image(const void* image, size_t image_size, const PALETTE* palette, uint32_t* out, uint32_t stride, bool premultiplied, bool* opaque)
{
    const uint8_t* base = (const uint8_t*)image;
    const IMAGE_RLE* header = (const IMAGE_RLE*)image;

    // Validate header
    if (!check_image_header(image, image_size))
        return false;

    // Only read within the image itself (image_size may cover more)
    image_size = header->packedSize;

    uint32_t width = header->base.width;
    uint32_t height = header->base.height;
    if (stride == 0)
        stride = width;
    if (stride < width)
        return false;

    // Use the image's own palette (already validated) if none supplied
    if (palette == nullptr)
    {
        if (header->paletteOffset == 0)
            return false;
        palette = (const PALETTE*)(base + header->paletteOffset);
    }

    bool alphaPixels = (header->base.flags & DRAW_FLAGS_ALPHA_PIXELS) != 0;
    bool largePalette = (header->base.flags & DRAW_FLAGS_LARGE_PALETTE) != 0;
    const uint32_t* rowOffsets = (const uint32_t*)(base + sizeof(IMAGE_RLE));

    // Rows outside the stored range are transparent
    bool allOpaque = header->firstRow == 0 && header->rowCount == height;

    for (uint32_t y = 0; y < height; y++)
    {
        uint32_t* row = out ? out + (size_t)y * stride : nullptr;

        // Rows outside the stored range are fully transparent
        if (y < header->firstRow || y >= header->firstRow + header->rowCount)
        {
            if (row)
                memset(row, 0, width * sizeof(uint32_t));
            continue;
        }

        uint32_t rowOffset = rowOffsets[y - header->firstRow];
        if (rowOffset >= image_size)
            return false;

        SAFEREADER r = { base + rowOffset, base + image_size, true };
        uint32_t runCount = safe_read_varlen(&r);
        uint32_t x = 0;

        for (uint32_t ri = 0; ri < runCount && r.ok; ri++)
        {
            uint32_t pixCount = safe_read_varlen(&r);
            bool is_rle = pixCount & 1;
            pixCount >>= 1;
            if (!is_rle && alphaPixels)
                pixCount >>= 1;     // drop the alpha-run bit

            if (pixCount > width - x)
                return false;

            for (uint32_t i = 0; i < pixCount; i++)
            {
                // RLE runs have one index, flat runs one per pixel
                if (i == 0 || !is_rle)
                {
                    uint32_t pi = largePalette ? safe_read_varlen(&r) : safe_read_byte(&r);
                    if (!r.ok || pi >= palette->count)
                        return false;
                    uint32_t c = palette->colors[pi];
                    if ((c >> 24) != 0xFF)
                        allOpaque = false;
                    if (row)
                        row[x] = palette_entry_to_argb8888(c, premultiplied);
                }
                else if (row)
                {
                    row[x] = row[x - 1];
                }
                x++;
            }
        }

        if (!r.ok)
            return false;

        // Treat anything not covered by runs as transparent
        if (x < width)
            allOpaque = false;
        while (row && x < width)
            row[x++] = 0;
    }

    if (opaque)
        *opaque = allOpaque;
    return true;
}

bool decode_image_argb8888(const void* image, size_t image_size, const PALETTE* palette, uint32_t* out, uint32_t stride, bool premultiplied)
{
    if (out == nullptr)
        return false;
    return walk_image(image, image_size, palette, out, stride, premultiplied, nullptr);
}

bool check_image(const void* image, size_t image_size, bool* opaque)
{
    return walk_image(image, image_size, nullptr, nullptr, 0, false, opaque);
}


// ---------------------------------------------------------------------------
// Fonts

static inline const FONT_GLYPH* font_glyphs(const FONT* font)
{
    return (const FONT_GLYPH*)((const uint8_t*)font + font->glyphsOffset);
}

bool check_font(const void* font_data, size_t size)
{
    const FONT* font = (const FONT*)font_data;
    if (font == nullptr || size < sizeof(FONT))
        return false;
    if (font->signature != FONT_SIGNATURE || font->version != FONT_VERSION)
        return false;
    if (font->packedSize < sizeof(FONT) || font->packedSize > size)
        return false;

    // Sections must fit (64-bit math so nothing wraps) and be aligned
    uint64_t packed = font->packedSize;
    if (font->glyphsOffset % 4 != 0 || font->kerningOffset % 2 != 0 || font->hashOffset % 2 != 0)
        return false;
    if (font->glyphsOffset + (uint64_t)font->glyphCount * sizeof(FONT_GLYPH) > packed)
        return false;
    if (font->kerningOffset + (uint64_t)font->kerningCount * sizeof(FONT_KERNING) > packed)
        return false;
    if (font->hashBits < 1 || font->hashBits > 17)
        return false;
    uint32_t slots = 1u << font->hashBits;
    if (font->hashOffset + (uint64_t)slots * 2 > packed)
        return false;
    if (font->bitmapsOffset > packed)
        return false;
    if (font->glyphCount >= FONT_NO_GLYPH)
        return false;
    if (font->defaultGlyph != FONT_NO_GLYPH && font->defaultGlyph >= font->glyphCount)
        return false;
    if (font->subpixel != 1 && font->subpixel != 2 && font->subpixel != 4 && font->subpixel != 8)
        return false;

    // Glyph bitmaps (one per subpixel position) and kerning ranges
    uint64_t bitmapBytes = packed - font->bitmapsOffset;
    const FONT_GLYPH* glyphs = font_glyphs(font);
    for (uint32_t i = 0; i < font->glyphCount; i++)
    {
        const FONT_GLYPH* g = &glyphs[i];
        if (g->bitmap + (uint64_t)g->width * g->height * font->subpixel > bitmapBytes)
            return false;
        if ((uint64_t)g->kerning + g->kerningCount > font->kerningCount)
            return false;
    }

    const FONT_KERNING* kerning = (const FONT_KERNING*)((const uint8_t*)font + font->kerningOffset);
    for (uint32_t i = 0; i < font->kerningCount; i++)
    {
        if (kerning[i].right >= font->glyphCount)
            return false;
    }

    // Hash entries must be valid glyph indexes
    const uint16_t* table = (const uint16_t*)((const uint8_t*)font + font->hashOffset);
    for (uint32_t i = 0; i < slots; i++)
    {
        if (table[i] != FONT_NO_GLYPH && table[i] >= font->glyphCount)
            return false;
    }

    return true;
}

const FONT_GLYPH* font_find_glyph(const FONT* font, uint32_t codepoint)
{
    assert(font->signature == FONT_SIGNATURE);

    const FONT_GLYPH* glyphs = font_glyphs(font);
    const uint16_t* table = (const uint16_t*)((const uint8_t*)font + font->hashOffset);
    uint32_t mask = (1u << font->hashBits) - 1;
    uint32_t slot = (codepoint * 0x9E3779B1u) >> (32 - font->hashBits);

    // The table is never full so this ends at an empty slot, but limit the
    // probes anyway so a damaged font can't loop forever
    for (uint32_t probes = 0; probes <= mask; probes++, slot = (slot + 1) & mask)
    {
        uint16_t index = table[slot];
        if (index == FONT_NO_GLYPH)
            return nullptr;
        if (glyphs[index].codepoint == codepoint)
            return &glyphs[index];
    }
    return nullptr;
}

const FONT_GLYPH* font_get_glyph(const FONT* font, uint32_t codepoint)
{
    const FONT_GLYPH* glyph = font_find_glyph(font, codepoint);
    if (glyph == nullptr && font->defaultGlyph != FONT_NO_GLYPH)
        glyph = &font_glyphs(font)[font->defaultGlyph];
    return glyph;
}

int32_t font_get_kerning(const FONT* font, const FONT_GLYPH* left, const FONT_GLYPH* right)
{
    if (left == nullptr || right == nullptr || left->kerningCount == 0)
        return 0;

    // Binary search the left glyph's pairs for the right glyph
    uint16_t target = (uint16_t)(right - font_glyphs(font));
    const FONT_KERNING* pairs = (const FONT_KERNING*)((const uint8_t*)font + font->kerningOffset) + left->kerning;
    uint32_t lo = 0, hi = left->kerningCount;
    while (lo < hi)
    {
        uint32_t mid = (lo + hi) / 2;
        if (pairs[mid].right < target)
            lo = mid + 1;
        else if (pairs[mid].right > target)
            hi = mid;
        else
            return pairs[mid].adjust;
    }
    return 0;
}

const uint8_t* font_get_bitmap(const FONT* font, const FONT_GLYPH* glyph)
{
    return (const uint8_t*)font + font->bitmapsOffset + glyph->bitmap;
}

// Decode one UTF-8 character and advance *pp.  Invalid, overlong or
// truncated sequences decode as U+FFFD.
static uint32_t utf8_decode(const uint8_t** pp, const uint8_t* end)
{
    const uint8_t* p = *pp;
    uint32_t c = *p++;
    uint32_t min;
    int extra;
    if (c < 0x80)
    {
        *pp = p;
        return c;
    }
    else if ((c & 0xE0) == 0xC0) { extra = 1; c &= 0x1F; min = 0x80; }
    else if ((c & 0xF0) == 0xE0) { extra = 2; c &= 0x0F; min = 0x800; }
    else if ((c & 0xF8) == 0xF0) { extra = 3; c &= 0x07; min = 0x10000; }
    else
    {
        *pp = p;
        return 0xFFFD;
    }

    for (int i = 0; i < extra; i++)
    {
        if (p >= end || (*p & 0xC0) != 0x80)
        {
            *pp = p;
            return 0xFFFD;
        }
        c = (c << 6) | (*p++ & 0x3F);
    }
    *pp = p;

    if (c < min || c > 0x10FFFF || (c >= 0xD800 && c <= 0xDFFF))
        return 0xFFFD;
    return c;
}

// Divide rounding towards negative infinity (b > 0)
static inline int64_t floor_div(int64_t a, int64_t b)
{
    int64_t q = a / b;
    return (a % b != 0 && a < 0) ? q - 1 : q;
}

int32_t font_get_advance(const FONT* font, const FONT_GLYPH* glyph, const FONT_GLYPH* next)
{
    // Round to the nearest step of the font's grid (halves up)
    int32_t unit = 64 / font->subpixel;
    int32_t v = (int32_t)glyph->advance + font_get_kerning(font, glyph, next);
    return (int32_t)floor_div(v + unit / 2, unit) * unit;
}

void draw_glyph(IMAGE_BUFFER* buffer, const FONT* font, const FONT_GLYPH* glyph, int32_t x64, int32_t y, uint16_t color, uint8_t alpha)
{
    if (glyph->width == 0)
        return;

    // Nearest subpixel position, split into whole pixels and bitmap index
    int64_t q = floor_div((int64_t)x64 * font->subpixel + 32, 64);
    int64_t x = floor_div(q, font->subpixel);
    uint32_t variant = (uint32_t)(q - x * font->subpixel);

    const uint8_t* bitmap = font_get_bitmap(font, glyph) + (size_t)variant * glyph->width * glyph->height;
    draw_alpha_mask(buffer, (int32_t)(x + glyph->left), y + glyph->top, bitmap, glyph->width, glyph->width, glyph->height, color, alpha);
}

// Lay out a UTF-8 string from pen position 0, drawing each glyph if buffer
// isn't NULL.  Returns the pen position after the text in 1/64 pixels.
static int32_t layout_text(IMAGE_BUFFER* buffer, const FONT* font, int32_t x, int32_t y, const char* text, int32_t length, uint16_t color, uint8_t alpha)
{
    assert(font->signature == FONT_SIGNATURE);

    const uint8_t* p = (const uint8_t*)text;
    const uint8_t* end = p + (length < 0 ? strlen(text) : (size_t)length);

    int32_t pen = 0;
    const FONT_GLYPH* prev = nullptr;
    while (p < end)
    {
        const FONT_GLYPH* glyph = font_get_glyph(font, utf8_decode(&p, end));
        if (glyph == nullptr)
            continue;
        if (prev != nullptr)
            pen += font_get_advance(font, prev, glyph);
        if (buffer != nullptr)
            draw_glyph(buffer, font, glyph, x * 64 + pen, y, color, alpha);
        prev = glyph;
    }
    if (prev != nullptr)
        pen += font_get_advance(font, prev, nullptr);
    return pen;
}

int32_t draw_text(IMAGE_BUFFER* buffer, const FONT* font, int32_t x, int32_t y, const char* text, int32_t length, uint16_t color, uint8_t alpha)
{
    if (alpha == 0)
        return x + font_measure_text(font, text, length);
    return x + (int32_t)floor_div(layout_text(buffer, font, x, y, text, length, color, alpha) + 32, 64);
}

int32_t font_measure_text(const FONT* font, const char* text, int32_t length)
{
    return (int32_t)floor_div(layout_text(nullptr, font, 0, 0, text, length, 0, 0) + 32, 64);
}
