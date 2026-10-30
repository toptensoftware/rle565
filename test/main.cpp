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

void main()
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
    draw_image(&buf, circles, 0, 0, 0, 0, get_image_width(transparent_square), get_image_height(transparent_square), NULL, 0x30, NULL);
    write_bmp("build/05.bmp", buf.buffer, buf.base.width, buf.base.height);

    draw_image(&buf, gradient, 0, 0, 0, 0, get_image_width(gradient), get_image_height(gradient), NULL, 0xFF, NULL);
    draw_image(&buf, circles, 0, 0, 50, 50, get_image_width(transparent_square), get_image_height(transparent_square), NULL, 0x30, NULL);
    write_bmp("build/06.bmp", buf.buffer, buf.base.width, buf.base.height);

    draw_image(&buf, gradient, 0, 0, 0, 0, get_image_width(gradient), get_image_height(gradient), NULL, 0xFF, NULL);
    draw_image(&buf, circles, 0, 0, -50, -50, get_image_width(transparent_square), get_image_height(transparent_square), NULL, 0x30, NULL);
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
}