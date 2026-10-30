#include "decoder.h"
#include "guids.h"
#include "module.h"

#include <new>
#include <wrl/client.h>

#pragma warning(push)
#pragma warning(disable: 4200)      // zero-sized arrays in rle565.h
#include "rle565.h"
#pragma warning(pop)

using Microsoft::WRL::ComPtr;

namespace
{
    // Limits so a hostile file can't make Explorer allocate huge amounts
    const uint64_t MAX_FILE_SIZE = 256ull * 1024 * 1024;
    const uint64_t MAX_PIXELS = 64ull * 1024 * 1024;

    // Read exactly len bytes or fail
    HRESULT ReadExact(IStream* stream, void* buffer, ULONG len)
    {
        BYTE* p = static_cast<BYTE*>(buffer);
        while (len)
        {
            ULONG got = 0;
            HRESULT hr = stream->Read(p, len, &got);
            if (FAILED(hr))
                return hr;
            if (got == 0)
                return WINCODEC_ERR_BADIMAGE;      // unexpected end of stream
            p += got;
            len -= got;
        }
        return S_OK;
    }

    // Quick check of the fixed header fields
    bool IsRle565Header(const IMAGE_RLE& header)
    {
        return header.base.signature == IMAGE_SIGNATURE
            && header.base.format == IMAGE_FORMAT_RLE;
    }
}


// ---- Rle565Decoder ----

Rle565Decoder::Rle565Decoder()
{
    ModuleAddRef();
}

Rle565Decoder::~Rle565Decoder()
{
    ModuleRelease();
}

IFACEMETHODIMP Rle565Decoder::QueryInterface(REFIID riid, void** ppv)
{
    if (!ppv)
        return E_POINTER;

    if (riid == IID_IUnknown || riid == IID_IWICBitmapDecoder)
    {
        *ppv = static_cast<IWICBitmapDecoder*>(this);
        AddRef();
        return S_OK;
    }

    *ppv = nullptr;
    return E_NOINTERFACE;
}

IFACEMETHODIMP_(ULONG) Rle565Decoder::AddRef()
{
    return InterlockedIncrement(&m_ref);
}

IFACEMETHODIMP_(ULONG) Rle565Decoder::Release()
{
    ULONG ref = InterlockedDecrement(&m_ref);
    if (ref == 0)
        delete this;
    return ref;
}

IFACEMETHODIMP Rle565Decoder::QueryCapability(IStream* pIStream, DWORD* pdwCapability)
{
    if (!pIStream || !pdwCapability)
        return E_INVALIDARG;
    *pdwCapability = 0;

    // Remember the position so it can be restored
    LARGE_INTEGER zero = {};
    ULARGE_INTEGER start;
    HRESULT hr = pIStream->Seek(zero, STREAM_SEEK_CUR, &start);
    if (FAILED(hr))
        return hr;

    IMAGE_RLE header;
    bool match = SUCCEEDED(ReadExact(pIStream, &header, sizeof(header))) && IsRle565Header(header);

    LARGE_INTEGER back;
    back.QuadPart = static_cast<LONGLONG>(start.QuadPart);
    hr = pIStream->Seek(back, STREAM_SEEK_SET, nullptr);
    if (FAILED(hr))
        return hr;

    if (match)
        *pdwCapability = WICBitmapDecoderCapabilityCanDecodeAllImages;
    return S_OK;
}

IFACEMETHODIMP Rle565Decoder::Initialize(IStream* pIStream, WICDecodeOptions /*cacheOptions*/)
{
    if (!pIStream)
        return E_INVALIDARG;

    AcquireSRWLockExclusive(&m_lock);
    struct Unlock { SRWLOCK* l; ~Unlock() { ReleaseSRWLockExclusive(l); } } unlock{ &m_lock };

    if (m_image)
        return WINCODEC_ERR_WRONGSTATE;

    // Read and sanity check the fixed header
    IMAGE_RLE header;
    HRESULT hr = ReadExact(pIStream, &header, sizeof(header));
    if (FAILED(hr))
        return hr;
    if (!IsRle565Header(header))
        return WINCODEC_ERR_UNKNOWNIMAGEFORMAT;
    if (header.packedSize < sizeof(header) || header.packedSize > MAX_FILE_SIZE)
        return WINCODEC_ERR_BADHEADER;
    if (header.base.width == 0 || header.base.height == 0)
        return WINCODEC_ERR_BADHEADER;
    if (static_cast<uint64_t>(header.base.width) * header.base.height > MAX_PIXELS)
        return WINCODEC_ERR_IMAGESIZEOUTOFRANGE;

    // Images packed against a shared palette can't be shown on their own
    if (header.paletteOffset == 0)
        return WINCODEC_ERR_BADIMAGE;

    try
    {
        // Read the rest of the packed image
        std::vector<uint8_t> data(header.packedSize);
        memcpy(data.data(), &header, sizeof(header));
        hr = ReadExact(pIStream, data.data() + sizeof(header), header.packedSize - static_cast<ULONG>(sizeof(header)));
        if (FAILED(hr))
            return hr;

        if (!check_image_header(data.data(), data.size()))
            return WINCODEC_ERR_BADHEADER;

        // Decode
        auto image = std::make_shared<DecodedImage>();
        image->width = header.base.width;
        image->height = header.base.height;
        image->pixels.resize(static_cast<size_t>(image->width) * image->height);
        if (!decode_image_argb8888(data.data(), data.size(), nullptr, image->pixels.data(), 0, false))
            return WINCODEC_ERR_BADIMAGE;

        m_image = std::move(image);
        return S_OK;
    }
    catch (const std::bad_alloc&)
    {
        return E_OUTOFMEMORY;
    }
}

