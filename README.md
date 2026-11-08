# Spotify Lyrics

[简体中文](README.zh-CN.md) · **English**

[Download the latest release](https://github.com/E77zL/spotify_lyrics/releases/latest) · [Original project](https://github.com/Fanbeixi/spotify-lyrics-HUD_CN)

## Installation and Usage

Download and run the [EXE](https://github.com/E77zL/spotify_lyrics/releases/latest), accept the license agreement, then choose to install, uninstall, or repair.  
The icon to the left of the microphone in Spotify's bottom-right playback controls toggles the lyrics window.  
The shopping cart in Spotify's top-left corner opens Spicetify themes, extensions, and CSS snippets.  
Drag the lyrics window to move it, or drag its bottom-right corner to resize it.  
During fullscreen games, mouse input passes through the lyrics window to prevent accidental clicks from taking focus away from the game.  
Uninstallation is also available through the installer.

## Showcase

<img width="1027" height="406" alt="Spotify Lyrics preview" src="https://github.com/user-attachments/assets/e98082e1-ec7a-4025-b987-02aab0d314d1" />
<img width="2560" height="1440" alt="Fullscreen lyrics preview" src="https://github.com/user-attachments/assets/06597c5d-26e0-44b4-8ddf-730a7f596f39" />

## Mouse Click-Through During Fullscreen Apps

<img width="800" height="450" alt="Automatic mouse click-through demonstration" src="https://github.com/user-attachments/assets/5c965fc5-037a-4c3b-b1ec-4df9062bf8be" />

## Custom Colors and Opacity

<img width="987" height="408" alt="Custom color settings" src="https://github.com/user-attachments/assets/af28e327-8a59-4be2-b24f-d9fe0f0dfdc8" />
<img width="942" height="354" alt="Custom opacity settings" src="https://github.com/user-attachments/assets/8a756099-032e-40d8-bb89-3c36c1ebb93c" />

## Credits

A derivative of **[Fanbeixi / spotify-lyrics-HUD_CN](https://github.com/Fanbeixi/spotify-lyrics-HUD_CN)**.  
Adds Spotify startup integration, an in-app toggle, an improved interface, and per-track lyric timing adjustments.  
Full attribution: [Credits](CREDITS.md).

## How It Works

The software adds `version.dll` and `LyricsHUD/bridge.ini` to the Spotify installation directory.  
The module forwards Windows version queries to the system DLL and starts the lyrics component only in Spotify's main process.  
If another plugin already uses a DLL with the same name, the installer refuses to overwrite it.  
Uninstallation removes only this project's integration module and preserves per-track timing adjustments.

## Changelog

[View release notes](https://github.com/E77zL/spotify_lyrics/releases)

## Data and Licensing

The control service listens only on `127.0.0.1:27946` and uses a randomly generated authentication token created during installation. Connection details are stored in `%LOCALAPPDATA%/SpotifyLyricsCompanion/connection.json`.  
This file contains a local secret and is not part of the published source code.  
Interface settings are stored in `%APPDATA%/spotify-lyrics-hud`.  
Source packages exclude connection secrets, per-track timing adjustments, and personal settings.

Lyric searches send the song title, artist, and duration to the relevant service. No Spotify or NetEase account password is required.  
This is not an official Spotify product. Lyrics belong to their respective rights holders.  
The project and its integration code are MIT licensed. The original author's license is preserved in `licenses/HUD-LICENSE.txt`.  
Spicetify CLI 2.46.4 is licensed under LGPL-2.1. Its corresponding source code and full license are included in the installer.  
Spicetify Marketplace 1.0.12 is MIT licensed. Its full license is available in `licenses/MARKETPLACE-LICENSE.txt`. The integrated version disables catalog preloading at startup; sources and modifications are documented in [Credits](CREDITS.md).  
Electron/Chromium license notices are retained. Details about other dependencies and modifications are available in [CREDITS.md](CREDITS.md) and [NOTICE.txt](NOTICE.txt).  
See [RELEASING.md](RELEASING.md) for GitHub publishing and attribution instructions.

Implemented with OpenAI models.
