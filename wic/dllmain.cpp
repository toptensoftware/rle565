#include <windows.h>
#include <wincodec.h>
#include <shlobj.h>
#include <new>
#include <string>

#include "decoder.h"
#include "guids.h"
#include "module.h"

HMODULE g_module = nullptr;
static LONG g_moduleRefs = 0;

void ModuleAddRef()
{
    InterlockedIncrement(&g_moduleRefs);
}

void ModuleRelease()
{
    InterlockedDecrement(&g_moduleRefs);
}


// ---- Class factory ----

class Rle565ClassFactory : public IClassFactory
{
public:
    Rle565ClassFactory() { ModuleAddRef(); }

    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override
    {
        if (!ppv)
            return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_IClassFactory)
        {
            *ppv = static_cast<IClassFactory*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    IFACEMETHODIMP_(ULONG) AddRef() override
    {
        return InterlockedIncrement(&m_ref);
    }

    IFACEMETHODIMP_(ULONG) Release() override
    {
        ULONG ref = InterlockedDecrement(&m_ref);
        if (ref == 0)
            delete this;
        return ref;
    }

    IFACEMETHODIMP CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppv) override
    {
        if (!ppv)
            return E_POINTER;
        *ppv = nullptr;
        if (pUnkOuter)
            return CLASS_E_NOAGGREGATION;

        Rle565Decoder* decoder = new (std::nothrow) Rle565Decoder();
        if (!decoder)
            return E_OUTOFMEMORY;
        HRESULT hr = decoder->QueryInterface(riid, ppv);
        decoder->Release();
        return hr;
    }

    IFACEMETHODIMP LockServer(BOOL fLock) override
    {
        if (fLock)
            ModuleAddRef();
        else
            ModuleRelease();
        return S_OK;
    }

private:
    ~Rle565ClassFactory() { ModuleRelease(); }

    LONG m_ref = 1;
};


// ---- DLL entry points ----

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID /*reserved*/)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        g_module = hModule;
        DisableThreadLibraryCalls(hModule);
    }
    return TRUE;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv)
{
    if (!ppv)
        return E_POINTER;
    *ppv = nullptr;

    if (rclsid != CLSID_Rle565Decoder)
        return CLASS_E_CLASSNOTAVAILABLE;

    Rle565ClassFactory* factory = new (std::nothrow) Rle565ClassFactory();
    if (!factory)
        return E_OUTOFMEMORY;
    HRESULT hr = factory->QueryInterface(riid, ppv);
    factory->Release();
    return hr;
}

STDAPI DllCanUnloadNow()
{
    return g_moduleRefs == 0 ? S_OK : S_FALSE;
}


// ---- Registration ----
//
// Registers machine wide under HKLM\SOFTWARE\Classes, so regsvr32 must be
// run elevated.
//
//  * The decoder CLSID with WIC's decoder attributes, pixel format and the
//    "R565" signature pattern
//  * The decoder in WIC's decoder category
//  * .rle565 as an image so Explorer uses its photo thumbnail provider
//    (which uses WIC) for thumbnails and the preview pane

namespace
{
    const wchar_t* const FRIENDLY_NAME = L"RLE565 Decoder";
    const wchar_t* const FILE_EXTENSION = L".rle565";
    const wchar_t* const MIME_TYPE = L"image/x-rle565";

    const wchar_t* const CLASSES = L"SOFTWARE\\Classes\\";
    const wchar_t* const KINDMAP = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\KindMap";

    // CATID_WICBitmapDecoders
    const wchar_t* const WIC_DECODERS_CATEGORY = L"{7ED96837-96F0-4812-B211-F13C24117ED3}";

    // Shell thumbnail handler key, and the Windows photo thumbnail provider
    const wchar_t* const SHELLEX_THUMBNAIL = L"{e357fccd-a995-4576-b01f-234630154e96}";
    const wchar_t* const PHOTO_THUMBNAIL_PROVIDER = L"{C7657C4A-9F68-40fa-A4DF-96BC08EB3551}";

