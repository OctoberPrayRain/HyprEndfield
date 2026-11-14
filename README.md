# HyprEndfield （源项目:[EndfieldHUD](https://github.com/DDDuoDuo/EndfieldHUD)）
<img width="2560" height="1440" alt="image" src="https://github.com/user-attachments/assets/2b3e0c09-18b4-426d-b2cf-ec177305d778" />


# 欢迎Linux用户提交Issue/Pr

HyprEndfield 把 [EndfieldHUD](https://github.com/DDDuoDuo/EndfieldHUD) 的 HUD 界面移植到了 Linux。项目使用 C++20、Qt 6 / QML 和 LayerShellQt，开发与日常使用环境是 Arch Linux + Hyprland，也支持 niri。

表盘、卡片、动画和素材尽量沿用原项目，主表盘使用 EndfieldHUD 的分层三维资源。这个仓库目前只提供源码，没有安装包。

## 能做什么

- 显示全屏 HUD，支持三维表盘、展开和收起动画、视差、倾斜、主题、缩放、时钟、模块排序和显隐。
- 使用便笺、视频便笺、待办、绘图、剪贴板、文件暂存架、档案库、阅读器、日历、快捷终端和应用快捷方式。
- 控制 SPlayer，显示封面、进度和歌词；调整音频设备和应用音量。
- 加工图片和视频，在全屏投影中绘图、显示点阵背景、放置图片和播放视频。
- 查看存储、CPU、内存、GPU、网络、电池和设施警报，使用倒计时与正计时专注模式。
- 绑定鹰角账号，显示终末地资料和明日方舟理智信息；也可以使用独立的 Linux 自定义名片。
- 运行“融合！山团团！”小游戏，查看 HUD 自身的事件日志。

地图模块已经移除。

## 编译和运行

完整依赖以 [CMakeLists.txt](CMakeLists.txt) 为准。编译需要 CMake 3.25+、C++20 编译器、Qt 6.8+、LayerShellQt、libpulse、libvterm 0.3.3+、Wayland、libsecret、PAM、libzip 和 GIO。

Arch Linux 可以安装以下软件包：

```bash
sudo pacman -S --needed base-devel cmake ninja pkgconf \
  qt6-base qt6-declarative qt6-wayland qt6-shadertools \
  qt6-multimedia qt6-websockets qt6-webengine qt6-imageformats \
  layer-shell-qt libpulse libvterm wayland libsecret pam libzip glib2
```

编译并启动：

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build --target hyprendfield -j2
./build/bin/HyprEndfield --background
./build/bin/HyprEndfield --toggle
```

程序还接受 `--show`、`--hide` 和 `--quit`。视频相关功能需要 FFmpeg、ffprobe 和 ffmpegthumbnailer；存储页面默认打开 Dolphin；部分文件粘贴操作需要 `wl-paste`。

## 桌面配置

HUD 是一个 layer-shell Overlay 图层，不占用普通窗口的焦点，所以全局热键必须由合成器绑定。本项目默认用 **Super + Esc** 打开或关闭 HUD。把下面的程序路径换成实际路径。

显示和隐藏都走同一个 `--toggle` 命令，由单实例 socket 转交给已经在运行的进程。

### Hyprland

```lua
local hyprEndfield = "/absolute/path/to/HyprEndfield"

hl.bind("SUPER + ESCAPE", hl.dsp.exec_cmd(hyprEndfield .. " --toggle"))

hl.define_submap("hyprendfield", function()
    hl.bind("SUPER + ESCAPE", function()
        hl.dispatch(hl.dsp.submap("reset"))
        hl.dispatch(hl.dsp.exec_cmd(hyprEndfield .. " --toggle"))
    end)
    hl.bind("PRINT", hl.dsp.exec_cmd("hyprshot -m output"))
    hl.bind("CTRL + ALT + A", hl.dsp.exec_cmd("hyprshot -m region --clipboard-only --silent"))
end)

hl.layer_rule({
    name = "hyprendfield-frosted-background",
    match = { namespace = "^hyprendfield$" },
    blur = true,
    ignore_alpha = 0.1,
})
```

HUD 显示时会进入 `hyprendfield` 子键位配置，隐藏时恢复普通键位。

### niri

niri 的键位表不能在运行时替换，但它支持 `zwp_keyboard_shortcuts_inhibit_v1`：HUD 拿到焦点后会向合成器申请抑制快捷键，隐藏时解除。所以只需要一条常驻绑定：

```kdl
binds {
    Mod+Escape allow-inhibiting=false { spawn "/absolute/path/to/HyprEndfield" "--toggle"; }
}
```

`allow-inhibiting=false` 让这条绑定在 HUD 抑制快捷键期间依然生效，否则开了 HUD 就关不掉了。

如果这个组合键在你本地已经被占用（比如已经绑给 `toggle-keyboard-shortcuts-inhibit`），换成别的即可，比如 `Mod+Shift+Escape`。

要让背景模糊生效，给这个图层命名空间加一条 layer-rule：

```kdl
layer-rule {
    match namespace="^hyprendfield"
    geometry-corner-radius 8
    background-effect {
        xray false
        blur true
    }
}
```

快捷键抑制用的是 niri 的 `toggle-keyboard-shortcuts-inhibit` action，它是**焦点图层上的开关**，所以只在 HUD 确实获得焦点后才申请（见 `ApplicationController::activateCompositorMode`）。如果某次退出时焦点已经跑掉，这个开关会反向切一次，再打开时又会切回来，两次互相抵消；任何时候按 niri 自己绑定的这个 action 也能恢复默认。

### 其它桌面

两者都不匹配时，程序照常运行：layer-shell 部分走标准协议，全局热键需要你自己绑定，HUD 打开时快捷键不会被抑制（Esc 只在 HUD 有焦点时有效）。HUD 内的窗口列表信息（活动监视器）在不支持的桌面上会退化为只按进程名匹配。

## 还没有完成的内容

- 只在 Arch Linux 上完整使用过；Hyprland 与 niri 已适配，其它发行版和桌面需要自行适配。
- 没有 X11 后端，也没有 DEB、RPM、AUR 或 Flatpak 包。
- 当前播放只识别 SPlayer，歌词也使用 SPlayer 的专用接口。
- “模糊程度”滑块暂不生效，实际模糊由合成器的图层规则控制（Hyprland 的 `layer_rule`、niri 的 `layer-rule`）。
- 影像加工不支持高光、阴影、视频贴纸和 HDR 视频导出。
- 存储页面只有容量信息，没有文件分类、大文件查找、清理和分区操作。
- 专注计时在隐藏 HUD 后继续运行，退出应用后不会恢复正在进行的计时。
- 更新功能只检查远端提交，不会自动下载和安装新版本。

## 代码导航

[qml/](qml/) 保存界面，[src/](src/) 保存数据和系统功能，[assets/](assets/) 保存素材，[shaders/](shaders/) 保存表盘着色器，[tools/](tools/) 保存素材导入脚本。

### 程序和主界面

- 程序入口、单实例、托盘和组件注册：[src/main.cpp](src/main.cpp)
- Wayland 图层、显示隐藏、屏幕选择和桌面集成（Hyprland 子键位 / niri 快捷键抑制）：[src/applicationcontroller.cpp](src/applicationcontroller.cpp)、[qml/Main.qml](qml/Main.qml)
- 表盘、菜单和页面装载：[qml/WatchShell.qml](qml/WatchShell.qml)、[qml/MenuScene.qml](qml/MenuScene.qml)
- 卡片和菜单几何：[qml/MenuPlane.qml](qml/MenuPlane.qml)、[qml/MenuCard.qml](qml/MenuCard.qml)、[qml/MenuTile.qml](qml/MenuTile.qml)、[qml/MenuGeometry.js](qml/MenuGeometry.js)
- 三维表盘、相机、材质和着色器：[src/sourcedial.cpp](src/sourcedial.cpp)、[assets/watch-dial/](assets/watch-dial/)、[assets/watch/](assets/watch/)、[shaders/](shaders/)
- 动画时序：[src/watchanimationcontroller.cpp](src/watchanimationcontroller.cpp)、[assets/watch/clips.json](assets/watch/clips.json)
- 系统设置、显示设置、模块顺序和翻译：[src/settingscontroller.cpp](src/settingscontroller.cpp)、[qml/SystemPage.qml](qml/SystemPage.qml)、[qml/DisplayPage.qml](qml/DisplayPage.qml)、[src/localization_catalog.inc](src/localization_catalog.inc)
- MOD 编号：[qml/ModuleNumbering.js](qml/ModuleNumbering.js)

新增组件时，还要检查 [CMakeLists.txt](CMakeLists.txt)、`src/main.cpp`、`MenuScene.qml`、`WatchShell.qml`、`SettingsController::defaultModules()` 和 `SystemPage.qml`。

### 功能组件

- 便笺和视频便笺：[qml/NotesPage.qml](qml/NotesPage.qml)、[qml/NotesWorkspace.qml](qml/NotesWorkspace.qml)、[src/notescontroller.cpp](src/notescontroller.cpp)
- 剪贴板：[qml/ClipboardPage.qml](qml/ClipboardPage.qml)、[src/clipboardcontroller.cpp](src/clipboardcontroller.cpp)
- 文件暂存架：[qml/FileShelfPage.qml](qml/FileShelfPage.qml)、[src/fileshelfcontroller.cpp](src/fileshelfcontroller.cpp)
- 档案库：[qml/ArchivePage.qml](qml/ArchivePage.qml)、[src/archivecontroller.cpp](src/archivecontroller.cpp)
- 阅读器：[qml/ReaderPage.qml](qml/ReaderPage.qml)、[src/readercontroller.cpp](src/readercontroller.cpp)
- 日历和专注模式：[qml/CalendarPage.qml](qml/CalendarPage.qml)、[src/calendarcontroller.cpp](src/calendarcontroller.cpp)、[qml/FocusModePage.qml](qml/FocusModePage.qml)、[src/focusmodecontroller.cpp](src/focusmodecontroller.cpp)
- 快捷终端：[qml/TerminalPage.qml](qml/TerminalPage.qml)、[src/terminalsession.cpp](src/terminalsession.cpp)、[src/terminalview.cpp](src/terminalview.cpp)
- 当前播放和歌词：[qml/NowPlayingPage.qml](qml/NowPlayingPage.qml)、[src/nowplayingcontroller.cpp](src/nowplayingcontroller.cpp)、[src/splayerlyrics.cpp](src/splayerlyrics.cpp)
- 音量：[qml/VolumePage.qml](qml/VolumePage.qml)、[src/audiocontroller.cpp](src/audiocontroller.cpp)
- 影像加工：[qml/MediaAssemblyPage.qml](qml/MediaAssemblyPage.qml)、[src/mediaeditor.cpp](src/mediaeditor.cpp)、[src/mediaengine.cpp](src/mediaengine.cpp)
- 投影、绘图和点阵背景：[qml/ProjectionPage.qml](qml/ProjectionPage.qml)、[qml/ProjectionWorkspace.qml](qml/ProjectionWorkspace.qml)、[src/projectioncanvas.cpp](src/projectioncanvas.cpp)
- 存储、活动监视器和设施警报：[qml/StoragePage.qml](qml/StoragePage.qml)、[src/storagecontroller.cpp](src/storagecontroller.cpp)、[qml/ActivityPage.qml](qml/ActivityPage.qml)、[src/activitycollector.cpp](src/activitycollector.cpp)、[src/facilitycontroller.cpp](src/facilitycontroller.cpp)
- 电池：[qml/BatteryBadge.qml](qml/BatteryBadge.qml)、[src/batterycontroller.cpp](src/batterycontroller.cpp)
- 两套个人名片：[qml/ProfilePage.qml](qml/ProfilePage.qml)、[qml/MenuBadge.qml](qml/MenuBadge.qml)、[qml/GameProfileBadge.qml](qml/GameProfileBadge.qml)、[src/profilecontroller.cpp](src/profilecontroller.cpp)
- 账号绑定和理智信息：[qml/AccountPage.qml](qml/AccountPage.qml)、[src/accountcontroller.cpp](src/accountcontroller.cpp)、[src/accountlogin.cpp](src/accountlogin.cpp)、`src/endfield/modules/hypergryph_account_*`
- “融合！山团团！”：[qml/OrbiPomPage.qml](qml/OrbiPomPage.qml)、[src/orbipomcontroller.cpp](src/orbipomcontroller.cpp)、[assets/orbipom/](assets/orbipom/)
- 事件日志：[qml/EventLogPage.qml](qml/EventLogPage.qml)、[src/eventlogcontroller.cpp](src/eventlogcontroller.cpp)、[src/endfield/modules/event_log.cpp](src/endfield/modules/event_log.cpp)
- 应用快捷方式：[qml/AppShortcutsPage.qml](qml/AppShortcutsPage.qml)、[src/appshortcutscontroller.cpp](src/appshortcutscontroller.cpp)

## 移植入口

更换 Linux 发行版时，按 [CMakeLists.txt](CMakeLists.txt) 中的 Qt 组件和 pkg-config 名称安装对应开发包。协议文件不在默认位置时，编译时传入：

```bash
-DWLR_DATA_CONTROL_PROTOCOL=/absolute/path/to/wlr-data-control-unstable-v1.xml
```

更换桌面时，先看这三个位置：

- HUD 覆盖、屏幕和快捷键模式：[src/applicationcontroller.cpp](src/applicationcontroller.cpp)
- 桌面窗口信息（Hyprland `hyprctl clients` / niri `niri msg windows`）：[src/activitycollector.cpp](src/activitycollector.cpp)
- Wayland 剪贴板：[src/clipboardcontroller.cpp](src/clipboardcontroller.cpp)

更换默认软件时，对应入口如下：

- SPlayer：[src/nowplayingcontroller.cpp](src/nowplayingcontroller.cpp)、[src/splayerlyrics.cpp](src/splayerlyrics.cpp)
- Dolphin：[src/storagecontroller.cpp](src/storagecontroller.cpp)、[src/fileshelfcontroller.cpp](src/fileshelfcontroller.cpp)
- Bash：[src/terminalsession.cpp](src/terminalsession.cpp)
- PulseAudio 后端：[src/audiocontroller.cpp](src/audiocontroller.cpp)
- 名片中的 `ARCH` 和合成器名称（`appController.compositorName`）：[qml/MenuBadge.qml](qml/MenuBadge.qml)

## 数据位置

设置保存在 `$XDG_CONFIG_HOME/HyprEndfield/HyprEndfield.conf`。组件数据位于 `$XDG_DATA_HOME/HyprEndfield/` 和 Qt 的 HyprEndfield 应用数据目录。

账号凭据与档案库密码验证记录使用桌面的 Secret Service。Linux 用户密码只交给 PAM 验证。剪贴板历史只保存在内存中，退出应用后清空。

## 来源和许可

移植时使用的 EndfieldHUD 参考提交为 [`e9a6bdc2`](https://github.com/DDDuoDuo/EndfieldHUD/tree/e9a6bdc2fe468008c830132715998161f002b902)。原项目的[中文说明](https://github.com/DDDuoDuo/EndfieldHUD/blob/e9a6bdc2fe468008c830132715998161f002b902/README.zh-CN.md)介绍 macOS 和 Windows 版本。

许可信息见 [LICENSE](LICENSE)、[src/endfield/LICENSE](src/endfield/LICENSE)、[assets/settings/EndfieldHUD-LICENSE](assets/settings/EndfieldHUD-LICENSE)、[Matter.js 许可证](assets/orbipom/Matter-LICENSE.txt)和 [HarmonyOS Sans 许可证](assets/fonts/HarmonyOS-Sans-LICENSE.txt)。这是非官方同人移植，游戏素材、商标和第三方资源仍属于各自权利人。
