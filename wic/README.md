# RLE565 WIC Codec

A Windows Imaging Component (WIC) decoder for `.rle565` images, so they show
thumbnails and previews in Windows Explorer, and open in any WIC based app.

Decode only. Images are decoded to `32bppBGRA` (straight alpha) using
`decode_image_argb8888()` from `../src/rle565.c`.

## Building

Open `rle565wic.sln` in Visual Studio 2022 (or use MSBuild), and build x64
and/or x86. Output goes to `build/<Platform>/<Configuration>/`.

```
msbuild rle565wic.sln -p:Configuration=Release -p:Platform=x64
```

The C runtime is linked statically so the DLL has no redistributable
dependency.

## Testing

`wictest.exe` loads `rle565wic.dll` from its own directory (no registration
needed), decodes each file, runs a set of checks, and with `-o` writes each
image back out as PNG through the system WIC encoder:

```
build\x64\Release\wictest.exe ..\test\artwork\circles.bin -o out
```

## Installing

From an **elevated** command prompt:

```
regsvr32 build\x64\Release\rle565wic.dll
```

64-bit Explorer needs the x64 DLL. Register the x86 DLL as well if 32-bit
apps need to open `.rle565` files.

Registration is machine wide (`HKLM\SOFTWARE\Classes`) and adds:

- The decoder CLSID with its WIC attributes, the `32bppBGRA` output format
  and the `R565` signature pattern (bytes `52 35 36 35` at offset 0)
- The decoder in WIC's decoder category
- `.rle565` with `PerceivedType=image`, `Content Type=image/x-rle565` and
  Windows' Photo Thumbnail Provider as its thumbnail handler, plus a
  `picture` KindMap entry. (The x86 DLL skips these on 64-bit Windows as
  they're shared.)

Explorer caches thumbnails, so files viewed before registering may need the
thumbnail cache cleared (Disk Cleanup → Thumbnails) or Explorer restarted.

To uninstall:

```
regsvr32 /u build\x64\Release\rle565wic.dll
```

Images encoded with a shared palette (`rle565 --shared-palette`) don't
contain a palette, so they can't be previewed on their own.