    std::wstring GuidString(const GUID& guid)
    {
        wchar_t buf[40];
        StringFromGUID2(guid, buf, ARRAYSIZE(buf));
        return buf;
    }

    // Tracks the first failure across a sequence of registry writes
    class RegWriter
    {
    public:
        void SetString(const std::wstring& key, const wchar_t* name, const std::wstring& value)
        {
            Check(RegSetKeyValueW(HKEY_LOCAL_MACHINE, key.c_str(), name, REG_SZ,
                value.c_str(), static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t))));
        }

        void SetDword(const std::wstring& key, const wchar_t* name, DWORD value)
        {
            Check(RegSetKeyValueW(HKEY_LOCAL_MACHINE, key.c_str(), name, REG_DWORD, &value, sizeof(value)));
        }

        void SetBinary(const std::wstring& key, const wchar_t* name, const void* data, DWORD size)
        {
            Check(RegSetKeyValueW(HKEY_LOCAL_MACHINE, key.c_str(), name, REG_BINARY, data, size));
        }

        void CreateKey(const std::wstring& key)
        {
            HKEY hkey;
            LSTATUS status = RegCreateKeyExW(HKEY_LOCAL_MACHINE, key.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &hkey, nullptr);
            if (status == ERROR_SUCCESS)
                RegCloseKey(hkey);
            Check(status);
        }

        HRESULT Result() const { return m_hr; }

    private:
        void Check(LSTATUS status)
        {
            if (status != ERROR_SUCCESS && SUCCEEDED(m_hr))
                m_hr = HRESULT_FROM_WIN32(status);
        }

        HRESULT m_hr = S_OK;
    };

    // The shell keys (.rle565, KindMap) aren't redirected for 32-bit
    // processes, so leave them to the native (64-bit) DLL on 64-bit Windows
    bool ShouldRegisterShellKeys()
    {
        BOOL wow64 = FALSE;
        return !(IsWow64Process(GetCurrentProcess(), &wow64) && wow64);
    }

    // Delete a value-less ShellEx key only if it still points where we set it
    void DeleteShellExIfOurs(const std::wstring& key, const wchar_t* expected)
    {
        wchar_t value[64] = {};
        DWORD size = sizeof(value) - sizeof(wchar_t);
        if (RegGetValueW(HKEY_LOCAL_MACHINE, key.c_str(), nullptr, RRF_RT_REG_SZ, nullptr, value, &size) == ERROR_SUCCESS
            && _wcsicmp(value, expected) == 0)
        {
            RegDeleteKeyW(HKEY_LOCAL_MACHINE, key.c_str());
        }
    }
}

STDAPI DllUnregisterServer();

