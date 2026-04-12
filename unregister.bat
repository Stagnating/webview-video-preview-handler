@echo off
echo Unregistering WebView2 Video Preview Handler...
regsvr32 /u "%~dp0bin\WebViewVideoPreview.dll"
pause
