# Spotify Lyrics Better HUD

**简体中文** · [English](README.en.md)

基于 **[Fanbeixi / spotify-lyrics-HUD_CN](https://github.com/Fanbeixi/spotify-lyrics-HUD_CN)** 的衍生项目  
在原作者基础上添加Spotify启动联动、图标摁钮开关、更好的UI和逐曲校时  
完整署名见 [学分](CREDITS.md)  

[下载最新构建版](https://github.com/E77zL/spotify-lyrics-better-hud/releases/latest) · [原作者项目](https://github.com/Fanbeixi/spotify-lyrics-HUD_CN)

## 效果展示

<img width="1500" height="1000" alt="image" src="https://github.com/user-attachments/assets/8fcde938-c67e-45d6-b507-bc0b4bc1a535" />
<img width="2560" height="1440" alt="image" src="https://github.com/user-attachments/assets/50b27097-ee33-426e-95b5-2a8e89d79ef8" />

## 全屏模式下穿透鼠标

<img width="800" height="450" alt="d3500313488a394f041060a4eaf6962d-ezgif com-video-to-gif-converter" src="https://github.com/user-attachments/assets/4d2e6a5c-8a38-406d-b34a-5585d1e6998e" />

## 自定义颜色和自定义透明度

<img width="954" height="388" alt="image" src="https://github.com/user-attachments/assets/3ee0f1ce-0967-4773-b564-ad81ef69db77" /> 
<img width="919" height="342" alt="image" src="https://github.com/user-attachments/assets/4a1d8029-215c-49d4-a7de-7246d9ccd8e9" />


## 安装和使用

支持Windows 10以上系统，Spotify官网桌面版  
下载Release中最新的exe程序，双击运行安装器，接受许可后选择 **Install / Repair** 安装/修复
UI右下角麦克风左侧的图标为开关浮窗摁钮  
Spotify左上角的购物车可以浏览Spicetify主题、扩展和 CSS 小组件
窗口可拖动大小，鼠标抓住窗口右下角落缩放  
在全屏游戏时会让鼠标穿透浮窗以免跳出游戏进程  

## 功能

没有独立Launcher、开机监视程序或自动更新器  
本软件仅在Spotify安装目录添加`version.dll`和`LyricsHUD/bridge.ini`
模块将版本查询转发到Windows系统DLL，只在Spotify主进程启动歌词  
已有其它插件的同名 DLL 时会拒绝覆盖  
升级会移除旧软件和它的开机启动项 (老版本实现歌词的方法是自启动独立进程)  
卸载只删除本项目模块，保留逐曲校时  

## 更新日志

[查看最新构建更新日志](https://github.com/E77zL/spotify-lyrics-better-hud/releases)

## 数据与授权

控制服务仅监听 `127.0.0.1:27946`，使用安装时随机生成的令牌；连接文件保存在 `%LOCALAPPDATA%/SpotifyLyricsCompanion/connection.json`  
它包含本机密钥，不属于发布源码  
界面设置保存在 `%APPDATA%/spotify-lyrics-hud`  
源码包不含你的连接密钥、歌曲校时或个人设置  

歌词查询会向相应服务发送歌名、歌手和曲长，不需要输入 Spotify 或网易云密码  
本项目不是 Spotify 官方产品；歌词版权归相应权利人所有  
HUD 与本仓库集成代码为 MIT；原作者许可原样保留在 `licenses/HUD-LICENSE.txt`  
Spicetify CLI 2.46.4 为 LGPL-2.1，安装器附带对应源码与完整许可  
Spicetify Marketplace 1.0.12 为 MIT，完整许可见 `licenses/MARKETPLACE-LICENSE.txt`；集成版取消启动时的目录预加载，来源及修改见 [学分](CREDITS.md)  
Electron/Chromium   
许可保留在运行时中其它依赖及修改范围见 [CREDITS.md](CREDITS.md) 和 [NOTICE.txt](NOTICE.txt)  
GitHub 上传与署名步骤见 [RELEASING.md](RELEASING.md)
OpenAI大模型代替人手
