## 2.0.3

- Replace the console menu with a compact dark WPF installer: license page, operation selection, asynchronous progress/logs, retry and source export. Keep the verified payload, administrator support and single-file packaging; no browser runtime is bundled.
- Prepare the independent spotify_lyrics repository with Chinese/English README navigation and preserved upstream attribution; use English source comments.
- Preserve the 2.0.0 HUD and settings-flyout radii (16/14 px). Fill the background once and apply a cached, symmetric alpha edge; remove the competing Windows clip and border from foregrounds and glass backdrops. Repair corner tint gaps and hard seams without changing opacity or blur strength, adding rendering layers or scanning the full surface.

## 2.0.2

- Animate fullscreen arrow seeks from the current lyric position with the same easing as normal playback; keep the requested lyric clock immediate and retarget smoothly during rapid or reversed seeks.
- Disable the extra Windows DWM border and system rounding on the layered HUD, every settings flyout and their glass backdrops. Keep the custom rounded background and antialias coverage, removing the mismatched dark/gray arcs at all four corners, including at 100% opacity.

## 2.0.1

- Update lyrics immediately when fullscreen seek shortcuts are pressed. Bridge the requested position until Spotify's playback cache catches up, so periodic snapshots cannot briefly restore the old time.
- Accumulate repeated and reversed seeks from the pending target. Reconcile confirmations, failed commands, track changes and a bounded timeout; temporary confirmation polling stops when finished.
- Snap lyric position and highlighting on seeks, including external timeline jumps, while retaining smooth scrolling during normal playback and reusing cached layouts.

## 2.0.0

- Add focused fullscreen playback controls: Left/Right seek 5 seconds, Up/Down change Spotify volume by 5%, and Space toggles playback without key-repeat toggling. No global keyboard hook or system volume changes.
- Drag the fullscreen top strip to restore the previous window size and continue moving under the pointer; simple clicks stay fullscreen. Preserve frame-paced dragging and continuous glass.
- Integrate Spicetify Marketplace 1.0.12 inside Spotify, with a native navigation entry and a Customization profile-menu shortcut. Keep the A button dedicated to the desktop lyrics HUD.
- Browse and choose themes, extensions and CSS snippets; browse custom apps with their manual installation instructions. Do not enable third-party items by default.
- Disable Marketplace's startup-wide catalog preload and global sessionStorage clearing; skip CDN probing when no remote theme or extension is installed. Keep installed item loading and local snippets.
- Pin and verify Marketplace release assets, reuse independent installations, retain existing themes, and preserve Marketplace licensing and source attribution. Both installer variants include the Marketplace payload.

## 1.3.5

- Compress installer resources with the system .NET decoder. Default to a lightweight single EXE that reuses installed Spicetify or downloads its pinned, SHA256-verified archive on first installation; provide an Offline build with the complete archive. Both include all corresponding source and licenses.
- Release lyric and toolbar raster surfaces when the HUD finishes fading out, retain layouts for quick reopening, clear stale layouts on track changes, defer unused backdrop creation, and remove duplicate hidden health serialization.
- Enable whole-program optimization and unused/identical code elimination for the native DLL; reserve known-size layout buffers and avoid rebuilding the provider host allowlist per request.

## 1.3.4

- Replace the legacy Accent tint workaround with a Windows Composition host-backdrop visual. Blend blur over 0–40% while keeping text and controls crisp; 0% hides the blur, 100% paints solid and skips the covered backdrop. Lyrics and flyouts share the same transition.
- Slightly increase the low-range glass blend by at most one percentage point; preserve the transparent/solid endpoints and the 40–100% range.
- Keep all composition windows inside Spotify, off the taskbar and unable to accept input. Reuse one compositor per HUD thread, update properties only when changed, and retain translucent tint if host backdrop is unsupported. No desktop capture or extra rendering process.
- Keep blank areas interactive at 0% background opacity; prevent intermittent hover controls and failed dragging caused by Windows layered-window alpha hit testing. Apply the same fix to settings flyouts; fullscreen-game click-through remains explicit.
- Keep a minimum 1/255 background alpha for interaction, without changing saved opacity or fullscreen-game click-through.

## 1.3.3

- Restore multiline lyrics with automatic width-based wrapping and no fixed row limit, including long words and Unicode text.
- Space lyric blocks by their measured height; preserve smooth scrolling and place the breathing underline below the whole active phrase.
- Fit over-height phrases with a smaller effective font without changing the saved font preference; cache layouts across animation frames and reflow on resize.

## 1.3.2

- Rasterize toolbar text only when it changes, using a 32-pixel surface instead of a full-window surface; skip unnecessary fading layers for UI text.
- Parse appearance colors on settings changes, cache track labels and ignore unchanged paused playback snapshots.
- Share the native runtime version and derive release filenames from package.json; retain all 1.3.1 glass, fullscreen, animation and timing behavior.
- Use explicit ownership when posting transport messages so failed window delivery releases allocated messages.

## 1.3.1

