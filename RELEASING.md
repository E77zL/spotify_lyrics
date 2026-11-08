# 上传 GitHub 与原作者署名

## 上传源码

本项目在 [E77zL/spotify_lyrics](https://github.com/E77zL/spotify_lyrics) 作为独立衍生仓库维护，不再使用 GitHub Fork 关联。原作者链接、版权与许可继续保留；旧仓库不受影响。

`README.md` 为中文主页，`README.en.md` 为英文说明，两份文档顶部互相链接。更新功能、安装步骤和版本时同步维护两种语言，项目代码注释使用英文。

把源码 ZIP **解压后的内容**作为仓库根目录上传，确保根目录直接有 `README.md`、`Build.ps1` 和 `app/`，包括以点开头的文件。不要上传本机的 `outputs/`、安装目录、`connection.json`、校时缓存、`.cache/`、`.build/` 或 `dist/`。发布包已经排除这些内容。

根目录 `LICENSE` 是集成代码的 MIT；`licenses/HUD-LICENSE.txt` 是原作者 MIT，必须保留原来的版权行和完整正文。`CREDITS.md` 与 `NOTICE.txt` 说明了原作和你的修改范围，不要删除。可以在集成贡献者名单中添加自己的 GitHub 用户名，勿把原作者替换成自己。

## 发布可执行文件

源码构建需要 Node.js 22+、Visual Studio 2022 或更新版本的 C++ 工具链（C++20）、Windows SDK 10.0.22000+，以及 Windows 自带的 .NET Framework 4.x / WPF 编译组件。在仓库根目录运行 `powershell -NoProfile -ExecutionPolicy Bypass -File .\Build.ps1` 生成轻量包，添加 `-Offline` 同时生成内置Spicetify程序包的完整离线版；已有依赖缓存可添加 `-CacheDir <缓存目录>`。两种单文件安装器均使用深色图形界面，并内置对应源码和许可证；可通过底部源码图标导出。安装器提供 Install / Repair、Disable integration、Check environment，仍保留命令行参数以便自动化。轻量版仅在没有已安装的Spicetify时下载锁定版本并校验SHA256，不检查新版本。Windows Composition和解压缩由系统提供，无需额外运行库安装包。

1. 仓库选择 **Releases → Draft a new release**。
2. 创建标签 **v2.0.3**，标题可写 `Spotify Lyrics HUD Companion 2.0.3`。
3. 上传 `Spotify-Lyrics-HUD-Setup-v2.0.3.exe`、`Spotify-Lyrics-HUD-Setup-v2.0.3-Offline.exe`、`Spotify-Lyrics-HUD-Source-v2.0.3.zip` 和 `SHA256SUMS.txt`。
4. 在说明中写明这是基于 Fanbeixi 项目的衍生集成，并粘贴下方署名段落。

编译后的单文件 EXE 应放在 **Release 附件**中，源码提交到 Git 仓库。本仓库不包含 GitHub Actions 自动构建文件；在本机运行 `Build.ps1` 生成产物后，手动上传到 Release。

## 可直接使用的署名

```markdown
### Credits

本项目基于 [Fanbeixi/spotify-lyrics-HUD_CN](https://github.com/Fanbeixi/spotify-lyrics-HUD_CN) 开发，感谢原作者提供歌词窗口与基础实现。
本衍生版本添加了 Spotify 启动联动、客户端 A 开关、网易云兜底、逐曲校时与播放时钟同步等功能。
保留原项目 MIT 许可证与版权声明，详细第三方来源见 CREDITS.md 和 licenses/。
```

MIT 不要求给原作者付费或额外索取许可，要求保留版权与许可声明。README 致谢和上游链接能让用户看清来源；单独点一个 Star 不能替代许可证保留。Spicetify 的 LGPL 许可与对应源码也已随安装器提供，发布时继续保留。
