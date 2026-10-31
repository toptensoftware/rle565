#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Signature at the start of every image: the bytes "R565" (little endian)
#define IMAGE_SIGNATURE                    0x35363552

// Base image format
#define IMAGE_FORMAT_RLE                  1     // Run length encoded
#define IMAGE_FORMAT_BUFFER                2     // Flat buffer

// Flags used to select appropriately optimized drawing function
#define DRAW_FLAGS_ALPHA_PIXELS     0x01        // Some pixels require blending
#define DRAW_FLAGS_LARGE_PALETTE    0x02        // Palette has more than 256 entries
#define DRAW_FLAGS_ALPHA_BLEND      0x04        // Alpha component needs blending
#define DRAW_FLAGS_XCLIP            0x08        // Needs clipping in x-direction
#define DRAW_FLAGS_CAPTURE          0x10        // Capture pixels as drawn

// Helpers to pack/unpack rgb565 <-> rgb888 (other bits of x are ignored).
// Unpacking replicates the top bits into the low bits so full intensity maps
// to 0xFF; packing truncates, so pack(unpack(x)) == x.
#define rgb565_to_rgb888(x) ( (((x) & 0xF800) << 8) | (((x) & 0xE000) << 3) \
                            | (((x) & 0x07E0) << 5) | (((x) & 0x0600) >> 1) \
                            | (((x) & 0x001F) << 3) | (((x) & 0x001C) >> 2) )
#define rgb888_to_rgb565(x) ( (((x) & 0xF80000) >> 8) | (((x) & 0xFC00) >> 5) | (((x) & 0xF8) >> 3) )

// Helpers to premultiply/unpremultiply alpha for ARGB8888 colors
uint32_t unpremultiply_argb8888(uint32_t c);
uint32_t premultiply_argb8888(uint32_t c);

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
    uint32_t signature;             // IMAGE_SIGNATURE ("R565")
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

// Draw an 8-bit alpha mask (eg: a font glyph) in a solid RGB565 color.
//
//   buffer          destination buffer or clip view
//   dest_x, dest_y  where the mask's top left corner lands (may be negative)
//   mask            8-bit coverage values: 0 = transparent, 255 = solid
//   mask_stride     bytes from one mask row to the next
//   width, height   size of the mask
//   color           RGB565 color to draw
//   alpha           overall opacity, 0-255 (255 = mask coverage only)
//
// Clipped to the buffer.  To draw part of a mask, offset the mask pointer
// and reduce width/height.  Optimized for speed: coverage is blended at 33
// levels, so partially covered pixels can be off by up to 1.5 RGB565 steps.
void draw_alpha_mask(IMAGE_BUFFER* buffer, int32_t dest_x, int32_t dest_y,
                     const uint8_t* mask, uint32_t mask_stride,
                     uint32_t width, uint32_t height,
                     uint16_t color, uint8_t alpha);

// Validation
// Check that image_size bytes at image hold a well formed RLE image header:
// signature, format, row table and (if present) palette all fit, and the
// stored rows are within the image.  Doesn't validate the row data itself.
// Use before trusting anything read from an untrusted file.
bool check_image_header(const void* image, size_t image_size);

// Decoding
// Decode an entire image to 32-bit 0xAARRGGBB pixels (BGRA byte order in
// memory, as used by WIC's 32bppBGRA / 32bppPBGRA formats).  Not composited
// and not optimized - intended for previews and tooling.
//
//   image_size     size of the image data in bytes; everything read is
//                  bounds checked against it so corrupt files fail safely
//   palette        palette to use, or NULL for the image's own palette
//   out            receives width x height pixels
//   stride         output row stride in pixels (0 = image width)
//   premultiplied  true for premultiplied alpha output, false for straight
//
// Returns false if the image data is invalid (out may be partially written).
// Calls check_image_header() itself.
bool decode_image_argb8888(const void* image, size_t image_size, const PALETTE* palette, uint32_t* out, uint32_t stride, bool premultiplied);

// Initialize a buffered image
void init_image_buffer(IMAGE_BUFFER* buffer, uint32_t width, uint32_t height, uint16_t* mem, uint32_t stride);
void clip_image_buffer(IMAGE_BUFFER* buffer, IMAGE_BUFFER* underlying, uint32_t x, uint32_t y, uint32_t width, uint32_t height);

// Fill a rectangle of a buffer with an RGB565 color.  The rectangle is
// clipped to the buffer, so it may be partly or entirely off-buffer.
void fill_rect(IMAGE_BUFFER* buffer, int32_t x, int32_t y, uint32_t width, uint32_t height, uint16_t color);

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


