#pragma once

#include <guiddef.h>

// CLSID of the RLE565 WIC decoder
// {FF41194F-181B-4271-8933-88332AE3C0E8}
inline constexpr GUID CLSID_Rle565Decoder =
    { 0xff41194f, 0x181b, 0x4271, { 0x89, 0x33, 0x88, 0x33, 0x2a, 0xe3, 0xc0, 0xe8 } };

// WIC container format for .rle565 files
// {15B79825-0D2A-4FB0-B9ED-4697E5BE57C7}
inline constexpr GUID GUID_ContainerFormatRle565 =
    { 0x15b79825, 0x0d2a, 0x4fb0, { 0xb9, 0xed, 0x46, 0x97, 0xe5, 0xbe, 0x57, 0xc7 } };

// WIC vendor GUID
// {185E19CF-ED16-4B75-A605-F422C8BEFCE6}
inline constexpr GUID GUID_VendorRle565 =
    { 0x185e19cf, 0xed16, 0x4b75, { 0xa6, 0x05, 0xf4, 0x22, 0xc8, 0xbe, 0xfc, 0xe6 } };
