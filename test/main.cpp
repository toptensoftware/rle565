#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <assert.h>
#pragma warning(disable: 4200)
#include "../src/rle565.h"

void rgb565_to_888(uint16_t px, uint8_t *r, uint8_t *g, uint8_t *b) {
    uint32_t c = rgb565_to_rgb888(px);
    *r = (uint8_t)(c >> 16);
    *g = (uint8_t)(c >> 8);
    *b = (uint8_t)c;
}

// Divide by 255 rounding to nearest (same as the library's blending)
static uint32_t div255(uint32_t x)
{
    x += 128;
    return (x + (x >> 8)) >> 8;
}

void write_bmp(const char *filename, uint16_t *pixels, int w, int h) {
    FILE *f = fopen(filename, "wb");

    int row_size = (w * 3 + 3) & ~3;  // 4-byte aligned
    int pixel_data_size = row_size * h;
    int file_size = 54 + pixel_data_size;

    // File header (14 bytes)
    uint8_t fh[14] = {
        'B','M',
        (uint8_t)(file_size),(uint8_t)(file_size>>8),(uint8_t)(file_size>>16),(uint8_t)(file_size>>24),
        0,0, 0,0,
        54,0,0,0
    };
    fwrite(fh, 1, 14, f);

    // Info header (40 bytes)
    uint8_t ih[40] = {0};
    *(int32_t*)(ih+0)  = 40;
    *(int32_t*)(ih+4)  = w;
    *(int32_t*)(ih+8)  = -h;  // negative = top-down
    *(int16_t*)(ih+12) = 1;
    *(int16_t*)(ih+14) = 24;  // bits per pixel
    fwrite(ih, 1, 40, f);

    // Pixel data
    uint8_t* row = (uint8_t*)alloca(row_size);
    for (int y = 0; y < h; y++) {
        memset(row, 0, row_size);
        for (int x = 0; x < w; x++) {
            uint8_t r, g, b;
            rgb565_to_888(pixels[y * w + x], &r, &g, &b);
            row[x*3+0] = b;  // BMP is BGR
            row[x*3+1] = g;
            row[x*3+2] = r;
        }
        fwrite(row, 1, row_size, f);
    }
    fclose(f);
}

// Minimal text drawing for the font tests: UTF-8, pen in 1/64 pixels
static void draw_text_test(IMAGE_BUFFER* buf, const FONT* font, int32_t x, int32_t baseline, const char* text, uint16_t color, bool kern, uint8_t alpha = 255)
{
    int32_t pen = x * 64;
    const FONT_GLYPH* prev = NULL;
    const uint8_t* p = (const uint8_t*)text;
    while (*p)
    {
        uint32_t cp = *p++;
        int extra = cp >= 0xF0 ? 3 : cp >= 0xE0 ? 2 : cp >= 0xC0 ? 1 : 0;
        cp &= extra == 3 ? 0x07 : extra == 2 ? 0x0F : extra == 1 ? 0x1F : 0x7F;
        while (extra-- > 0 && *p)
            cp = (cp << 6) | (*p++ & 0x3F);

        const FONT_GLYPH* g = font_get_glyph(font, cp);
        if (g == NULL)
            continue;
        if (kern)
            pen += font_get_kerning(font, prev, g);
        draw_alpha_mask(buf, ((pen + 32) >> 6) + g->left, baseline + g->top,
                        font_get_bitmap(font, g), g->width, g->width, g->height, color, alpha);
        pen += g->advance;
        prev = g;
    }
}

void* read_bin(const char* filename, size_t* psize = NULL)
{
    FILE* file = fopen(filename, "rb");
    if (!file) {
        perror("fopen");
        return NULL;
    }    
    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    fseek(file, 0, SEEK_SET);
    void* img = malloc(size);
    fread(img, size, 1, file);
    fclose(file);
    if (psize)
        *psize = size;
    return img;
}

// Blend helpers from rle565.c (not in the public header)
extern "C" uint16_t alpha_blend_premul_rgba8888_over_rgb565(uint16_t bg, uint32_t fg);
extern "C" uint16_t alpha_blend_premul_rgba8888_over_rgb565_with_alpha(uint16_t bg, uint32_t fg, uint8_t alpha);
extern "C" uint16_t alpha_blend_rgb565_over_rgb565_with_alpha(uint16_t bg, uint16_t fg, uint8_t alpha);

static int failures = 0;

static void check(bool ok, const char* name)
{
    printf("%s: %s\n", ok ? "PASS" : "FAIL", name);
    if (!ok)
        failures++;
}