- Replace the modal system drag loop with captured input and frame-paced geometry updates. Keep backdrop blur enabled throughout dragging/resizing, coalesce region changes and redraws, reuse the paused movement surface, and support Escape cancellation.
- Add a monitor fullscreen toggle with normal window position/size restoration and in-screen flyouts; extend font sizes to 120 px. Soften the slider green, remove the visible provider footer and give timing the same toolbar button dimensions.
- Pace animation with per-thread high-resolution 60 FPS deadlines and fractional text positioning; retain idle throttling without changing system timer resolution.
- Use glass sliders for typography, antialiasing and opacity; Tab opens precise input, dragging previews without writing to disk until release. Timing keeps its 0.5-second controls.
- Expand the old 0–5% tint range to 0–40% of the shared opacity slider. Accept six-digit color codes with or without #.

- Replace the settings gear and standalone settings window with compact Timing, Typography, Opacity and Colors flyouts in the lyric toolbar.
- Share adjustable 0–100% background opacity between lyrics and flyouts; remove the fixed extra tint, surplus bottom spacing and background-size entry (edge resizing remains).
- Consolidate per-track timing into one compact number; click to enter seconds or adjust in 0.5-second steps. Align toolbar labels and values at a shared size and baseline.
- Use rounded glass panels, shared vector icons and DirectWrite typography for every HUD label and input; remove native widget frames and color pickers.
- Anchor flyouts below the lyrics, or above when screen space is limited; validate and automatically save changes. Support numeric steps, typed values, hex-code paste and Escape.
- Add adjustable 0–100% grayscale antialiasing; preserve bold lyrics, the green breathing underline, glow and viewport fading.
- Cache text layouts and drawing surfaces; avoid repainting settled paused lyrics and skip unchanged settings writes.
- Retain Spotify process hosting, fullscreen click-through, lyric providers and per-track offsets.
## 1.3.0

- Replace the Electron HUD and fullscreen helper with native windows and threads inside Spotify.exe.
- Reuse LRCLIB → QQ → NetEase matching in Spotify’s existing renderer.
- Preserve per-song offsets, colors and saved window geometry.
- Native tool windows stay off the taskbar; fullscreen games retain mouse input.
- Remove the bundled Electron runtime and child-process launcher.
- Preserve the original 700-weight Segoe UI lyrics, 1.6-second green breathing underline, glow and fading viewport edges.

# Changelog

## 1.2.2

- 自动检测前台全屏和无边框全屏应用，歌词保持显示并切换为鼠标穿透、不可聚焦，避免点击歌词让游戏失去焦点。
- 切回普通窗口、最大化窗口或桌面后自动恢复拖动和按钮；锁定期间隐藏悬停工具栏，不转发鼠标移动，不修改系统或游戏的光标设置。
- 原生检测支持多显示器与不同 DPI；前台切换事件即时刷新，窗口样式变化最迟约 250 ms 复查。隐藏在后台的全屏应用不会触发锁定。
- 检测组件随 HUD 启动/退出，父进程异常退出后也会退出；检测失败会恢复鼠标操作，最多尝试恢复三次。

## 1.2.1

- 正式集成 Spotify 进程内 C++ 联动模块，替换独立 Launcher 和开机后台；原有 A 开关、歌词来源及逐曲校时保留。
- 主动从托盘退出后保持关闭，崩溃才延时恢复；释放不再使用的线程句柄和互斥量，并缓存系统版本 API 转发地址。
- 设置与逐曲偏移共用原子 JSON 写入，避免中断写入破坏旧文件；设置保存临时失败时，退出前可再次尝试。
- 模块、INI 和安装记录均使用完整替换；升级中途失败会恢复已修改文件，回滚受阻时保留恢复备份。
- 修复设置窗口内容被裁切、选择预设后颜色输入框不同步；新增当前预设选中标记。
- 增加实际管道退出、保存失败重试、主动退出不重启、锁定文件回滚及版本一致性回归检查。

版本号为 1.2.1，Windows 文件版本为 1.2.1.0；上游 HUD 运行时仍为 5.1.0。

## 1.2

- 用 Spotify 进程内 C++ 模块替换独立 Launcher，由 Spotify 直接启动歌词子进程；移除旧开机启动项和设置中的独立自启动选项。无自动更新功能。
- 优先使用 Spotify 客户端播放进度，修复系统进度迟到造成的普遍歌词滞后；客户端断线/超时自动回退 SMTC。
- 客户端的暂停、恢复、跳转与歌曲状态及时更新，重复的权威进度不再被误当作旧系统事件。
- 保留字体左侧 ±0.5 秒逐曲调整与 Spotify 安装目录内的缓存，已有设置继续有效。
- 保留 LRCLIB → QQ → 网易云、严格同名歌曲匹配、稳定缩放、快速淡入淡出及 Spotify 生命周期联动。
- 源码改为单一 app/ 树，移除旧版字符串替换构建链、未使用的配置/状态模块和重复维护文件。
- 增加锁定依赖的校验下载、可从干净目录执行的 Windows 构建、六组回归测试与安装器自检。
- README、Credits 和许可证明确原作者与第三方来源；发布源码不包含本机连接令牌或私人缓存。

衍生版本统一显示为 1.2；Node package 使用语义化版本 1.2.0，Windows 文件版本为 1.2.0.0。上游 HUD 运行时版本 5.1.0 单独标明。