IFACEMETHODIMP Rle565Decoder::GetContainerFormat(GUID* pguidContainerFormat)
{
    if (!pguidContainerFormat)
        return E_INVALIDARG;
    *pguidContainerFormat = GUID_ContainerFormatRle565;
    return S_OK;
}

IFACEMETHODIMP Rle565Decoder::GetDecoderInfo(IWICBitmapDecoderInfo** ppIDecoderInfo)
{
    if (!ppIDecoderInfo)
        return E_INVALIDARG;
    *ppIDecoderInfo = nullptr;

    // Component info comes from our registration
    ComPtr<IWICImagingFactory> factory;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (FAILED(hr))
        return hr;

    ComPtr<IWICComponentInfo> info;
    hr = factory->CreateComponentInfo(CLSID_Rle565Decoder, &info);
    if (FAILED(hr))
        return hr;

    return info->QueryInterface(IID_PPV_ARGS(ppIDecoderInfo));
}

IFACEMETHODIMP Rle565Decoder::CopyPalette(IWICPalette* /*pIPalette*/)
{
    return WINCODEC_ERR_PALETTEUNAVAILABLE;
}

IFACEMETHODIMP Rle565Decoder::GetMetadataQueryReader(IWICMetadataQueryReader** ppIMetadataQueryReader)
{
    if (ppIMetadataQueryReader)
        *ppIMetadataQueryReader = nullptr;
    return WINCODEC_ERR_UNSUPPORTEDOPERATION;
}

IFACEMETHODIMP Rle565Decoder::GetPreview(IWICBitmapSource** ppIBitmapSource)
{
    if (ppIBitmapSource)
        *ppIBitmapSource = nullptr;
    return WINCODEC_ERR_UNSUPPORTEDOPERATION;
}

IFACEMETHODIMP Rle565Decoder::GetColorContexts(UINT /*cCount*/, IWICColorContext** /*ppIColorContexts*/, UINT* pcActualCount)
{
    if (!pcActualCount)
        return E_INVALIDARG;
    *pcActualCount = 0;
    return S_OK;
}

IFACEMETHODIMP Rle565Decoder::GetThumbnail(IWICBitmapSource** ppIThumbnail)
{
    if (ppIThumbnail)
        *ppIThumbnail = nullptr;
    return WINCODEC_ERR_CODECNOTHUMBNAIL;
}

IFACEMETHODIMP Rle565Decoder::GetFrameCount(UINT* pCount)
{
    if (!pCount)
        return E_INVALIDARG;

    AcquireSRWLockShared(&m_lock);
    *pCount = m_image ? 1 : 0;
    bool initialized = m_image != nullptr;
    ReleaseSRWLockShared(&m_lock);

    return initialized ? S_OK : WINCODEC_ERR_NOTINITIALIZED;
}

IFACEMETHODIMP Rle565Decoder::GetFrame(UINT index, IWICBitmapFrameDecode** ppIBitmapFrame)
{
    if (!ppIBitmapFrame)
        return E_INVALIDARG;
    *ppIBitmapFrame = nullptr;

    AcquireSRWLockShared(&m_lock);
    std::shared_ptr<const DecodedImage> image = m_image;
    ReleaseSRWLockShared(&m_lock);

    if (!image)
        return WINCODEC_ERR_NOTINITIALIZED;
    if (index != 0)
        return WINCODEC_ERR_FRAMEMISSING;

    Rle565Frame* frame = new (std::nothrow) Rle565Frame(std::move(image));
    if (!frame)
        return E_OUTOFMEMORY;

    *ppIBitmapFrame = frame;
    return S_OK;
}


