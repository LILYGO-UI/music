# LILYGO Music

一个使用 LVGL 9.5 / LILYGO UI AppKit 实现的本地音乐播放器。界面参考 iOS 的大标题、圆角卡片、清晰留白与分段切换；音乐库和播放区会根据实际容器尺寸在竖屏与横屏间自动重排。

功能包括本地音乐库扫描、搜索、歌曲选择、播放/暂停、上一首/下一首、进度拖动、音量、随机播放、循环播放和收藏。`assets/audio/` 中包含三段原创演示音乐和 Espressif ADF 双声道格式样本；设备测试时将这些文件上传到 `~/Music` 后即可被 Music 扫描。

功能参考 [T-Display-K230 的 k230_launcher](https://github.com/Xinyuan-LilyGO/T-Display-K230/tree/main/k230_launcher) 音乐应用。这里使用独立的 C++ MVVM 实现和 `mpv` 播放后端，通过本地 IPC 控制播放，不依赖该 Launcher 的源码或 K230 音频管道。

## 启动

开发环境需要 CMake 3.21+、C/C++17 编译器、`pkg-config`、SDL2、FreeType、LPM 和 `mpv`。macOS 可用 Homebrew 安装 `mpv`；Debian 设备包已声明 `mpv` 依赖。

```sh
# Git checkout 尚未初始化 SDK 时执行
git submodule update --init --recursive

cmake --preset host-simulator
cmake --build --preset host-simulator --parallel
ctest --preset host-simulator

# 指向自己的音乐目录，再启动模拟器
LILYGO_MUSIC_DIR="$HOME/Music" lpm start --no-build --foreground
```

`LILYGO_MUSIC_DIR` 指定一个音乐目录并覆盖默认扫描位置。未设置时扫描 `$HOME/Music`、`/root/music`、`/mnt/sdcard/music`、`/mnt/music` 和 `/media/music`，默认目录均无歌曲时加载开发环境中的示例。显式指定的目录为空或不存在时，显示空音乐库，不回退到示例。添加或删除歌曲后可重新扫描。

支持的文件扩展名为 MP3、WAV、FLAC、M4A、AAC、OGG、Opus、AIFF、AC3、MP4、TS 和 WMA，实际解码能力由 `mpv` 提供。扫描最多递归 8 层、收录 2048 首。歌曲标题主要取自文件名；WAV 时长可在扫描时读取，其他格式在播放后由 `mpv` 返回时长。

收藏、音量、随机和循环设置保存在 `$XDG_CONFIG_HOME/lilygo-ui-music/preferences`，未设置 XDG 路径时使用 `$HOME/.config/lilygo-ui-music/preferences`。

应用无需联网播放。模拟器和设备都需要在 `PATH` 中找到 `mpv`，且输出音频设备可用。缺少 `mpv` 时会提示安装；损坏文件或音频设备错误会显示播放失败，不会模拟播放进度。

## 测试与预览

测试覆盖不依赖 LVGL 的播放器模型、ViewModel 命令和 Subject 状态，以及两个标准视口的渲染/交互。预览图为：

| 竖屏 · 568 × 1232 | 横屏 · 1232 × 568 |
| --- | --- |
| ![竖屏音乐播放器](assets/previews/music-portrait.png) | ![横屏音乐播放器](assets/previews/music-landscape.png) |

```sh
ctest --preset host-simulator
```

主机渲染测试会生成 `build/host-simulator/music-portrait.ppm` 和 `music-landscape.ppm`。UI 使用一套 Flex 布局适配尺寸变化，不需要设置横屏/竖屏显示模式。

## 设备构建与打包

首次设备配置会下载锁定的 CM0 BSP 0.1.0、校验 SHA-256 后缓存到 `.cache/cm0-bsp/`；还需要可用的 AArch64 Linux C/C++ 交叉编译器。

```sh
cmake --preset cm0-cross
cmake --build --preset cm0-cross --parallel
cpack --config build/cm0-cross/CPackConfig.cmake -B dist
```

Debian 包 `lilygo-ui-music` 只安装可执行文件、Launcher manifest、图标、Desktop/AppStream 元数据及所需字体许可说明，不携带 `assets/audio/` 中的音频。设备测试时请将音频单独上传到 `$HOME/Music`，发布预览图也不进入设备包。

应用身份和产品元数据统一维护于 `lpm.toml`：

```text
包名 / 可执行文件  lilygo-ui-music
应用 ID           cc.lilygo.ui.Music
应用名称          Music
```

## 工程结构

- `src/pages/music/music_model.*`：歌曲库、队列与播放状态，不依赖 LVGL。
- `src/pages/music/audio_backend.*`：`mpv` 进程和本地 IPC 播放控制。
- `src/pages/music/music_view_model.*`：用户命令、展示数据和 Subject。
- `src/pages/music/music_view.*`：LVGL 视图、事件转发和响应式布局。
- `src/app.cpp`：AppKit 生命周期与页面装配。
- `src/components/`：语义样式和 Subject 生命周期辅助。
- `tests/`：模型、ViewModel 和无头渲染测试。
- `assets/audio/`：开发和设备测试使用的原创演示音频及 Espressif ADF 双声道格式样本；来源与许可见 README。

SDK 仅通过 `find_package(LilyGoUI CONFIG REQUIRED)` 消费，LVGL 与公共字体由 SDK 统一提供。开发时遵循 [工程总览](docs/00-overview.md) 和 [UI 规范](docs/01-user-interface.md)。