STDAPI DllRegisterServer()
{
    wchar_t path[MAX_PATH];
    DWORD len = GetModuleFileNameW(g_module, path, ARRAYSIZE(path));
    if (len == 0 || len >= ARRAYSIZE(path))
        return E_FAIL;

    const std::wstring clsid = GuidString(CLSID_Rle565Decoder);
    const std::wstring classes = CLASSES;
    const std::wstring clsidKey = classes + L"CLSID\\" + clsid;

    RegWriter reg;

    // COM server
    reg.SetString(clsidKey, nullptr, FRIENDLY_NAME);
    reg.SetString(clsidKey + L"\\InprocServer32", nullptr, path);
    reg.SetString(clsidKey + L"\\InprocServer32", L"ThreadingModel", L"Both");

    // WIC decoder attributes
    reg.SetString(clsidKey, L"FriendlyName", FRIENDLY_NAME);
    reg.SetString(clsidKey, L"Author", L"Topten Software");
    reg.SetString(clsidKey, L"Description", L"RLE compressed RGB565 image decoder");
    reg.SetString(clsidKey, L"Vendor", GuidString(GUID_VendorRle565));
    reg.SetString(clsidKey, L"ContainerFormat", GuidString(GUID_ContainerFormatRle565));
    reg.SetString(clsidKey, L"Version", L"1.0.0.0");
    reg.SetString(clsidKey, L"SpecVersion", L"1.0.0.0");
    reg.SetString(clsidKey, L"FileExtensions", FILE_EXTENSION);
    reg.SetString(clsidKey, L"MimeTypes", MIME_TYPE);
    reg.SetDword(clsidKey, L"SupportAnimation", 0);
    reg.SetDword(clsidKey, L"SupportChromakey", 0);
    reg.SetDword(clsidKey, L"SupportLossless", 1);
    reg.SetDword(clsidKey, L"SupportMultiframe", 0);

    // Output pixel format
    reg.CreateKey(clsidKey + L"\\Formats\\" + GuidString(GUID_WICPixelFormat32bppBGRA));

    // "R565" signature at the start of the file
    const BYTE pattern[] = { 'R', '5', '6', '5' };
    const BYTE mask[] = { 0xFF, 0xFF, 0xFF, 0xFF };
    const std::wstring patternKey = clsidKey + L"\\Patterns\\0";
    reg.SetDword(patternKey, L"Position", 0);
    reg.SetDword(patternKey, L"Length", sizeof(pattern));
    reg.SetBinary(patternKey, L"Pattern", pattern, sizeof(pattern));
    reg.SetBinary(patternKey, L"Mask", mask, sizeof(mask));

    // WIC decoder category
    const std::wstring instanceKey = classes + L"CLSID\\" + WIC_DECODERS_CATEGORY + L"\\Instance\\" + clsid;
    reg.SetString(instanceKey, L"CLSID", clsid);
    reg.SetString(instanceKey, L"FriendlyName", FRIENDLY_NAME);

    // Shell integration
    if (ShouldRegisterShellKeys())
    {
        const std::wstring extKey = classes + FILE_EXTENSION;
        reg.SetString(extKey, L"Content Type", MIME_TYPE);
        reg.SetString(extKey, L"PerceivedType", L"image");
        reg.SetString(extKey + L"\\ShellEx\\" + SHELLEX_THUMBNAIL, nullptr, PHOTO_THUMBNAIL_PROVIDER);
        reg.SetString(KINDMAP, FILE_EXTENSION, L"picture");
    }

    HRESULT hr = reg.Result();
    if (FAILED(hr))
        DllUnregisterServer();
    else
        SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return hr;
}

STDAPI DllUnregisterServer()
{
    const std::wstring clsid = GuidString(CLSID_Rle565Decoder);
    const std::wstring classes = CLASSES;

    LSTATUS status = RegDeleteTreeW(HKEY_LOCAL_MACHINE, (classes + L"CLSID\\" + clsid).c_str());
    RegDeleteTreeW(HKEY_LOCAL_MACHINE, (classes + L"CLSID\\" + WIC_DECODERS_CATEGORY + L"\\Instance\\" + clsid).c_str());

    if (ShouldRegisterShellKeys())
    {
        // Only remove what we added, then the keys themselves if now empty
        const std::wstring extKey = classes + FILE_EXTENSION;
        DeleteShellExIfOurs(extKey + L"\\ShellEx\\" + SHELLEX_THUMBNAIL, PHOTO_THUMBNAIL_PROVIDER);
        RegDeleteKeyValueW(HKEY_LOCAL_MACHINE, extKey.c_str(), L"Content Type");
        RegDeleteKeyValueW(HKEY_LOCAL_MACHINE, extKey.c_str(), L"PerceivedType");
        RegDeleteKeyW(HKEY_LOCAL_MACHINE, (extKey + L"\\ShellEx").c_str());
        RegDeleteKeyW(HKEY_LOCAL_MACHINE, extKey.c_str());
        RegDeleteKeyValueW(HKEY_LOCAL_MACHINE, KINDMAP, FILE_EXTENSION);
    }

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);

    if (status != ERROR_SUCCESS && status != ERROR_FILE_NOT_FOUND)
        return HRESULT_FROM_WIN32(status);
    return S_OK;
}
