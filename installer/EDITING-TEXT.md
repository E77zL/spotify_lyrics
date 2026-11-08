# 安装器界面文案修改

主要修改 `SetupStrings.xml`，每个条目中的 `<en>` 为英文，`<zh>` 为中文。两种语言并排放置，便于核对。

```xml
<string key="options.title">
  <en>Make yourself at home</en>
  <zh>准备开始</zh>
</string>
```

只修改标签中间的文字，保留 `key`、`target`、`property` 和 XML 结构。文本中的 `&` 写为 `&amp;`，`<` 写为 `&lt;`。不要改动原作者许可证和版权声明。

- `license.*`：许可页说明与接受按钮。
- `options.*`、`action.*`：操作选择页。
- `button.*`、`link.*`：按钮文字与悬停提示。
- `guide.*`：快速指南。
- `working.*`、`success.*`、`error.*`：进行中、完成和失败状态。
- `dialog.*`：文件选择窗口和提示标题。

`SetupWindow.xaml` 管理布局、颜色和图标；`SetupWindow.cs` 管理交互；`SetupText.cs` 读取文案。界面中的安装日志和系统错误保留后端原文，相关源码为 `ConsoleActions.ps1`、`Bridge.ps1`、`Dependency.ps1` 和 `Marketplace.ps1`。启动前的错误提示及命令行帮助位于 `Installer.cs`。

修改后在完整仓库根目录执行：

```powershell
.\Build.ps1 -Offline
```

文案会嵌入单文件 EXE，安装器运行时不需要附属 XML 文件。单独的界面源码包用于修改与审阅，不包含编译所需的全部歌词组件；完整项目中保留对应目录即可构建。

你也可以只修改这份 XML 后交回来，由维护者重新编译。安装器每次启动默认使用英文，左下角“文/A”按钮随时切换中英文。