int main()
{
    // Allocate buffer memory
    int width = 800;
    int height = 480;
    uint16_t* bufmem = (uint16_t*)malloc(width * height * sizeof(uint16_t));

    memset(bufmem, 0, width * height * sizeof(uint16_t));

    // Setup buffer
    IMAGE_BUFFER buf;
    init_image_buffer(&buf, 800, 480, bufmem, 800);

    // Read source images
    void* gradient = read_bin("./artwork/grey-gradient.rle565");
    void* square = read_bin("./artwork/square.rle565");
    void* transparent_square = read_bin("./artwork/transparent-square.rle565");
    void* circles = read_bin("./artwork/circles.rle565");
    void* red_circle = read_bin("./artwork/red-circle.rle565");

    // Draw it
    draw_image(&buf, gradient, 0, 0, 0, 0, get_image_width(gradient), get_image_height(gradient), NULL, 0xFF, NULL);
    write_bmp("build/01.bmp", buf.buffer, buf.base.width, buf.base.height);

    draw_image(&buf, square, 0, 0, 0, 0, get_image_width(square), get_image_height(square), NULL, 0xFF, NULL);
    write_bmp("build/02.bmp", buf.buffer, buf.base.width, buf.base.height);

    draw_image(&buf, gradient, 0, 0, 0, 0, get_image_width(gradient), get_image_height(gradient), NULL, 0xFF, NULL);
    draw_image(&buf, circles, 0, 0, 0, 0, get_image_width(circles), get_image_height(circles), NULL, 0xFF, NULL);
    write_bmp("build/03.bmp", buf.buffer, buf.base.width, buf.base.height);

    draw_image(&buf, gradient, 0, 0, 0, 0, get_image_width(gradient), get_image_height(gradient), NULL, 0xFF, NULL);
    draw_image(&buf, transparent_square, 0, 0, 0, 0, get_image_width(transparent_square), get_image_height(transparent_square), NULL, 0xFF, NULL);
    write_bmp("build/04.bmp", buf.buffer, buf.base.width, buf.base.height);

    draw_image(&buf, gradient, 0, 0, 0, 0, get_image_width(gradient), get_image_height(gradient), NULL, 0xFF, NULL);
    draw_image(&buf, circles, 0, 0, 0, 0, get_image_width(circles), get_image_height(circles), NULL, 0x30, NULL);
    write_bmp("build/05.bmp", buf.buffer, buf.base.width, buf.base.height);

    draw_image(&buf, gradient, 0, 0, 0, 0, get_image_width(gradient), get_image_height(gradient), NULL, 0xFF, NULL);
    draw_image(&buf, circles, 0, 0, 50, 50, get_image_width(circles), get_image_height(circles), NULL, 0x30, NULL);
    write_bmp("build/06.bmp", buf.buffer, buf.base.width, buf.base.height);

    draw_image(&buf, gradient, 0, 0, 0, 0, get_image_width(gradient), get_image_height(gradient), NULL, 0xFF, NULL);
    draw_image(&buf, circles, 0, 0, -50, -50, get_image_width(circles), get_image_height(circles), NULL, 0x30, NULL);
    write_bmp("build/07.bmp", buf.buffer, buf.base.width, buf.base.height);

    draw_image(&buf, gradient, 0, 0, 0, 0, get_image_width(gradient), get_image_height(gradient), NULL, 0xFF, NULL);
    draw_image(&buf, circles, 0, 0, 50, 50, 200, 200, NULL, 0x30, NULL);
    write_bmp("build/08.bmp", buf.buffer, buf.base.width, buf.base.height);

    draw_image(&buf, gradient, 0, 0, 0, 0, get_image_width(gradient), get_image_height(gradient), NULL, 0xFF, NULL);
    draw_image(&buf, square, 0, 0, 50, 50, 100, 100, NULL, 0x30, NULL);
    write_bmp("build/09.bmp", buf.buffer, buf.base.width, buf.base.height);

    draw_image(&buf, gradient, 0, 0, 0, 0, get_image_width(gradient), get_image_height(gradient), NULL, 0xFF, NULL);
    draw_image(&buf, circles, 50, 50, 0, 0, 300, 300, NULL, 0xFF, NULL);
    write_bmp("build/10.bmp", buf.buffer, buf.base.width, buf.base.height);

    draw_image(&buf, gradient, 0, 0, 0, 0, get_image_width(gradient), get_image_height(gradient), NULL, 0xFF, NULL);
    draw_image(&buf, circles, -50, -50, 0, 0, 300, 300, NULL, 0xFF, NULL);
    write_bmp("build/11.bmp", buf.buffer, buf.base.width, buf.base.height);

    const PALETTE* palette = get_image_palette(red_circle);
    PALETTE* palette2 = (PALETTE*)malloc(sizeof(PALETTE) + palette->count * sizeof(uint32_t));
    memcpy(palette2, palette, sizeof(PALETTE) + palette->count * sizeof(uint32_t));
    recolor_palette(palette2, 0x000000, 0xFF0000, 16);

    draw_image(&buf, gradient, 0, 0, 0, 0, get_image_width(gradient), get_image_height(gradient), NULL, 0xFF, NULL);
    draw_image(&buf, red_circle, 0, 0, 0, 0, 300, 300, NULL, 0xFF, palette2);
    write_bmp("build/12.bmp", buf.buffer, buf.base.width, buf.base.height);

    size_t capsize = get_capture_size(red_circle);
    void* cap = malloc(capsize);
    draw_image(&buf, gradient, 0, 0, 0, 0, get_image_width(gradient), get_image_height(gradient), NULL, 0xFF, NULL);
    draw_image(&buf, red_circle, 0, 0, 0, 0, 300, 300, cap, 0xFF, NULL);
    write_bmp("build/13a.bmp", buf.buffer, buf.base.width, buf.base.height);

    restore_image(&buf, cap);
    write_bmp("build/13b.bmp", buf.buffer, buf.base.width, buf.base.height);
    free(cap);

    // ---- Self-checking tests ----

    size_t bufBytes = width * height * sizeof(uint16_t);
    uint16_t* snapshot = (uint16_t*)malloc(bufBytes);

    // 14: Drawing into a clipped view near its right/bottom edges must not
    //     touch anything outside the view
    {
        for (int i = 0; i < width * height; i++) bufmem[i] = 0x1234;
        IMAGE_BUFFER view;
        clip_image_buffer(&view, &buf, 100, 100, 200, 200);
        draw_image(&view, circles, 150, 150, 0, 0, 300, 300, NULL, 0xFF, NULL);
        draw_image(&view, circles, -150, -150, 0, 0, 300, 300, NULL, 0x80, NULL);
        draw_image(&view, red_circle, 120, -20, 0, 0, 300, 300, NULL, 0xFF, NULL);
        write_bmp("build/14.bmp", buf.buffer, buf.base.width, buf.base.height);

        bool outsideOk = true, insideDrawn = false;
        for (int y = 0; y < height; y++)
        {
            for (int x = 0; x < width; x++)
            {
                bool inside = x >= 100 && x < 300 && y >= 100 && y < 300;
                if (!inside && bufmem[y * width + x] != 0x1234)
                    outsideOk = false;
                if (inside && bufmem[y * width + x] != 0x1234)
                    insideDrawn = true;
            }
        }
        check(outsideOk, "14: edge clipping stays inside the view");
        check(insideDrawn, "14: edge clipping still draws visible part");
    }

    // 15: Requesting only rows above the image's first stored row draws nothing
    {
        for (int i = 0; i < width * height; i++) bufmem[i] = 0x1234;
        draw_image(&buf, circles, 0, 0, 0, 0, 300, 20, NULL, 0xFF, NULL);   // circles firstRow = 25
        bool unchanged = true;
        for (int i = 0; i < width * height; i++)
            if (bufmem[i] != 0x1234) unchanged = false;
        check(unchanged, "15: rows above firstRow draw nothing");
    }

    // 16: Capture/restore round trips, including alpha blended and clipped
    //     draws, without writing past get_capture_size()
    {
        struct { const char* name; void* img; int dx, dy, sx, sy, w, h; uint8_t alpha; } cases[] = {
            { "16a: opaque image, alpha 0x80",       square,     0,   0,  0,  0, 300, 300, 0x80 },
            { "16b: opaque image, alpha 0x80, xclip", square,    0,   0, 70, 10, 150, 250, 0x80 },
            { "16c: alpha image, alpha 0xFF",         red_circle, 20, 30, 0,  0, 300, 300, 0xFF },
            { "16d: alpha image, alpha 0x40, xclip",  circles,   -40, 60, 30,  0, 200, 300, 0x40 },
            { "16e: offscreen draw",                  circles,  2000, 0,  0,  0, 300, 300, 0xFF },
        };
        for (auto& c : cases)
        {
            draw_image(&buf, gradient, 0, 0, 0, 0, 300, 300, NULL, 0xFF, NULL);
            draw_image(&buf, gradient, 300, 0, 0, 0, 300, 300, NULL, 0xFF, NULL);
            memcpy(snapshot, bufmem, bufBytes);

            size_t size = get_capture_size(c.img);
            uint8_t* capbuf = (uint8_t*)malloc(size + 256);
            memset(capbuf, 0xCD, size + 256);

            draw_image(&buf, c.img, c.dx, c.dy, c.sx, c.sy, c.w, c.h, capbuf, c.alpha, NULL);

            bool guardOk = true;
            for (size_t i = size; i < size + 256; i++)
                if (capbuf[i] != 0xCD) guardOk = false;

            restore_image(&buf, capbuf);
            bool restored = memcmp(snapshot, bufmem, bufBytes) == 0;

            char name[128];
            snprintf(name, sizeof(name), "%s (capture size)", c.name);
            check(guardOk, name);
            snprintf(name, sizeof(name), "%s (restore)", c.name);
            check(restored, name);
            free(capbuf);
        }
    }

    // 17: Capture where spans merge across rows into one run longer than
    //     65535 pixels (buffer stride == image width)
    {
        uint16_t* mem = (uint16_t*)malloc(300 * 300 * sizeof(uint16_t));
        uint16_t* copy = (uint16_t*)malloc(300 * 300 * sizeof(uint16_t));
        for (int i = 0; i < 300 * 300; i++) mem[i] = (uint16_t)(i * 7);
        memcpy(copy, mem, 300 * 300 * sizeof(uint16_t));

        IMAGE_BUFFER small;
        init_image_buffer(&small, 300, 300, mem, 300);

        size_t size = get_capture_size(gradient);
        uint8_t* capbuf = (uint8_t*)malloc(size + 256);
        memset(capbuf, 0xCD, size + 256);
        draw_image(&small, gradient, 0, 0, 0, 0, 300, 300, capbuf, 0xFF, NULL);

        bool guardOk = true;
        for (size_t i = size; i < size + 256; i++)
            if (capbuf[i] != 0xCD) guardOk = false;
        restore_image(&small, capbuf);

        check(guardOk, "17: long merged capture fits capture size");
        check(memcmp(copy, mem, 300 * 300 * sizeof(uint16_t)) == 0, "17: long merged capture restores");
        free(capbuf);
        free(mem);
        free(copy);
    }

    // 18: Decoding to ARGB8888 matches draw_image over black, and straight
    //     alpha output is written out for comparison against the PNGs
    {
        const char* names[] = { "grey-gradient", "square", "transparent-square", "circles", "red-circle" };
        for (const char* name : names)
        {
            char path[256];
            snprintf(path, sizeof(path), "./artwork/%s.rle565", name);
            size_t size;
            void* img = read_bin(path, &size);
            uint32_t w = get_image_width(img);
            uint32_t h = get_image_height(img);

            // Reference: draw over black
            uint16_t* ref = (uint16_t*)calloc(w * h, sizeof(uint16_t));
            IMAGE_BUFFER refbuf;
            init_image_buffer(&refbuf, w, h, ref, w);
            draw_image(&refbuf, img, 0, 0, 0, 0, w, h, NULL, 0xFF, NULL);

            // Premultiplied decode, rounded to 565 like the blender, should match exactly
            uint32_t* dec = (uint32_t*)malloc(w * h * sizeof(uint32_t));
            bool ok = decode_image_argb8888(img, size, NULL, dec, 0, true);
            bool match = ok;
            for (uint32_t i = 0; ok && i < w * h; i++)
            {
                uint32_t c = dec[i];
                uint16_t px = (uint16_t)(div255(((c >> 16) & 0xFF) * 31) << 11 | div255(((c >> 8) & 0xFF) * 63) << 5 | div255((c & 0xFF) * 31));
                if (px != ref[i])
                    match = false;
            }
            char msg[128];
            snprintf(msg, sizeof(msg), "18: decode %s matches draw_image", name);
            check(match, msg);

            // Straight alpha decode, saved as raw BGRA for inspection
            ok = decode_image_argb8888(img, size, NULL, dec, 0, false);
            snprintf(msg, sizeof(msg), "18: decode %s straight alpha", name);
            check(ok, msg);
            snprintf(path, sizeof(path), "build/%s.argb", name);
            FILE* f = fopen(path, "wb");
            fwrite(dec, sizeof(uint32_t), w * h, f);
            fclose(f);

            // Stride larger than width
            uint32_t* wide = (uint32_t*)malloc((w + 7) * h * sizeof(uint32_t));
            ok = decode_image_argb8888(img, size, NULL, wide, w + 7, false);
            bool strideOk = ok;
            for (uint32_t y = 0; ok && y < h; y++)
                if (memcmp(wide + y * (w + 7), dec + y * w, w * sizeof(uint32_t)) != 0)
                    strideOk = false;
            snprintf(msg, sizeof(msg), "18: decode %s with stride", name);
            check(strideOk, msg);

            free(wide);
            free(dec);
            free(ref);
            free(img);
        }
    }

    // 19: Truncated and corrupted data must fail cleanly (no crash)
    {
        size_t size;
        void* orig = read_bin("./artwork/circles.rle565", &size);
        uint32_t w = get_image_width(orig);
        uint32_t h = get_image_height(orig);
        uint32_t* dec = (uint32_t*)malloc(w * h * sizeof(uint32_t));

        bool truncOk = true;
        for (size_t len = 0; len < size; len++)
        {
            // Exact-size copy so any overread runs off the allocation
            uint8_t* copy = (uint8_t*)malloc(len ? len : 1);
            memcpy(copy, orig, len);
            if (len < sizeof(IMAGE_RLE) && decode_image_argb8888(copy, len, NULL, dec, 0, true))
                truncOk = false;
            else if (len >= sizeof(IMAGE_RLE))
                decode_image_argb8888(copy, len, NULL, dec, 0, true);
            free(copy);
        }
        check(truncOk && !decode_image_argb8888(orig, size - 1, NULL, dec, 0, true), "19: truncated data fails cleanly");

        uint8_t* copy = (uint8_t*)malloc(size);
        srand(1234);
        int rejected = 0;
        for (int iter = 0; iter < 20000; iter++)
        {
            memcpy(copy, orig, size);
            int flips = 1 + rand() % 8;
            for (int i = 0; i < flips; i++)
                copy[((size_t)rand() * RAND_MAX + rand()) % size] = (uint8_t)rand();

            // Keep dimensions sane so the output buffer is big enough
            ((IMAGE*)copy)->width = w;
            ((IMAGE*)copy)->height = h;
            if (!decode_image_argb8888(copy, size, NULL, dec, 0, rand() & 1))
                rejected++;
        }
        printf("      (corruption: %d of 20000 rejected)\n", rejected);
        check(true, "19: corrupted data doesn't crash");
        free(copy);
        free(dec);
        free(orig);
    }

    // 20: Header checks
    {
        size_t size;
        uint8_t* orig = (uint8_t*)read_bin("./artwork/red-circle.rle565", &size);
        uint8_t* copy = (uint8_t*)malloc(size + 100);
        uint32_t* dec = (uint32_t*)malloc(300 * 300 * sizeof(uint32_t));

        check(check_image_header(orig, size), "20: valid header accepted");
        check(memcmp(orig, "R565", 4) == 0, "20: file starts with R565");

        memcpy(copy, orig, size);
        copy[0] = 'X';
        check(!check_image_header(copy, size), "20: bad signature rejected");
        check(!decode_image_argb8888(copy, size, NULL, dec, 0, true), "20: decode rejects bad signature");

        memcpy(copy, orig, size);
        ((IMAGE*)copy)->format = IMAGE_FORMAT_BUFFER;
        check(!check_image_header(copy, size), "20: wrong format rejected");

        memcpy(copy, orig, size);
        ((IMAGE_RLE*)copy)->packedSize = (uint32_t)size + 1;
        check(!check_image_header(copy, size), "20: packedSize beyond data rejected");

        memcpy(copy, orig, size);
        ((IMAGE_RLE*)copy)->paletteOffset = (uint32_t)size - 2;
        check(!check_image_header(copy, size), "20: palette beyond data rejected");

        memcpy(copy, orig, size);
        ((IMAGE_RLE*)copy)->firstRow = 10;   // 10 + 292 rows > 300 high
        check(!check_image_header(copy, size), "20: rows beyond height rejected");

        // Trailing data (eg: image inside a blob pack) is fine
        memcpy(copy, orig, size);
        memset(copy + size, 0xEE, 100);
        check(check_image_header(copy, size + 100) && decode_image_argb8888(copy, size + 100, NULL, dec, 0, true),
              "20: trailing data accepted");

        free(dec);
        free(copy);
        free(orig);
    }

    // 21: Color conversion and blending precision
    {
        // 565 -> 888 -> 565 round trips, and full intensity expands to 0xFF
        bool roundTrip = rgb565_to_rgb888(0xFFFF) == 0xFFFFFF;
        for (uint32_t c = 0; c < 0x10000; c++)
            if (rgb888_to_rgb565(rgb565_to_rgb888(c)) != c)
                roundTrip = false;
        check(roundTrip, "21: rgb565 <-> rgb888 round trip");

        // Blending a color over itself leaves it unchanged
        bool identity = true;
        const uint8_t alphas[] = { 1, 64, 128, 200, 254 };
        for (uint32_t c = 0; c < 0x10000; c++)
            for (uint8_t a : alphas)
                if (alpha_blend_rgb565_over_rgb565_with_alpha((uint16_t)c, (uint16_t)c, a) != c)
                    identity = false;
        check(identity, "21: rgb565 blended over itself is unchanged");

        // Nearly transparent pixels don't darken the background
        check(alpha_blend_premul_rgba8888_over_rgb565(0xFFFF, 0x01000000) == 0xFFFF, "21: alpha 1 black over white stays white");
        check(alpha_blend_premul_rgba8888_over_rgb565_with_alpha(0xFFFF, 0x80000000, 2) == 0xFFFF, "21: faded black over white stays white");

        // Premultiplied white at any alpha over white stays white
        bool whiteOk = true;
        for (uint32_t a = 1; a < 256; a++)
            if (alpha_blend_premul_rgba8888_over_rgb565(0xFFFF, (a << 24) | (a << 16) | (a << 8) | a) != 0xFFFF)
                whiteOk = false;
        check(whiteOk, "21: premultiplied white over white stays white");

        // 50% red over blue
        uint16_t mix = alpha_blend_rgb565_over_rgb565_with_alpha(0x001F, 0xF800, 128);
        check(mix == ((16 << 11) | 15), "21: 50% red over blue");

        // normalize + prepare leaves a palette unchanged
        const uint32_t colors[] = { 0, 0xFF00FFFF, 0xFF000000, 0xFF00F800, 0xFF0007E0, 0xFF00001F, 0xFF001234, 0x80808080, 0x80400020, 0x10101010, 0xFE7F3F1F };
        uint32_t n = sizeof(colors) / sizeof(colors[0]);
        PALETTE* pal = (PALETTE*)malloc(sizeof(PALETTE) + n * sizeof(uint32_t));
        pal->count = n;
        memcpy(pal->colors, colors, sizeof(colors));
        normalize_palette(pal);
        bool whiteNormalized = pal->colors[1] == 0xFFFFFFFF;
        prepare_palette(pal);
        check(memcmp(pal->colors, colors, sizeof(colors)) == 0, "21: normalize + prepare round trips palette");
        check(whiteNormalized, "21: normalize expands white to 0xFFFFFFFF");
        free(pal);

        // premultiply matches the encoder's rounding: (x + 128) * 257 >> 16
        bool premulOk = true;
        for (uint32_t a = 1; a < 256; a++)
            for (uint32_t v = 0; v < 256; v++)
                if (((premultiply_argb8888((a << 24) | (v << 16)) >> 16) & 0xFF) != (((v * a + 128) * 257) >> 16))
                    premulOk = false;
        check(premulOk, "21: premultiply matches encoder");
    }

    // 22: fill_rect clips to the buffer (and to clip views)
    {
        const uint32_t W = 20, H = 15, STRIDE = 24;
        const uint16_t BG = 0x1234, FG = 0xF81F;
        uint16_t mem[STRIDE * H];

        // Fill a buffer with BG, fill_rect with FG, then compare against
        // a rectangle intersected by hand. Padding past W must stay untouched.
        struct { int32_t x, y; uint32_t w, h; } cases[] = {
            { 3, 4, 5, 6 },                     // inside
            { -5, -3, 10, 8 },                  // off top left
            { 15, 10, 100, 100 },               // off bottom right
            { -100, -100, 1000, 1000 },         // covers everything
            { 25, 2, 5, 5 },                    // entirely right
            { 2, -20, 5, 5 },                   // entirely above
            { 3, 3, 0, 5 },                     // empty
            { INT32_MAX, INT32_MAX, UINT32_MAX, UINT32_MAX },   // would wrap in 32-bit
            { INT32_MIN, 0, UINT32_MAX, 1 },    // spans the whole row
        };
        bool ok = true;
        for (auto& c : cases)
        {
            for (auto& p : mem) p = BG;
            IMAGE_BUFFER buf;
            init_image_buffer(&buf, W, H, mem, STRIDE);
            fill_rect(&buf, c.x, c.y, c.w, c.h, FG);

            int64_t x0 = c.x, y0 = c.y, x1 = x0 + c.w, y1 = y0 + c.h;
            for (uint32_t y = 0; y < H; y++)
                for (uint32_t x = 0; x < STRIDE; x++)
                {
                    bool in = x < W && x >= x0 && x < x1 && y >= y0 && y < y1;
                    if (mem[y * STRIDE + x] != (in ? FG : BG))
                        ok = false;
                }
        }
        check(ok, "22: fill_rect clips to buffer");

        // Through a clip view, only the view's rectangle is touched
        for (auto& p : mem) p = BG;
        IMAGE_BUFFER buf, view;
        init_image_buffer(&buf, W, H, mem, STRIDE);
        clip_image_buffer(&view, &buf, 4, 5, 6, 3);
        fill_rect(&view, -10, -10, 100, 100, FG);
        bool viewOk = true;
        for (uint32_t y = 0; y < H; y++)
            for (uint32_t x = 0; x < STRIDE; x++)
            {
                bool in = x >= 4 && x < 10 && y >= 5 && y < 8;
                if (mem[y * STRIDE + x] != (in ? FG : BG))
                    viewOk = false;
            }
        check(viewOk, "22: fill_rect clips to clip view");
    }

    // 23: draw_alpha_mask
    {
        const uint32_t W = 20, H = 15, STRIDE = 24;
        const uint16_t BG = 0x1234, FG = 0xF81F;
        uint16_t mem[STRIDE * H];

        // Mask of 0/255 values in a pattern, with a stride wider than the
        // mask and a width that isn't a multiple of 4, so placement and
        // clipping can be checked exactly
        const uint32_t MW = 13, MH = 9, MSTRIDE = 17;
        uint8_t mask[MSTRIDE * MH];
        for (uint32_t i = 0; i < sizeof(mask); i++)
            mask[i] = 0x5A;     // junk past the mask width must be ignored
        for (uint32_t y = 0; y < MH; y++)
            for (uint32_t x = 0; x < MW; x++)
                mask[y * MSTRIDE + x] = ((x + 2 * y) % 3 == 0 || y == 4) ? 255 : 0;

        struct { int32_t x, y; } positions[] = {
            { 3, 2 }, { -5, -4 }, { 12, 10 }, { -12, 3 }, { 19, 14 },
            { 20, 0 }, { 0, -9 }, { INT32_MIN, INT32_MIN }, { INT32_MAX, INT32_MAX },
        };
        bool placeOk = true;
        for (auto& p : positions)
        {
            for (auto& px : mem) px = BG;
            IMAGE_BUFFER buf;
            init_image_buffer(&buf, W, H, mem, STRIDE);
            draw_alpha_mask(&buf, p.x, p.y, mask, MSTRIDE, MW, MH, FG, 255);

            for (uint32_t y = 0; y < H; y++)
                for (uint32_t x = 0; x < STRIDE; x++)
                {
                    int64_t mx = (int64_t)x - p.x, my = (int64_t)y - p.y;
                    bool in = x < W && mx >= 0 && mx < MW && my >= 0 && my < MH;
                    uint16_t expect = in && mask[my * MSTRIDE + mx] == 255 ? FG : BG;
                    if (mem[y * STRIDE + x] != expect)
                        placeOk = false;
                }
        }
        check(placeOk, "23: alpha mask placement and clipping");

        // Through a clip view
        {
            for (auto& px : mem) px = BG;
            IMAGE_BUFFER buf, view;
            init_image_buffer(&buf, W, H, mem, STRIDE);
            clip_image_buffer(&view, &buf, 4, 5, 6, 3);
            uint8_t solid[40 * 40];
            memset(solid, 255, sizeof(solid));
            draw_alpha_mask(&view, -10, -10, solid, 40, 40, 40, FG, 255);
            bool viewOk = true;
            for (uint32_t y = 0; y < H; y++)
                for (uint32_t x = 0; x < STRIDE; x++)
                {
                    bool in = x >= 4 && x < 10 && y >= 5 && y < 8;
                    if (mem[y * STRIDE + x] != (in ? FG : BG))
                        viewOk = false;
                }
            check(viewOk, "23: alpha mask clips to clip view");
        }

        // Alpha 0 draws nothing
        {
            for (auto& px : mem) px = BG;
            IMAGE_BUFFER buf;
            init_image_buffer(&buf, W, H, mem, STRIDE);
            draw_alpha_mask(&buf, 0, 0, mask, MSTRIDE, MW, MH, FG, 0);
            bool none = true;
            for (auto& px : mem) if (px != BG) none = false;
            check(none, "23: alpha 0 draws nothing");
        }

        // Blending accuracy against an exact blend, for every coverage and
        // a range of global alphas and colors
        {
            const uint16_t colors[] = { 0x0000, 0xFFFF, 0xF800, 0x07E0, 0x001F, 0x1234, 0x8410, 0xA5C3 };
            const uint8_t alphas[] = { 255, 254, 200, 128, 37, 1 };
            uint8_t ramp[256];
            for (int i = 0; i < 256; i++) ramp[i] = (uint8_t)i;
            uint16_t row[256];
            double maxErr = 0;
            bool endsExact = true;
            for (uint16_t bg : colors)
                for (uint16_t fg : colors)
                    for (uint8_t alpha : alphas)
                    {
                        for (auto& px : row) px = bg;
                        IMAGE_BUFFER buf;
                        init_image_buffer(&buf, 256, 1, row, 0);
                        draw_alpha_mask(&buf, 0, 0, ramp, 256, 256, 1, fg, alpha);
                        for (int m = 0; m < 256; m++)
                        {
                            double a = m * alpha / (255.0 * 255.0);
                            const int shifts[] = { 11, 5, 0 };
                            const int maxes[] = { 31, 63, 31 };
                            for (int c = 0; c < 3; c++)
                            {
                                double b = (bg >> shifts[c]) & maxes[c];
                                double f = (fg >> shifts[c]) & maxes[c];
                                double got = (row[m] >> shifts[c]) & maxes[c];
                                double err = got - (b + (f - b) * a);
                                if (err < 0) err = -err;
                                if (err > maxErr) maxErr = err;
                            }
                        }
                        if (row[0] != bg || (alpha == 255 && row[255] != fg))
                            endsExact = false;
                    }
            char msg[100];
            snprintf(msg, sizeof(msg), "23: alpha mask blend within 1.5 steps of exact (max %.2f)", maxErr);
            check(maxErr <= 1.5, msg);
            check(endsExact, "23: coverage 0 and 255 are exact");
        }
    }

    // 24: Fonts
    {
        size_t size;
        uint8_t* data = (uint8_t*)read_bin("./artwork/opensans-20.rfnt", &size);
        const FONT* font = (const FONT*)data;
        check(check_font(font, size), "24: font is valid");

        // Every codepoint finds its own glyph or nothing, and every glyph is found
        const FONT_GLYPH* glyphs = (const FONT_GLYPH*)(data + font->glyphsOffset);
        uint32_t found = 0;
        bool lookupOk = true;
        for (uint32_t cp = 0; cp < 0x30000; cp++)
        {
            const FONT_GLYPH* g = font_find_glyph(font, cp);
            if (g == NULL)
                continue;
            if (g->codepoint != cp)
                lookupOk = false;
            found++;
        }
        for (uint32_t i = 0; i < font->glyphCount; i++)
            if (font_find_glyph(font, glyphs[i].codepoint) != &glyphs[i])
                lookupOk = false;
        check(lookupOk && found == font->glyphCount, "24: hash lookup finds every glyph");
        check(font_find_glyph(font, 0x20AC) != NULL && font_find_glyph(font, 0xE9) != NULL, "24: non-ASCII glyphs found");
        check(font_find_glyph(font, 0x65E5) == NULL && font_get_glyph(font, 0x65E5) == font_find_glyph(font, '?'),
              "24: missing glyph falls back to '?'");

        // Kerning (values from the msdf-atlas-gen JSON, in 1/64 px)
        const FONT_GLYPH* A = font_find_glyph(font, 'A');
        const FONT_GLYPH* V = font_find_glyph(font, 'V');
        const FONT_GLYPH* T = font_find_glyph(font, 'T');
        const FONT_GLYPH* o = font_find_glyph(font, 'o');
        check(font_get_kerning(font, A, V) == -51 && font_get_kerning(font, T, o) == -89 && font_get_kerning(font, T, T) == 26,
              "24: kerning pairs");
        check(font_get_kerning(font, o, o) == 0 && font_get_kerning(font, NULL, A) == 0 && font_get_kerning(font, A, NULL) == 0,
              "24: unkerned pairs are 0");
        bool allPairs = true;
        const FONT_KERNING* pairs = (const FONT_KERNING*)(data + font->kerningOffset);
        for (uint32_t i = 0; i < font->glyphCount; i++)
            for (uint32_t k = 0; k < glyphs[i].kerningCount; k++)
            {
                const FONT_KERNING* p = &pairs[glyphs[i].kerning + k];
                if (font_get_kerning(font, &glyphs[i], &glyphs[p->right]) != p->adjust)
                    allPairs = false;
            }
        check(allPairs && font->kerningCount > 0, "24: every kerning pair found");

        // Bitmaps are trimmed: each edge row and column has some coverage
        bool trimmed = true;
        for (uint32_t i = 0; i < font->glyphCount; i++)
        {
            const FONT_GLYPH* g = &glyphs[i];
            if (g->width == 0)
                continue;
            const uint8_t* bm = font_get_bitmap(font, g);
            bool top = false, bottom = false, left = false, right = false;
            for (uint32_t x = 0; x < g->width; x++)
            {
                top |= bm[x] != 0;
                bottom |= bm[(g->height - 1) * g->width + x] != 0;
            }
            for (uint32_t y = 0; y < g->height; y++)
            {
                left |= bm[y * g->width] != 0;
                right |= bm[y * g->width + g->width - 1] != 0;
            }
            if (!(top && bottom && left && right))
                trimmed = false;
        }
        check(trimmed, "24: glyph bitmaps are trimmed");
        check(A->top + A->height == 0 && font->ascent > A->height, "24: 'A' sits on the baseline");

        // Damaged fonts are rejected
        uint8_t* copy = (uint8_t*)malloc(size);
        auto reset = [&]() { memcpy(copy, data, size); return (FONT*)copy; };
        check(!check_font(data, size - 1), "24: truncated font rejected");
        reset()->signature ^= 1;
        check(!check_font(copy, size), "24: bad signature rejected");
        reset()->hashBits = 0;
        check(!check_font(copy, size), "24: zero hashBits rejected");
        reset()->hashBits = 18;
        check(!check_font(copy, size), "24: large hashBits rejected");
        {
            FONT* f = reset();
            ((FONT_GLYPH*)(copy + f->glyphsOffset))[10].height = 0xFFFF;
            check(!check_font(copy, size), "24: glyph bitmap beyond data rejected");
        }
        {
            FONT* f = reset();
            ((FONT_KERNING*)(copy + f->kerningOffset))[0].right = f->glyphCount;
            check(!check_font(copy, size), "24: bad kerning glyph rejected");
        }
        {
            FONT* f = reset();
            uint16_t* table = (uint16_t*)(copy + f->hashOffset);
            for (uint32_t i = 0; i < (1u << f->hashBits); i++)
                if (table[i] == FONT_NO_GLYPH) { table[i] = f->glyphCount; break; }
            check(!check_font(copy, size), "24: bad hash entry rejected");
        }
        {
            // A full table (no empty slots) still can't loop forever
            FONT* f = reset();
            uint16_t* table = (uint16_t*)(copy + f->hashOffset);
            for (uint32_t i = 0; i < (1u << f->hashBits); i++)
                if (table[i] == FONT_NO_GLYPH) table[i] = 0;
            check(check_font(copy, size) && font_find_glyph(f, 0x65E5) == NULL, "24: full hash table lookup terminates");
        }

        // Random damage: anything check_font accepts must be safe to use
        srand(1234);
        for (int iter = 0; iter < 2000; iter++)
        {
            FONT* f = reset();
            for (int n = 0; n < 4; n++)
                copy[rand() % size] = (uint8_t)rand();
            if (!check_font(copy, size))
                continue;
            volatile uint32_t sink = 0;
            for (uint32_t cp = 0; cp < 0x200; cp++)
            {
                const FONT_GLYPH* g = font_get_glyph(f, cp);
                if (!g) continue;
                const uint8_t* bm = font_get_bitmap(f, g);
                for (uint32_t i = 0; i < (uint32_t)g->width * g->height; i++)
                    sink += bm[i];
                sink += font_get_kerning(f, g, font_find_glyph(f, 'o'));
            }
        }
        check(true, "24: damaged fonts don't crash");
        free(copy);

        // Measuring
        {
            auto adv = [&](const char* s) { int32_t pen = 0; const FONT_GLYPH* prev = NULL;
                for (; *s; s++) { const FONT_GLYPH* g = font_get_glyph(font, (uint8_t)*s); pen += font_get_kerning(font, prev, g) + g->advance; prev = g; }
                return (pen + 32) >> 6; };
            const FONT_GLYPH* q = font_find_glyph(font, '?');
            const FONT_GLYPH* e = font_find_glyph(font, 0xE9);
            const FONT_GLYPH* euro = font_find_glyph(font, 0x20AC);

            check(font_measure_text(font, "", -1) == 0 && font_measure_text(font, "AVATAR", 0) == 0, "24: measure empty");
            check(font_measure_text(font, "AVATAR Toyota", -1) == adv("AVATAR Toyota"), "24: measure includes kerning");
            check(font_measure_text(font, "AV", -1) == ((A->advance + V->advance - 51 + 32) >> 6), "24: measure AV");
            check(font_measure_text(font, "AVATAR", 2) == font_measure_text(font, "AV", -1), "24: measure with length");
            check(font_measure_text(font, "\xC3\xA9\xE2\x82\xAC", -1) == ((e->advance + euro->advance + 32) >> 6), "24: measure UTF-8");
            check(font_measure_text(font, "\xE6\x97\xA5", -1) == ((q->advance + 32) >> 6), "24: measure missing char uses default");

            // Invalid UTF-8 measures as U+FFFD, which this font draws as '?'
            int32_t one = (q->advance + 32) >> 6;
            check(font_measure_text(font, "\xFF", -1) == one, "24: measure invalid byte");
            check(font_measure_text(font, "\xE2\x82", -1) == one, "24: measure truncated sequence");
            check(font_measure_text(font, "\xC0\xAF", -1) == one, "24: measure overlong sequence");
            check(font_measure_text(font, "\xED\xA0\x80", -1) == one, "24: measure surrogate");
            check(font_measure_text(font, "\xE2\x82" "A", -1) == ((q->advance + A->advance + 32) >> 6), "24: measure resyncs after bad sequence");
        }

        // Render some text to look at
        const uint32_t W = 420, H = 220;
        uint16_t* mem = (uint16_t*)malloc(W * H * sizeof(uint16_t));
        IMAGE_BUFFER buf;
        init_image_buffer(&buf, W, H, mem, W);
        fill_rect(&buf, 0, 0, W, H, 0xFFFF);
        fill_rect(&buf, 0, 150, W, H - 150, 0x18E3);

        const uint32_t lh = font->lineHeight;
        int32_t y = font->ascent + 4;
        draw_text_test(&buf, font, 8, y, "AVATAR Toyota Wave. (kerned)", 0x0000, true);
        fill_rect(&buf, 8, y + 2, font_measure_text(font, "AVATAR Toyota Wave. (kerned)", -1), 1, 0x07E0);   y += lh;
        draw_text_test(&buf, font, 8, y, "AVATAR Toyota Wave. (not kerned)", 0x0000, false); y += lh;
        draw_text_test(&buf, font, 8, y, "The quick brown fox jumps over the lazy dog", 0x001F, true); y += lh;
        draw_text_test(&buf, font, 8, y, "Caf\xC3\xA9 \xE2\x82\xAC" "12 21\xC2\xB0 missing:\xE6\x97\xA5", 0xF800, true); y += lh;
        draw_text_test(&buf, font, 8, y, "Faded text at alpha 96", 0x0000, true, 96);

        // Clipped: a view across the middle of a line, and text on dark
        IMAGE_BUFFER view;
        clip_image_buffer(&view, &buf, 60, 160, 200, 14);
        fill_rect(&view, 0, 0, 200, 14, 0x4208);
        draw_text_test(&view, font, -20, 14, "Clipped to a view on all sides!", 0xFFE0, true);
        draw_text_test(&buf, font, 8, 210, "Light on dark 0123456789", 0xFFFF, true);
        write_bmp("build/font.bmp", mem, W, H);
        free(mem);
        free(data);
    }

    printf(failures ? "\n%d FAILED\n" : "\nAll checks passed\n", failures);
    return failures ? 1 : 0;
}