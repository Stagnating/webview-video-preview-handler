@echo off
setlocal

REM === WebView2 Video Preview Handler Release Build Script ===

REM Set up VS 2019 BuildTools x64 environment
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 (
    echo ERROR: Failed to initialize VS build environment.
    exit /b 1
)

REM Locate WebView2 SDK
set WEBVIEW2_SDK=packages\Microsoft.Web.WebView2.1.0.3856.49\build\native
if not exist "%WEBVIEW2_SDK%\include\WebView2.h" (
    echo ERROR: WebView2 SDK not found. Run: nuget install Microsoft.Web.WebView2 -OutputDirectory packages
    exit /b 1
)

REM Create output directory
if not exist bin mkdir bin

REM Compile and link
echo Compiling WebViewVideoPreview.dll ...
cl /nologo /EHsc /std:c++17 /DNDEBUG /O2 /W3 /MT /GL ^
   /I"%WEBVIEW2_SDK%\include" ^
   src\dllmain.cpp src\WebViewVideoPreviewHandler.cpp ^
   /link /DEF:src\WebViewVideoPreview.def /LTCG /OPT:REF /OPT:ICF /RELEASE ^
   shlwapi.lib ole32.lib user32.lib uuid.lib shell32.lib oleaut32.lib advapi32.lib gdi32.lib runtimeobject.lib ^
   "%WEBVIEW2_SDK%\x64\WebView2Loader.dll.lib" ^
   /OUT:bin\WebViewVideoPreview.dll /DLL

if errorlevel 1 (
    echo ERROR: Build failed.
    exit /b 1
)

REM Copy WebView2Loader.dll to output
copy /Y "%WEBVIEW2_SDK%\x64\WebView2Loader.dll" bin\ >nul

REM Clean up object files
del /q *.obj 2>nul

echo.
echo Build succeeded: bin\WebViewVideoPreview.dll
echo.
echo To register (requires admin):
echo   regsvr32 bin\WebViewVideoPreview.dll
echo.
echo To unregister:
echo   regsvr32 /u bin\WebViewVideoPreview.dll

endlocal
