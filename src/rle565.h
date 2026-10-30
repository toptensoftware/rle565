#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Base image format
#define IMAGE_FORMAT_RLE                   1     // Run length encoded
#define IMAGE_FORMAT_BUFFER                2     // Flat buffer

// Flags used to select appropriately optimized drawing function
#define DRAW_FLAGS_ALPHA_PIXELS     0x01        // Some pixels require blending
#define DRAW_FLAGS_LARGE_PALETTE    0x02        // Palette has more than 256 entries
#define DRAW_FLAGS_ALPHA_BLEND      0x04        // Alpha component needs blending
#define DRAW_FLAGS_XCLIP            0x08        // Needs clipping in x-direction
#define DRAW_FLAGS_CAPTURE          0x10        // Capture pixels as drawn

// Helpers to pack/unpack rgb565 <-> rgb888
#define rgb565_to_rgb888(x) (((x & 0xF800) << 8) | ((x & 0x07E0) << 5) |  ((x & 0x001F) << 3))
#define rgb888_to_rgb565(x) (((x & 0xF80000) >> 8) | ((x & 0xFC00) >> 5) | ((x & 0xF8) >> 3))

// Helpers to premultiply/unpremultiply alpha for ARGB8888 colors
uint32_t unpremultiply_argb8888(uint32_t c);
uint32_t premultiply_argb888(uint32_t c);

#pragma pack(push, 1)

typedef struct PALETTE PALETTE;
typedef struct IMAGE IMAGE;
typedef struct IMAGE_RLE IMAGE_RLE;
typedef struct IMAGE_BUFFER IMAGE_BUFFER;

struct PALETTE
{
    uint32_t count;
    uint32_t colors[];
};

struct IMAGE
{
    uint32_t format;
    uint32_t width;
    uint32_t height;
    uint32_t flags;
};

struct IMAGE_RLE
{
    IMAGE base;                     // Base image information
    uint32_t packedSize;            // Entire packed size of this image
    uint32_t captureSize;           // Max bytes required to capture this image
    uint32_t paletteOffset;         // Offset to the start of the palette, from the start of the header  
    uint32_t firstRow;              // Y-coordinate of first row in the row table
    uint32_t rowCount;              // Number of rows in the row table

    // uint32_t rowOffsets[rowCount];   // Offset to the start of each row's run data, from the start of the header
    // PALETTE palette;                 // Palette count + entries 
    // rowdata rows[rowCount]
};

// rowOffsets are offsets to the start of each row's run data, from the 
//    start of the header

// Palette entries:
//   * transparent are 0xAARRGGBB
//   * opaque 0xFF00[rgb565]

// Row format is:
//   * varlen run count
//   * runs[runCount]

// Run format:
//   * varlen [pixelCount:n][rle:1]
//   * palette indicies (if non-RLE) or single palette index (if RLE)

// Run format when DRAW_FLAGS_ALPHA_PIXELS is set in header.flags and rle is not set.
// then pixelCount has an extra alpha flag bit.  If not set, all pixels in the run are
// opaque. 
//   * varlen [pixelCount:n][alpha:1][!rle:1]
//   * palette indicies (if non-RLE) or single palette index (if RLE)

// If palette count is <= 256 then palette indices are one byte
// otherwise they are varlen encoded.

struct IMAGE_BUFFER
{
    IMAGE base;
    uint16_t* buffer;
    uint32_t stride;
    void* user;
};

#pragma pack(pop)

// Packed images
uint32_t get_image_packed_size(const void* image);
size_t get_capture_size(const void* image);
const PALETTE* get_image_palette(const void* image);

// Attributes
uint32_t get_image_width(const void* image);
uint32_t get_image_height(const void* image);

// Drawing
void draw_image(IMAGE_BUFFER* buffer, const void* image, int32_t dest_x, int32_t dest_y, int32_t src_x, int32_t src_y, uint32_t width, uint32_t height, void* cap, uint8_t alpha, const PALETTE* palette);
void restore_image(IMAGE_BUFFER* buffer, const void* capture);

// Initialize a buffered image
void init_image_buffer(IMAGE_BUFFER* buffer, uint32_t width, uint32_t height, uint16_t* mem, uint32_t stride);
void clip_image_buffer(IMAGE_BUFFER* buffer, IMAGE_BUFFER* underlying, uint32_t x, uint32_t y, uint32_t width, uint32_t height);

// Palette manipulation
// Normalize a palette to ARGB8888 format, converting any opaque 
// RGB565 entries to RGB888
void normalize_palette(PALETTE* palette);

// Prepare a palette for drawing, converting any opaque ARGB8888 
// entries to RGB565 prepared for drawing
void prepare_palette(PALETTE* palette);

// Recolor a palette using one channel from the original palette
// to interpolate between two new colors.  channelShift is the 
// bit shift to get the channel to use for interpolation.
void recolor_palette(PALETTE* palette, uint32_t colorA, uint32_t colorB, int channelShift);

#ifdef __cplusplus
}
#endif
