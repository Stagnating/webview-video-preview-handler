@echo off
echo Registering WebView2 Video Preview Handler...
regsvr32 "%~dp0bin\WebViewVideoPreview.dll"
pause
