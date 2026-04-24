#include "WebViewVideoPreviewHandler.h"
#include <shlobj.h>
#include <filesystem>
#include <algorithm>
#include <cwctype>
#include <WebView2EnvironmentOptions.h>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "uuid.lib")
#pragma comment(lib, "shell32.lib")

extern HMODULE g_hModule;

using namespace Microsoft::WRL;

// --- IInitializeWithFile ---

IFACEMETHODIMP WebViewVideoPreviewHandler::Initialize(LPCWSTR pszFilePath, DWORD grfMode)
{
    if (!pszFilePath) return E_INVALIDARG;
    m_filePath = pszFilePath;
    return S_OK;
}

// --- IPreviewHandler ---

IFACEMETHODIMP WebViewVideoPreviewHandler::SetWindow(HWND hwnd, const RECT* prc)
{
    m_hwndParent = hwnd;
    if (prc) m_rc = *prc;

    if (m_controller) {
        m_controller->put_ParentWindow(hwnd);
        m_controller->put_Bounds(m_rc);
    }
    if (m_hwndFallback) {
        MoveWindow(m_hwndFallback, m_rc.left, m_rc.top,
                   m_rc.right - m_rc.left, m_rc.bottom - m_rc.top, TRUE);
    }
    return S_OK;
}

IFACEMETHODIMP WebViewVideoPreviewHandler::SetRect(const RECT* prc)
{
    if (!prc) return E_POINTER;
    m_rc = *prc;

    if (m_controller) {
        m_controller->put_Bounds(m_rc);
    }
    if (m_hwndFallback) {
        MoveWindow(m_hwndFallback, m_rc.left, m_rc.top,
                   m_rc.right - m_rc.left, m_rc.bottom - m_rc.top, TRUE);
    }
    return S_OK;
}


IFACEMETHODIMP WebViewVideoPreviewHandler::Unload()
{
    if (m_controller) {
        // Pause video and stop any network activity
        if (m_webView) {
            m_webView->ExecuteScript(
                L"(function(){ var v = document.getElementById('player'); if(v){ v.pause(); v.src=''; v.load(); } })();",
                nullptr);
        }
        // Hide but keep alive for reuse on next DoPreview
        m_controller->put_IsVisible(FALSE);
    }

    if (m_hwndFallback) {
        DestroyWindow(m_hwndFallback);
        m_hwndFallback = nullptr;
    }

    m_hwndParent = nullptr;
    m_filePath.clear();
    return S_OK;
}

IFACEMETHODIMP WebViewVideoPreviewHandler::SetFocus() { return S_OK; }

IFACEMETHODIMP WebViewVideoPreviewHandler::QueryFocus(HWND* phwnd)
{
    if (!phwnd) return E_POINTER;
    *phwnd = ::GetFocus();
    return S_OK;
}

IFACEMETHODIMP WebViewVideoPreviewHandler::TranslateAccelerator(MSG* pmsg) { return S_FALSE; }

// --- IOleWindow ---

IFACEMETHODIMP WebViewVideoPreviewHandler::GetWindow(HWND* phwnd)
{
    if (!phwnd) return E_POINTER;
    *phwnd = m_hwndParent;
    return S_OK;
}

IFACEMETHODIMP WebViewVideoPreviewHandler::ContextSensitiveHelp(BOOL fEnterMode) { return E_NOTIMPL; }

// --- IObjectWithSite ---

IFACEMETHODIMP WebViewVideoPreviewHandler::SetSite(IUnknown* pUnkSite)
{
    m_site = pUnkSite;
    return S_OK;
}

IFACEMETHODIMP WebViewVideoPreviewHandler::GetSite(REFIID riid, void** ppvSite)
{
    if (!ppvSite) return E_POINTER;
    if (!m_site) return E_FAIL;
    return m_site->QueryInterface(riid, ppvSite);
}



IFACEMETHODIMP WebViewVideoPreviewHandler::DoPreview()
{
    if (!m_hwndParent || m_filePath.empty()) return E_FAIL;

    // FAST PATH: WebView already exists from a previous preview. Reparent, remap, reload.
    if (m_webView) {
        m_controller->put_ParentWindow(m_hwndParent);
        m_controller->put_Bounds(m_rc);
        LoadVideoIntoWebView();
        m_controller->put_IsVisible(TRUE);
        return S_OK;
    }

    // COLD START: First preview ever on this instance.
    if (m_isInitializing) return S_OK;
    m_isInitializing = true;

    LoadSettings();

    PWSTR localLowPath = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppDataLow, 0, NULL, &localLowPath)))
        return E_FAIL;

    std::wstring userDataFolder = std::wstring(localLowPath) + L"\\WebView2VideoPreview";
    CoTaskMemFree(localLowPath);

    auto options = Make<CoreWebView2EnvironmentOptions>();
    options->put_AdditionalBrowserArguments(L"--autoplay-policy=no-user-gesture-required");

    ComPtr<IPreviewHandler> self(this);

    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, userDataFolder.c_str(), options.Get(),
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this, self](HRESULT res, ICoreWebView2Environment* env) -> HRESULT {
                if (FAILED(res)) { m_isInitializing = false; return res; }
                m_env = env;
                return m_env->CreateCoreWebView2Controller(
                    m_hwndParent,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [this, self](HRESULT res2, ICoreWebView2Controller* ctrl) -> HRESULT {
                            m_isInitializing = false;
                            if (FAILED(res2)) {
                                if (ctrl) ctrl->Close();
                                return res2;
                            }

                            m_controller = ctrl;
                            m_controller->put_Bounds(m_rc);
                            m_controller->put_IsVisible(TRUE);
                            m_controller->get_CoreWebView2(&m_webView);

                            ComPtr<ICoreWebView2Settings> settings;
                            if (SUCCEEDED(m_webView->get_Settings(&settings))) {
                                settings->put_AreDefaultContextMenusEnabled(FALSE);
                                settings->put_IsScriptEnabled(TRUE);
                            }

                            LoadVideoIntoWebView();
                            return S_OK;
                        }).Get());
            }).Get());

    if (FAILED(hr)) {
        m_isInitializing = false;
        CreateFallbackWindow(L"WebView2 failed to initialize.");
    }
    return S_OK;
}

