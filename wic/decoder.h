#pragma once

#include <windows.h>
#include <wincodec.h>
#include <memory>
#include <vector>

// Fully decoded image shared (read only) between a decoder and its frame
struct DecodedImage
{
    UINT width = 0;
    UINT height = 0;
    std::vector<uint32_t> pixels;      // 32bppBGRA, straight alpha
};

// IWICBitmapDecoder for .rle565 files.  The whole image is read and
// decoded in Initialize() so the stream isn't held afterwards.
class Rle565Decoder : public IWICBitmapDecoder
{
public:
    Rle565Decoder();

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    IFACEMETHODIMP_(ULONG) AddRef() override;
    IFACEMETHODIMP_(ULONG) Release() override;

    // IWICBitmapDecoder
    IFACEMETHODIMP QueryCapability(IStream* pIStream, DWORD* pdwCapability) override;
    IFACEMETHODIMP Initialize(IStream* pIStream, WICDecodeOptions cacheOptions) override;
    IFACEMETHODIMP GetContainerFormat(GUID* pguidContainerFormat) override;
    IFACEMETHODIMP GetDecoderInfo(IWICBitmapDecoderInfo** ppIDecoderInfo) override;
    IFACEMETHODIMP CopyPalette(IWICPalette* pIPalette) override;
    IFACEMETHODIMP GetMetadataQueryReader(IWICMetadataQueryReader** ppIMetadataQueryReader) override;
    IFACEMETHODIMP GetPreview(IWICBitmapSource** ppIBitmapSource) override;
    IFACEMETHODIMP GetColorContexts(UINT cCount, IWICColorContext** ppIColorContexts, UINT* pcActualCount) override;
    IFACEMETHODIMP GetThumbnail(IWICBitmapSource** ppIThumbnail) override;
    IFACEMETHODIMP GetFrameCount(UINT* pCount) override;
    IFACEMETHODIMP GetFrame(UINT index, IWICBitmapFrameDecode** ppIBitmapFrame) override;

private:
    ~Rle565Decoder();

    LONG m_ref = 1;
    SRWLOCK m_lock = SRWLOCK_INIT;
    std::shared_ptr<const DecodedImage> m_image;
};

// The single frame of a .rle565 image
class Rle565Frame : public IWICBitmapFrameDecode
{
public:
    explicit Rle565Frame(std::shared_ptr<const DecodedImage> image);

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    IFACEMETHODIMP_(ULONG) AddRef() override;
    IFACEMETHODIMP_(ULONG) Release() override;

    // IWICBitmapSource
    IFACEMETHODIMP GetSize(UINT* puiWidth, UINT* puiHeight) override;
    IFACEMETHODIMP GetPixelFormat(WICPixelFormatGUID* pPixelFormat) override;
    IFACEMETHODIMP GetResolution(double* pDpiX, double* pDpiY) override;
    IFACEMETHODIMP CopyPalette(IWICPalette* pIPalette) override;
    IFACEMETHODIMP CopyPixels(const WICRect* prc, UINT cbStride, UINT cbBufferSize, BYTE* pbBuffer) override;

    // IWICBitmapFrameDecode
    IFACEMETHODIMP GetMetadataQueryReader(IWICMetadataQueryReader** ppIMetadataQueryReader) override;
    IFACEMETHODIMP GetColorContexts(UINT cCount, IWICColorContext** ppIColorContexts, UINT* pcActualCount) override;
    IFACEMETHODIMP GetThumbnail(IWICBitmapSource** ppIThumbnail) override;

private:
    ~Rle565Frame();

    LONG m_ref = 1;
    std::shared_ptr<const DecodedImage> m_image;
};