// ---------------------------------------------------------------------------
// Fonts
//
// A packed font is one font at one size, pre-rasterized to 8-bit coverage
// bitmaps that can be drawn with draw_alpha_mask().  Built from
// msdf-atlas-gen output by encoder/font.js.  All values are little endian
// and the blob must be 4-byte aligned:
//
//   FONT            header
//   FONT_GLYPH[]    glyphCount glyphs, sorted by codepoint
//   FONT_KERNING[]  kerningCount pairs, grouped by left glyph
//   uint16_t[]      hash table (1 << hashBits slots) of glyph indexes
//   uint8_t[]       glyph bitmaps
//
// Positions are in pixels with y down, relative to the pen position on the
// baseline.  Advances and kerning are in 1/64 pixels.
//
// The hash table maps codepoints to glyph indexes with linear probing:
// start at slot (codepoint * 0x9E3779B1) >> (32 - hashBits) and step
// forward (wrapping) until the glyph's codepoint matches or an empty slot
// (FONT_NO_GLYPH) is found.  It's at most half full.

#define FONT_SIGNATURE      0x544E4652      // "RFNT" (little endian)
#define FONT_VERSION        1
#define FONT_NO_GLYPH       0xFFFF

typedef struct FONT FONT;
typedef struct FONT_GLYPH FONT_GLYPH;
typedef struct FONT_KERNING FONT_KERNING;

#pragma pack(push, 1)

struct FONT                         // 52 bytes
{
    uint32_t signature;             // FONT_SIGNATURE
    uint16_t version;               // FONT_VERSION
    uint16_t flags;                 // 0, reserved
    uint32_t packedSize;            // total size of the font data
    uint16_t size;                  // pixels per em it was rendered at
    uint16_t lineHeight;            // baseline to baseline
    uint16_t ascent;                // pixels above the baseline
    uint16_t descent;               // pixels below the baseline
    int16_t  underlineY;            // top of underline relative to baseline (+ve = below)
    uint16_t underlineThickness;
    uint16_t glyphCount;
    uint16_t defaultGlyph;          // glyph for missing characters, or FONT_NO_GLYPH
    uint16_t hashBits;              // hash table has 1 << hashBits slots
    uint16_t reserved;
    uint32_t kerningCount;          // total kerning pairs
    uint32_t glyphsOffset;          // offsets from start of font
    uint32_t kerningOffset;
    uint32_t hashOffset;
    uint32_t bitmapsOffset;
};

struct FONT_GLYPH                   // 24 bytes
{
    uint32_t codepoint;             // Unicode codepoint
    uint32_t bitmap;                // offset of bitmap from bitmapsOffset
    uint32_t kerning;               // index of first kerning pair with this glyph on the left
    uint16_t width;                 // bitmap size, stride = width.  0 for blank glyphs
    uint16_t height;
    int16_t  left;                  // bitmap's top left relative to the pen
    int16_t  top;
    uint16_t advance;               // pen advance in 1/64 pixels
    uint16_t kerningCount;          // kerning pairs with this glyph on the left
};

struct FONT_KERNING                 // 4 bytes
{
    uint16_t right;                 // index of the right glyph (sorted)
    int16_t  adjust;                // advance adjustment in 1/64 pixels
};

#pragma pack(pop)

// Check that size bytes at font hold a well formed font: header, every
// offset, glyph bitmap, kerning pair and hash table entry are checked.  Use
// before trusting a font from an untrusted source.
bool check_font(const void* font, size_t size);

// Find the glyph for a codepoint, or NULL if the font doesn't have one
const FONT_GLYPH* font_find_glyph(const FONT* font, uint32_t codepoint);

// As font_find_glyph() but falls back to the font's default glyph (which
// may also be NULL)
const FONT_GLYPH* font_get_glyph(const FONT* font, uint32_t codepoint);

// Kerning adjustment in 1/64 pixels to add to left's advance when it's
// followed by right.  Either may be NULL (returns 0).
int32_t font_get_kerning(const FONT* font, const FONT_GLYPH* left, const FONT_GLYPH* right);

// A glyph's bitmap: width x height bytes with stride = width
const uint8_t* font_get_bitmap(const FONT* font, const FONT_GLYPH* glyph);

// Width of a UTF-8 string in pixels: the sum of glyph advances plus kerning,
// ie: how far the pen moves when drawing it.  Characters missing from the
// font use the default glyph, or are skipped if it has none.  Invalid UTF-8
// is treated as U+FFFD.  length is in bytes, or -1 if NUL terminated.  The
// height of a line is font->lineHeight.
int32_t font_measure_text(const FONT* font, const char* text, int32_t length);

#ifdef __cplusplus
}
#endif
