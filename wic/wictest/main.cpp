// Tests the RLE565 WIC decoder without registering it, by loading
// rle565wic.dll from the exe's directory and creating the decoder through
// DllGetClassObject.
//
// Usage: wictest <input.rle565>... [-o <outdir>]
//
// Each input is decoded, checked, and (with -o) re-encoded to PNG using the
// system WIC PNG encoder.

#include <windows.h>
#include <wincodec.h>
#include <shlwapi.h>
#include <wrl/client.h>
#include <stdio.h>
#include <string>
#include <vector>

#include "../guids.h"

using Microsoft::WRL::ComPtr;

static int g_failures = 0;

static void check(bool ok, const wchar_t* what, const std::wstring& file)
{
    wprintf(L"%s: %s (%s)\n", ok ? L"PASS" : L"FAIL", what, file.c_str());
    if (!ok)
        g_failures++;
}

typedef HRESULT (STDAPICALLTYPE* PFNDLLGETCLASSOBJECT)(REFCLSID, REFIID, LPVOID*);

static ComPtr<IClassFactory> LoadDecoderFactory()
{
    // Load the DLL from next to this exe
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, ARRAYSIZE(path));
    PathRemoveFileSpecW(path);
    PathAppendW(path, L"rle565wic.dll");

    HMODULE dll = LoadLibraryW(path);
    if (!dll)
    {
        wprintf(L"Failed to load %s\n", path);
        return nullptr;
    }

    auto getClassObject = reinterpret_cast<PFNDLLGETCLASSOBJECT>(GetProcAddress(dll, "DllGetClassObject"));
    if (!getClassObject)
        return nullptr;

    ComPtr<IClassFactory> factory;
    if (FAILED(getClassObject(CLSID_Rle565Decoder, IID_PPV_ARGS(&factory))))
        return nullptr;
    return factory;
}

static ComPtr<IWICBitmapDecoder> CreateDecoder(IClassFactory* factory)
{
    ComPtr<IWICBitmapDecoder> decoder;
    factory->CreateInstance(nullptr, IID_PPV_ARGS(&decoder));
    return decoder;
}

static std::vector<BYTE> ReadFile(const std::wstring& path)
{
    std::vector<BYTE> data;
    FILE* f = _wfopen(path.c_str(), L"rb");
    if (!f)
        return data;
    fseek(f, 0, SEEK_END);
    data.resize(ftell(f));
    fseek(f, 0, SEEK_SET);
    fread(data.data(), 1, data.size(), f);
    fclose(f);
    return data;
}

static ComPtr<IStream> MemStream(const BYTE* data, size_t size)
{
    ComPtr<IStream> stream;
    stream.Attach(SHCreateMemStream(data, static_cast<UINT>(size)));
    return stream;
}

static HRESULT WritePng(IWICImagingFactory* wic, IWICBitmapSource* source, const std::wstring& path)
{
    ComPtr<IWICStream> stream;
    HRESULT hr = wic->CreateStream(&stream);
    if (SUCCEEDED(hr)) hr = stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE);

    ComPtr<IWICBitmapEncoder> encoder;
    if (SUCCEEDED(hr)) hr = wic->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder);
    if (SUCCEEDED(hr)) hr = encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache);

    ComPtr<IWICBitmapFrameEncode> frame;
    if (SUCCEEDED(hr)) hr = encoder->CreateNewFrame(&frame, nullptr);
    if (SUCCEEDED(hr)) hr = frame->Initialize(nullptr);
    if (SUCCEEDED(hr)) hr = frame->WriteSource(source, nullptr);
    if (SUCCEEDED(hr)) hr = frame->Commit();
    if (SUCCEEDED(hr)) hr = encoder->Commit();
    return hr;
}

