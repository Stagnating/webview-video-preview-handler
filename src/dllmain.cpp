#include <windows.h>
#include <wrl/module.h>
#include <olectl.h>
#include <string>
#include <vector>
#include <sstream>
#include <shlobj.h>
#include "WebViewVideoPreviewHandler.h"

using namespace Microsoft::WRL;

static HMODULE g_hModule = nullptr;

// DLL entry point
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hModule);
            g_hModule = hModule;
            break;
    }
    return TRUE;
}

// Register the creatable class with WRL Module
CoCreatableClass(WebViewVideoPreviewHandler);

// --- COM Exports ---

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, _COM_Outptr_ void** ppv)
{
    return Microsoft::WRL::Module<Microsoft::WRL::InProc>::GetModule().GetClassObject(rclsid, riid, ppv);
}

STDAPI DllCanUnloadNow()
{
    return Microsoft::WRL::Module<Microsoft::WRL::InProc>::GetModule().Terminate() ? S_OK : S_FALSE;
}

// --- Registration Helpers ---

static std::wstring GetDllPath()
{
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(g_hModule, path, MAX_PATH);
    return std::wstring(path);
}

static const wchar_t* const PREVIEW_CLSID_STR = L"{8EBA39E2-E4AE-4C0D-9C3B-5F1D8A7E2B4C}";
static const wchar_t* const PREVIEW_HANDLER_IID = L"{8895b1c6-b41f-4c1c-a562-0d564250836f}";
static const wchar_t* const PREVHOST_APPID = L"{6d2b5079-2f0b-48dd-ab7f-97cec514d30b}";
static const wchar_t* const HANDLER_NAME = L"WebView2 Video Preview Handler";

// --- Config Parser ---
static std::vector<std::wstring> GetTargetExtensions()
{
    std::vector<std::wstring> extensions;

    // Get DLL path
    wchar_t modulePath[MAX_PATH];
    GetModuleFileNameW(g_hModule, modulePath, MAX_PATH);

    // Swap .dll to .ini
    std::wstring iniPath = modulePath;
    size_t extPos = iniPath.find_last_of(L".");
    if (extPos != std::wstring::npos) {
        iniPath = iniPath.substr(0, extPos) + L".ini";
    }

    // Read the INI file
    wchar_t buf[512] = {0};
    GetPrivateProfileStringW(
        L"Settings", L"Extensions", L".webm,.mp4", // default fallback
        buf, _countof(buf), iniPath.c_str());

    // Tokenize by comma
    std::wstringstream wss(buf);
    std::wstring token;
    while (std::getline(wss, token, L',')) {
        // Trim whitespace just in case
        token.erase(0, token.find_first_not_of(L" \t\r\n"));
        token.erase(token.find_last_not_of(L" \t\r\n") + 1);

        if (!token.empty() && token[0] == L'.') {
            extensions.push_back(token);
        }
    }

    return extensions;
}

static BOOL SetRegValue(HKEY hKey, const wchar_t* valueName, const wchar_t* data)
{
    return RegSetValueExW(hKey, valueName, 0, REG_SZ,
                          reinterpret_cast<const BYTE*>(data),
                          static_cast<DWORD>((wcslen(data) + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
}

STDAPI DllRegisterServer()
{
    std::wstring dllPath = GetDllPath();
    HKEY hKey;

    // 1. Register COM object CLSID
    std::wstring clsidKey = L"CLSID\\" + std::wstring(PREVIEW_CLSID_STR);
    if (RegCreateKeyExW(HKEY_CLASSES_ROOT, clsidKey.c_str(), 0, nullptr,
                        REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr) != ERROR_SUCCESS)
        return SELFREG_E_CLASS;

    SetRegValue(hKey, nullptr, HANDLER_NAME);
    SetRegValue(hKey, L"AppID", PREVHOST_APPID);
    RegCloseKey(hKey);

    // 1b. InprocServer32
    std::wstring inprocKey = clsidKey + L"\\InprocServer32";
    if (RegCreateKeyExW(HKEY_CLASSES_ROOT, inprocKey.c_str(), 0, nullptr,
                        REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr) != ERROR_SUCCESS)
        return SELFREG_E_CLASS;

    SetRegValue(hKey, nullptr, dllPath.c_str());
    SetRegValue(hKey, L"ThreadingModel", L"Apartment");
    RegCloseKey(hKey);

    // 2. Add to trusted Preview Handlers
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE,
                        L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\PreviewHandlers",
                        0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        SetRegValue(hKey, PREVIEW_CLSID_STR, HANDLER_NAME);
        RegCloseKey(hKey);
    } else {
        // Fallback to HKCU if not elevated
        if (RegCreateKeyExW(HKEY_CURRENT_USER,
                            L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\PreviewHandlers",
                            0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
            SetRegValue(hKey, PREVIEW_CLSID_STR, HANDLER_NAME);
            RegCloseKey(hKey);
        }
    }

    // 3. Associate with Dynamic Extensions
    std::vector<std::wstring> extensions = GetTargetExtensions();
    for (const auto& ext : extensions) {
        std::wstring extKey = ext + L"\\shellex\\" + std::wstring(PREVIEW_HANDLER_IID);
        if (RegCreateKeyExW(HKEY_CLASSES_ROOT, extKey.c_str(), 0, nullptr,
                            REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
            SetRegValue(hKey, nullptr, PREVIEW_CLSID_STR);
            RegCloseKey(hKey);
        }
    }

    // Tell the Windows Shell to flush its icon and handler cache
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);

    return S_OK;
}

STDAPI DllUnregisterServer()
{
    // Remove dynamic file extension associations
    std::vector<std::wstring> extensions = GetTargetExtensions();
    for (const auto& ext : extensions) {
        std::wstring extKey = ext + L"\\shellex\\" + std::wstring(PREVIEW_HANDLER_IID);
        RegDeleteTreeW(HKEY_CLASSES_ROOT, extKey.c_str());
    }

    // Remove from Preview Handlers
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                      L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\PreviewHandlers",
                      0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        RegDeleteValueW(hKey, PREVIEW_CLSID_STR);
        RegCloseKey(hKey);
    }
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\PreviewHandlers",
                      0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        RegDeleteValueW(hKey, PREVIEW_CLSID_STR);
        RegCloseKey(hKey);
    }

    // Remove CLSID registration
    std::wstring clsidKey = L"CLSID\\" + std::wstring(PREVIEW_CLSID_STR);
    RegDeleteTreeW(HKEY_CLASSES_ROOT, clsidKey.c_str());

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);

    return S_OK;
}
