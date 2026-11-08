# Credits / 致谢与来源

本衍生仓库由 [E77zL](https://github.com/E77zL) 维护。

当前独立仓库为 [E77zL/spotify_lyrics](https://github.com/E77zL/spotify_lyrics)；此前的衍生仓库为 [E77zL/spotify-lyrics-better-hud](https://github.com/E77zL/spotify-lyrics-better-hud)。独立建仓只改变 GitHub 仓库关联，不改变上游来源、版权和许可证义务。

This independent repository derives from **Fanbeixi/spotify-lyrics-HUD_CN**, maintained here by **E77zL**. Creating a standalone repository does not remove upstream authorship or license obligations. Original copyright notices and complete licenses are retained. See the component table below and `NOTICE.txt` for sources, versions and modifications.

本项目是衍生集成，主要歌词窗口与最初实现来自 **Fanbeixi**，不是从零原创的歌词软件。

| 来源 | 使用范围 | 授权 |
| --- | --- | --- |
| [Fanbeixi/spotify-lyrics-HUD_CN](https://github.com/Fanbeixi/spotify-lyrics-HUD_CN)，HUD 5.1.0 | 歌词窗口的视觉设计、LRC 工具及 QQ 查询基础；1.3.0 窗口已改为原生实现 | MIT；原文见 `licenses/HUD-LICENSE.txt` |
| [Spicetify CLI](https://github.com/spicetify/cli)，2.46.4 | Spotify 客户端修改工具、A 按钮与自定义页面加载 | LGPL-2.1；见 `licenses/SPICETIFY-LICENSE`，对应源码随安装器提供 |
| [Spicetify Marketplace](https://github.com/spicetify/marketplace)，1.0.12 | Spotify 内的主题、扩展、CSS 小组件目录及安装管理 | MIT；版权 CharlieS1103 + theRealPadster，完整许可见 `licenses/MARKETPLACE-LICENSE.txt` |
| [Coooookies / node-windows-smtc-monitor](https://github.com/LeagueTavern/node-windows-smtc-monitor)，1.0.12 | Windows 系统媒体信息回退 | MIT；见 `licenses/SMTC-LICENSE.txt` |
| [websockets/ws](https://github.com/websockets/ws)，8.22.0 | 认证的本地 WebSocket 通信 | MIT；见 `licenses/WS-LICENSE.txt` |
| [Electron](https://github.com/electron/electron)，31.7.7 / Chromium | 历史版本所用运行时，1.3.0 不分发 | 历史 Electron MIT 文本保留于 `licenses/ELECTRON-LICENSE.txt` |
| [LRCLIB](https://lrclib.net/)、QQ 音乐、网易云音乐 | 歌词数据来源 | 歌词版权归相应权利人所有，不作为本项目的 MIT 代码再授权 |

衍生集成修改包括：Spotify 启动/退出联动、A 开关和快速淡入淡出、稳定的歌词缩放布局、网易云兜底、三源歌曲身份校验、本地文件元数据兼容、逐曲 ±0.5 秒校时、Spotify 客户端时钟桥接、请求取消与有限缓存，以及 WPF 深色图形单文件安装器和构建测试。

上游发布包为 `v5.1.0/SpotifyLyricsHUD-Setup-5.1.0.exe`，SHA256：

```text
1fb3df8b7bb67d1c150c4a86827091bd2bdecb8cad08ffc27c5124f6d86e93e9
```

当前构建使用仓库内的衍生源码和固定版本依赖，不把本机安装目录或用户设置当成源文件。`app/` 中继承的代码和图标仍受上游 MIT 许可约束；修改文件不代表取得原作者的版权。发布时请保留本文件、`NOTICE.txt`、根目录 `LICENSE` 及 `licenses/` 全部原文。

Native 1.3.0 overlay uses nlohmann/json v3.11.3, copyright Niels Lohmann, MIT; see licenses/JSON-LICENSE.txt. Provider modules derive from Fanbeixi HUD and retain their original MIT attribution.

Marketplace 1.0.12 official release archive SHA256: `43c8ccff189636cf2a8e8b5ca77e87487d22599ff8dc53b692170f92cbbb4e16`.
Upstream source: https://github.com/spicetify/marketplace/tree/v1.0.12 .
Integration changes are reproducible in `scripts/Patch-Marketplace.cjs`: remove eager catalog preloading and its global sessionStorage reset, skip idle CDN probes, and retain normal installed theme/extension/snippet loading. Other Marketplace code is preserved from the pinned upstream release.
Marketplace dependency copyright and license notices are retained in `licenses/MARKETPLACE-THIRD-PARTY.txt` and installed beside its JavaScript files.
