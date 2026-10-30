#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <assert.h>
#pragma warning(disable: 4200)
#include "../src/rle565.h"

void rgb565_to_888(uint16_t px, uint8_t *r, uint8_t *g, uint8_t *b) {
    *r = ((px >> 11) & 0x1F) << 3;
    *g = ((px >> 5)  & 0x3F) << 2;
    *b = ((px >> 0)  & 0x1F) << 3;
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

void* read_bin(const char* filename)
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
    return img;
}

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
    void* gradient = read_bin("./artwork/grey-gradient.bin");
    void* square = read_bin("./artwork/square.bin");
    void* transparent_square = read_bin("./artwork/transparent-square.bin");
    void* circles = read_bin("./artwork/circles.bin");
    void* red_circle = read_bin("./artwork/red-circle.bin");

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

    printf(failures ? "\n%d FAILED\n" : "\nAll checks passed\n", failures);
    return failures ? 1 : 0;
}