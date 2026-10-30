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

    printf(failures ? "\n%d FAILED\n" : "\nAll checks passed\n", failures);
    return failures ? 1 : 0;
}