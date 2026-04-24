# WebView Video Preview Handler

Enables previewing most common video and audio formats in explorer's preview pane by using WebView2.

## Install

1. Download the latest release zip
2. Extract to a permanent location (e.g. `%AppData%\VideoPreviewHandler`)
3. Run `register.bat` as administrator
4. Make sure Preview pane is enabled in explorer: Top bar -> View -> Preview pane

To uninstall, run `unregister.bat` as administrator and delete the folder.

## Configuration

Configuration happens via `WebViewVideoPreview.ini`. Available settings:

- **Extensions**: comma-separated list of file extensions to associate with the handler (leading dots required). Defaults: `.3gp` `.m4v` `.mkv` `.mov` `.mp4` `.ogv` `.webm` `.aac` `.flac` `.m4a` `.mp3` `.ogg` `.opus` `.wav` `.weba`. Changes take effect after re-running `register.bat`.
- **Volume**: initial player volume, integer from `0` to `100`. Default `50`. Applied each time a preview cold-starts; subsequent in-folder swaps preserve any manual adjustment. 
- **Autoplay**: `true` or `false`. Default `true`.

Changing volume or autoplay setting requires restarting explorer.exe to take effect.

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