// ---- Rle565Frame ----

Rle565Frame::Rle565Frame(std::shared_ptr<const DecodedImage> image)
    : m_image(std::move(image))
{
    ModuleAddRef();
}

Rle565Frame::~Rle565Frame()
{
    ModuleRelease();
}

IFACEMETHODIMP Rle565Frame::QueryInterface(REFIID riid, void** ppv)
{
    if (!ppv)
        return E_POINTER;

    if (riid == IID_IUnknown || riid == IID_IWICBitmapSource || riid == IID_IWICBitmapFrameDecode)
    {
        *ppv = static_cast<IWICBitmapFrameDecode*>(this);
        AddRef();
        return S_OK;
    }

    *ppv = nullptr;
    return E_NOINTERFACE;
}

IFACEMETHODIMP_(ULONG) Rle565Frame::AddRef()
{
    return InterlockedIncrement(&m_ref);
}

IFACEMETHODIMP_(ULONG) Rle565Frame::Release()
{
    ULONG ref = InterlockedDecrement(&m_ref);
    if (ref == 0)
        delete this;
    return ref;
}

IFACEMETHODIMP Rle565Frame::GetSize(UINT* puiWidth, UINT* puiHeight)
{
    if (!puiWidth || !puiHeight)
        return E_INVALIDARG;
    *puiWidth = m_image->width;
    *puiHeight = m_image->height;
    return S_OK;
}

IFACEMETHODIMP Rle565Frame::GetPixelFormat(WICPixelFormatGUID* pPixelFormat)
{
    if (!pPixelFormat)
        return E_INVALIDARG;
    *pPixelFormat = GUID_WICPixelFormat32bppBGRA;
    return S_OK;
}

IFACEMETHODIMP Rle565Frame::GetResolution(double* pDpiX, double* pDpiY)
{
    if (!pDpiX || !pDpiY)
        return E_INVALIDARG;
    *pDpiX = 96.0;
    *pDpiY = 96.0;
    return S_OK;
}

IFACEMETHODIMP Rle565Frame::CopyPalette(IWICPalette* /*pIPalette*/)
{
    return WINCODEC_ERR_PALETTEUNAVAILABLE;
}

IFACEMETHODIMP Rle565Frame::CopyPixels(const WICRect* prc, UINT cbStride, UINT cbBufferSize, BYTE* pbBuffer)
{
    if (!pbBuffer)
        return E_INVALIDARG;

    const INT64 width = m_image->width;
    const INT64 height = m_image->height;

    WICRect full = { 0, 0, static_cast<INT>(width), static_cast<INT>(height) };
    const WICRect& rc = prc ? *prc : full;

    // Rectangle must be within the image
    if (rc.X < 0 || rc.Y < 0 || rc.Width < 0 || rc.Height < 0
        || rc.X + static_cast<INT64>(rc.Width) > width
        || rc.Y + static_cast<INT64>(rc.Height) > height)
        return E_INVALIDARG;

    if (rc.Width == 0 || rc.Height == 0)
        return S_OK;

    // Buffer must hold the rectangle
    const UINT64 rowBytes = static_cast<UINT64>(rc.Width) * sizeof(uint32_t);
    if (cbStride < rowBytes)
        return E_INVALIDARG;
    if (static_cast<UINT64>(rc.Height - 1) * cbStride + rowBytes > cbBufferSize)
        return WINCODEC_ERR_INSUFFICIENTBUFFER;

    for (INT y = 0; y < rc.Height; y++)
    {
        const uint32_t* src = m_image->pixels.data() + static_cast<size_t>(rc.Y + y) * m_image->width + rc.X;
        memcpy(pbBuffer + static_cast<size_t>(y) * cbStride, src, static_cast<size_t>(rowBytes));
    }
    return S_OK;
}

IFACEMETHODIMP Rle565Frame::GetMetadataQueryReader(IWICMetadataQueryReader** ppIMetadataQueryReader)
{
    if (ppIMetadataQueryReader)
        *ppIMetadataQueryReader = nullptr;
    return WINCODEC_ERR_UNSUPPORTEDOPERATION;
}

IFACEMETHODIMP Rle565Frame::GetColorContexts(UINT /*cCount*/, IWICColorContext** /*ppIColorContexts*/, UINT* pcActualCount)
{
    if (!pcActualCount)
        return E_INVALIDARG;
    *pcActualCount = 0;
    return S_OK;
}

IFACEMETHODIMP Rle565Frame::GetThumbnail(IWICBitmapSource** ppIThumbnail)
{
    if (ppIThumbnail)
        *ppIThumbnail = nullptr;
    return WINCODEC_ERR_CODECNOTHUMBNAIL;
}
