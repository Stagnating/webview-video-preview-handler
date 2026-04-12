#pragma once

#include <windows.h>
#include <shobjidl.h>
#include <wrl/client.h>
#include <wrl/implements.h>
#include <wrl/event.h>
#include <WebView2.h>
#include <string>

// {8EBA39E2-E4AE-4C0D-9C3B-5F1D8A7E2B4C}
DEFINE_GUID(CLSID_WebViewVideoPreviewHandler,
    0x8EBA39E2, 0xE4AE, 0x4C0D, 0x9C, 0x3B, 0x5F, 0x1D, 0x8A, 0x7E, 0x2B, 0x4C);

using namespace Microsoft::WRL;

class DECLSPEC_UUID("8EBA39E2-E4AE-4C0D-9C3B-5F1D8A7E2B4C")
WebViewVideoPreviewHandler
    : public RuntimeClass<
          RuntimeClassFlags<ClassicCom>,
          IPreviewHandler,
          IInitializeWithFile,
          IOleWindow,
          IObjectWithSite>
{
public:
    WebViewVideoPreviewHandler() = default;
    virtual ~WebViewVideoPreviewHandler() = default;

    // IInitializeWithFile
    IFACEMETHODIMP Initialize(LPCWSTR pszFilePath, DWORD grfMode) override;

    // IPreviewHandler
    IFACEMETHODIMP SetWindow(HWND hwnd, const RECT* prc) override;
    IFACEMETHODIMP SetRect(const RECT* prc) override;
    IFACEMETHODIMP DoPreview() override;
    IFACEMETHODIMP Unload() override;
    IFACEMETHODIMP SetFocus() override;
    IFACEMETHODIMP QueryFocus(HWND* phwnd) override;
    IFACEMETHODIMP TranslateAccelerator(MSG* pmsg) override;

    // IOleWindow
    IFACEMETHODIMP GetWindow(HWND* phwnd) override;
    IFACEMETHODIMP ContextSensitiveHelp(BOOL fEnterMode) override;

    // IObjectWithSite
    IFACEMETHODIMP SetSite(IUnknown* pUnkSite) override;
    IFACEMETHODIMP GetSite(REFIID riid, void** ppvSite) override;

private:
    // --- WebView2 Instance Members ---
    ComPtr<ICoreWebView2Environment> m_env;
    ComPtr<ICoreWebView2Controller>  m_controller;
    ComPtr<ICoreWebView2>            m_webView;

    // --- State Management ---
    HWND          m_hwndParent = nullptr;
    HWND          m_hwndFallback = nullptr;
    RECT          m_rc = { 0 };
    std::wstring  m_filePath;
    ComPtr<IUnknown> m_site;

    bool m_isInitializing = false;
    std::wstring m_lastDirectory;

    // --- Private Internal Methods ---
    void LoadVideoIntoWebView();
    void CreateFallbackWindow(const wchar_t* message);
    std::wstring UrlEncodeFilename(const std::wstring& filename);
};