static void TestFile(IClassFactory* factory, IWICImagingFactory* wic, const std::wstring& file, const wchar_t* outdir)
{
    std::vector<BYTE> data = ReadFile(file);
    check(!data.empty(), L"read file", file);
    if (data.empty())
        return;

    // QueryCapability recognises the file and restores the stream position
    {
        auto decoder = CreateDecoder(factory);
        auto stream = MemStream(data.data(), data.size());
        DWORD cap = 0;
        HRESULT hr = decoder->QueryCapability(stream.Get(), &cap);
        ULARGE_INTEGER pos = {};
        LARGE_INTEGER zero = {};
        stream->Seek(zero, STREAM_SEEK_CUR, &pos);
        check(SUCCEEDED(hr) && cap == WICBitmapDecoderCapabilityCanDecodeAllImages, L"QueryCapability accepts", file);
        check(pos.QuadPart == 0, L"QueryCapability restores position", file);
    }

    // Decode
    auto decoder = CreateDecoder(factory);
    auto stream = MemStream(data.data(), data.size());
    HRESULT hr = decoder->Initialize(stream.Get(), WICDecodeMetadataCacheOnDemand);
    check(SUCCEEDED(hr), L"Initialize", file);
    if (FAILED(hr))
        return;

    GUID container = {};
    UINT count = 0;
    decoder->GetContainerFormat(&container);
    decoder->GetFrameCount(&count);
    check(container == GUID_ContainerFormatRle565, L"container format", file);
    check(count == 1, L"frame count", file);

    ComPtr<IWICBitmapFrameDecode> frame;
    check(SUCCEEDED(decoder->GetFrame(0, &frame)), L"GetFrame(0)", file);
    ComPtr<IWICBitmapFrameDecode> frame2;
    check(decoder->GetFrame(1, &frame2) == WINCODEC_ERR_FRAMEMISSING, L"GetFrame(1) fails", file);
    if (!frame)
        return;

    UINT width = 0, height = 0;
    GUID format = {};
    frame->GetSize(&width, &height);
    frame->GetPixelFormat(&format);
    check(width > 0 && height > 0, L"size", file);
    check(format == GUID_WICPixelFormat32bppBGRA, L"pixel format", file);

    // Full copy
    std::vector<uint32_t> pixels(static_cast<size_t>(width) * height);
    hr = frame->CopyPixels(nullptr, width * 4, static_cast<UINT>(pixels.size() * 4), reinterpret_cast<BYTE*>(pixels.data()));
    check(SUCCEEDED(hr), L"CopyPixels full", file);

    // Sub-rectangle with padded stride matches the full copy
    WICRect rc = { static_cast<INT>(width / 4), static_cast<INT>(height / 3), static_cast<INT>(width / 2), static_cast<INT>(height / 2) };
    UINT stride = rc.Width * 4 + 12;
    std::vector<BYTE> sub(static_cast<size_t>(stride) * rc.Height);
    hr = frame->CopyPixels(&rc, stride, static_cast<UINT>(sub.size()), sub.data());
    bool same = SUCCEEDED(hr);
    for (INT y = 0; same && y < rc.Height; y++)
        same = memcmp(sub.data() + y * stride, &pixels[(rc.Y + y) * width + rc.X], rc.Width * 4) == 0;
    check(same, L"CopyPixels sub-rect", file);

    // Bad arguments
    WICRect outside = { static_cast<INT>(width) - 2, 0, 4, 1 };
    check(frame->CopyPixels(&outside, 16, 16, sub.data()) == E_INVALIDARG, L"CopyPixels rejects out of range rect", file);
    check(frame->CopyPixels(nullptr, width * 4, width * 4, sub.data()) == WINCODEC_ERR_INSUFFICIENTBUFFER, L"CopyPixels rejects small buffer", file);
    check(frame->CopyPixels(nullptr, width * 2, static_cast<UINT>(pixels.size() * 4), sub.data()) == E_INVALIDARG, L"CopyPixels rejects small stride", file);

    // Second Initialize isn't allowed
    auto stream2 = MemStream(data.data(), data.size());
    check(decoder->Initialize(stream2.Get(), WICDecodeMetadataCacheOnDemand) == WINCODEC_ERR_WRONGSTATE, L"second Initialize fails", file);

    // Truncated and corrupted data fail cleanly
    {
        bool allFailed = true;
        for (size_t len : { size_t(0), size_t(3), size_t(20), size_t(40), data.size() / 2, data.size() - 1 })
        {
            auto d = CreateDecoder(factory);
            auto s = MemStream(data.data(), len);
            if (SUCCEEDED(d->Initialize(s.Get(), WICDecodeMetadataCacheOnDemand)))
                allFailed = false;
        }
        check(allFailed, L"truncated data rejected", file);

        std::vector<BYTE> bad = data;
        bad[1] = 'X';
        auto d = CreateDecoder(factory);
        auto s = MemStream(bad.data(), bad.size());
        DWORD cap = 1;
        d->QueryCapability(s.Get(), &cap);
        check(cap == 0, L"QueryCapability rejects bad signature", file);
        check(d->Initialize(s.Get(), WICDecodeMetadataCacheOnDemand) == WINCODEC_ERR_UNKNOWNIMAGEFORMAT, L"Initialize rejects bad signature", file);
    }

    // Frame outlives the decoder
    decoder.Reset();
    UINT w2 = 0, h2 = 0;
    check(SUCCEEDED(frame->GetSize(&w2, &h2)) && w2 == width, L"frame usable after decoder released", file);

    // Re-encode as PNG through the system encoder
    if (outdir)
    {
        std::wstring name = PathFindFileNameW(file.c_str());
        PathRemoveExtensionW(name.data());
        name.resize(wcslen(name.c_str()));
        std::wstring out = std::wstring(outdir) + L"\\" + name + L".png";
        check(SUCCEEDED(WritePng(wic, frame.Get(), out)), (L"write " + out).c_str(), file);
    }
}

int wmain(int argc, wchar_t** argv)
{
    std::vector<std::wstring> files;
    const wchar_t* outdir = nullptr;
    for (int i = 1; i < argc; i++)
    {
        if (wcscmp(argv[i], L"-o") == 0 && i + 1 < argc)
            outdir = argv[++i];
        else
            files.push_back(argv[i]);
    }
    if (files.empty())
    {
        wprintf(L"Usage: wictest <input.rle565>... [-o <outdir>]\n");
        return 2;
    }

    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    {
        ComPtr<IClassFactory> factory = LoadDecoderFactory();
        if (!factory)
        {
            wprintf(L"FAIL: couldn't load decoder\n");
            return 1;
        }

        ComPtr<IWICImagingFactory> wic;
        CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic));

        for (auto& file : files)
            TestFile(factory.Get(), wic.Get(), file, outdir);
    }
    CoUninitialize();

    wprintf(g_failures ? L"\n%d FAILED\n" : L"\nAll checks passed\n", g_failures);
    return g_failures ? 1 : 0;
}