void WebViewVideoPreviewHandler::LoadVideoIntoWebView()
{
    if (!m_webView) return;

    try {
        std::filesystem::path path(m_filePath);
        std::wstring directory = path.parent_path().wstring();
        std::wstring filename = path.filename().wstring();
        std::wstring videoUrl = L"http://preview.local/" + UrlEncodeFilename(filename);

        bool sameFolder = (directory == m_lastDirectory);

        if (!sameFolder) {
            // Remap virtual host to the new directory
            ComPtr<ICoreWebView2_3> webView3;
            if (SUCCEEDED(m_webView.As(&webView3))) {
                webView3->SetVirtualHostNameToFolderMapping(
                    L"preview.local", directory.c_str(),
                    COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_DENY_CORS);
            }
            m_lastDirectory = directory;
        }

        wchar_t volBuf[16];
        swprintf(volBuf, 16, L"%.3f", static_cast<double>(m_volume) / 100.0);
        std::wstring volumeFrac = volBuf;

        if (sameFolder && !m_lastDirectory.empty()) {
            // FAST PATH: Same folder — hot-swap video src via JS, keep DOM alive
            std::wstring js =
                L"(function(){"
                L"var v=document.getElementById('player');"
                L"if(!v)return;"
                L"v.pause();"
                L"v.src='" + videoUrl + L"';"
                L"v.load();";
            if (m_autoplay) {
                js += L"v.play().catch(function(){});";
            }
            js += L"})();";
            m_webView->ExecuteScript(js.c_str(), nullptr);
        } else {
            // SAFE PATH: Different folder or first load — full NavigateToString
            std::wstring autoplayAttr = m_autoplay ? L"autoplay " : L"";
            std::wstring htmlPayload =
                L"<!DOCTYPE html><html style='margin:0;padding:0;overflow:hidden;background:#000;'>"
                L"<body style='margin:0;padding:0;background:#000;'>"
                L"<video id='player' src='" + videoUrl + L"' "
                L"style='width:100vw;height:100vh;object-fit:contain;' "
                + autoplayAttr + L"controls loop></video>"
                L"<script>document.getElementById('player').volume=" + volumeFrac + L";</script>"
                L"</body></html>";
            m_webView->NavigateToString(htmlPayload.c_str());
        }
    } catch (...) {
        CreateFallbackWindow(L"Error loading file path.");
    }
}


// --- Helpers ---

void WebViewVideoPreviewHandler::LoadSettings()
{
    wchar_t modulePath[MAX_PATH];
    GetModuleFileNameW(g_hModule, modulePath, MAX_PATH);

    std::wstring iniPath = modulePath;
    size_t extPos = iniPath.find_last_of(L".");
    if (extPos != std::wstring::npos) {
        iniPath = iniPath.substr(0, extPos) + L".ini";
    }

    int vol = GetPrivateProfileIntW(L"Settings", L"Volume", 50, iniPath.c_str());
    m_volume = std::clamp(vol, 0, 100);

    wchar_t buf[16] = {0};
    GetPrivateProfileStringW(L"Settings", L"Autoplay", L"true",
                             buf, _countof(buf), iniPath.c_str());
    std::wstring val = buf;
    std::transform(val.begin(), val.end(), val.begin(),
                   [](wchar_t c){ return static_cast<wchar_t>(std::towlower(c)); });
    m_autoplay = (val != L"false" && val != L"0" && val != L"no");
}

std::wstring WebViewVideoPreviewHandler::UrlEncodeFilename(const std::wstring& filename)
{
    std::wstring encoded;
    for (wchar_t ch : filename) {
        if (iswalnum(ch) || ch == L'-' || ch == L'_' || ch == L'.' || ch == L'~') {
            encoded += ch;
        } else {
            wchar_t buf[8];
            swprintf(buf, 8, L"%%%02X", static_cast<unsigned int>(ch));
            encoded += buf;
        }
    }
    return encoded;
}

void WebViewVideoPreviewHandler::CreateFallbackWindow(const wchar_t* message)
{
    if (!m_hwndParent) return;

    int width = m_rc.right - m_rc.left;
    int height = m_rc.bottom - m_rc.top;

    m_hwndFallback = CreateWindowW(
        L"STATIC", message,
        SS_CENTER | SS_CENTERIMAGE | WS_CHILD | WS_VISIBLE,
        m_rc.left, m_rc.top, width, height,
        m_hwndParent, nullptr, nullptr, nullptr);

    if (m_hwndFallback) {
        HFONT hFont = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                   CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        SendMessage(m_hwndFallback, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
    }
}