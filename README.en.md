# Spotify Lyrics Better HUD

[简体中文](README.md) · **English**

A derivative of **[Fanbeixi / spotify-lyrics-HUD_CN](https://github.com/Fanbeixi/spotify-lyrics-HUD_CN)**, maintained by [E77zL](https://github.com/E77zL).
Adds Spotify lifecycle integration, an in-client toggle, a native desktop HUD, NetEase fallback and per-track lyric timing.
This is an independently maintained repository. Upstream attribution and licenses are retained in [CREDITS.md](CREDITS.md), [NOTICE.txt](NOTICE.txt) and [licenses/](licenses/). Version numbers refer to this derivative, not the upstream project.

[Download the latest build](https://github.com/E77zL/spotify-lyrics-better-hud/releases/latest) · [Original project](https://github.com/Fanbeixi/spotify-lyrics-HUD_CN)

## Preview

<img width="1500" height="1000" alt="Spotify with the desktop lyrics HUD" src="https://github.com/user-attachments/assets/8fcde938-c67e-45d6-b507-bc0b4bc1a535" />
<img width="2560" height="1440" alt="Fullscreen lyrics" src="https://github.com/user-attachments/assets/50b27097-ee33-426e-95b5-2a8e89d79ef8" />

### Automatic click-through over fullscreen apps

<img width="800" height="450" alt="Desktop lyrics with fullscreen click-through" src="https://github.com/user-attachments/assets/4d2e6a5c-8a38-406d-b34a-5585d1e6998e" />

The HUD ignores mouse input while another fullscreen app is in the foreground, preventing accidental clicks from interrupting a game. Desktop and shell windows are excluded. Normal interaction returns when you switch to a windowed app or the desktop.

### Colors and glass opacity

<img width="954" height="388" alt="Customizable lyric colors and background" src="https://github.com/user-attachments/assets/3ee0f1ce-0967-4773-b564-ad81ef69db77" />
<img width="919" height="342" alt="Translucent lyrics background" src="https://github.com/user-attachments/assets/4a1d8029-215c-49d4-a7de-7246d9ccd8e9" />

## Install and use

Requires Windows 10 or later, x64, and Spotify's desktop installer from its official website. The Microsoft Store version is not supported by this integration.

1. Download the latest setup EXE from Releases. Close Spotify, including its tray icon.
2. Open the dark graphical installer, read and accept the license, then click **Next**.
3. Select **Install / Repair** and click **Install**. Standard and administrator sessions are supported; elevated sessions pass `--bypass-admin` to Spicetify automatically.
4. Open Spotify. Use the **A** icon to the left of the microphone in the bottom playback bar to toggle desktop lyrics.

The default single-file installer reuses an existing Spicetify installation. If none is found, it downloads a pinned release and verifies its SHA256. The **Offline** installer includes that dependency; both variants provide the same features and include corresponding source and licenses.

Hover over the HUD to adjust typography, antialiasing, opacity, colors and per-song timing. Drag to move it and drag its lower-right edge to resize. Lyrics wrap to the available width. In the HUD's own fullscreen mode, Left/Right seek five seconds, Up/Down adjust Spotify volume, and Space pauses or resumes. Drag the top strip to return to a window.

Spotify's Marketplace entry opens themes, extensions and CSS snippets. These are optional; third-party customizations are not enabled by default.

In setup, **Disable integration** removes the HUD integration and A button while preserving personal settings and unrelated extensions. **Check environment** inspects the installation without modifying it. The source icon at the bottom exports source and license files. Run **Install / Repair** again if a Spotify update removes the button or integration.

## How it works

- The HUD starts and exits with Spotify. There is no resident launcher, login monitor or automatic updater.
- Installation adds `version.dll` and `LyricsHUD/bridge.ini` to Spotify's installation directory without modifying `Spotify.exe`. Windows version-query calls are forwarded to the system DLL; the HUD starts only in the main Spotify process.
- An unrelated `version.dll` is never overwritten. Upgrades remove this project's legacy launcher and startup entries.
- Lyrics are matched by track identity, including artist, rather than choosing the first song with the same title. Providers are tried in order: **LRCLIB → QQ Music → NetEase Music**, falling back when earlier providers have no usable match.
- Per-track timing offsets are preserved when integration is disabled.
- Windows Composition supplies glass blur; cached alpha edges keep the rounded corners smooth. No Electron runtime is distributed by the current native HUD.

## Changelog and building

[Changelog](CHANGELOG.md) · [Published releases](https://github.com/E77zL/spotify_lyrics/releases) · [Release and attribution guide](RELEASING.md)

Build on Windows with Node.js 22+, a Visual Studio C++20 toolchain and a Windows SDK supporting Windows Composition. The graphical installer uses the Windows .NET Framework WPF components; no additional browser runtime is bundled.

```powershell
.\Build.ps1 -Offline
```

This builds both installer variants and the source archive under `dist/`, runs the regression and installer checks, and writes `SHA256SUMS.txt`. Add `-CacheDir <directory>` to reuse a verified dependency cache. Running without `-Offline` builds the default installer only. This repository does not include a GitHub Actions workflow; build locally and upload release files manually.

## Data and licensing

The local control service listens only on `127.0.0.1:27946` and authenticates requests using a token generated during installation. `%LOCALAPPDATA%/SpotifyLyricsCompanion/connection.json` contains this local secret and must not be published. UI settings are stored in `%APPDATA%/spotify-lyrics-hud`; per-track timing is stored under Spotify's `LyricsHUD` directory. Source packages exclude connection secrets, personal settings and timing data.

Lyric queries send the song title, artist and duration to the relevant provider. You do not need to enter Spotify or NetEase account passwords into this software. Lyrics remain the property of their respective rights holders.

This is not an official Spotify product. The HUD and integration code are MIT licensed; the upstream license is preserved verbatim in `licenses/HUD-LICENSE.txt`. Spicetify CLI 2.46.4 is LGPL-2.1 and its license and corresponding source are included in the installer. Spicetify Marketplace 1.0.12 is MIT licensed; its notices and startup modifications are documented in [CREDITS.md](CREDITS.md). Historical dependency notices remain in `licenses/`.

OpenAI models assisted with implementation.
