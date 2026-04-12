# Windows Video Preview Handler

A Windows Shell preview handler that plays video and audio files directly in the File Explorer preview pane using WebView2.

## Install

1. Download the latest release zip
2. Extract to a permanent location (e.g. `%AppData%\VideoPreviewHandler`)
3. Run `register.bat` as administrator
4. In file explorer, enable the preview pane

To uninstall, run `unregister.bat` as administrator and delete the folder.

## Supported Formats

Configured via `WebViewVideoPreview.ini` — by default:

`.3gp` `.m4v` `.mkv` `.mov` `.mp4` `.ogv` `.webm` `.aac` `.flac` `.m4a` `.mp3` `.ogg` `.opus` `.wav` `.weba`

Edit the INI file to add or remove extensions. Re-register after changes.

## Requirements

- Windows 10 or later (Only tested on Win10)
- WebView2 Runtime (included with modern Windows)

## Building from Source

Requires Visual Studio Build Tools with the C++ workload.

1. Install the WebView2 SDK:
   ```
   nuget install Microsoft.Web.WebView2 -OutputDirectory packages
   ```
2. Run `build_release.bat` from a VS Developer Command Prompt

## License

MIT
