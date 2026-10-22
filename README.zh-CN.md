# LILYGO Music

[English](README.md) | **简体中文**

LILYGO Music 是一个面向 LILYGO UI 的本地音乐播放器，使用 C++17、
[LILYGO UI AppKit](https://github.com/LILYGO-UI/cm0-appkit) 和 LVGL 9.5
实现。应用会扫描设备上的音乐目录，通过 mpv 的本地 IPC 接口播放音频，
并根据可用容器尺寸在竖屏和横屏之间自动重排资料库与播放器。

项目同时提供 CM0 ARM64 Linux 构建和基于 SDL2 的桌面模拟器，便于开发与测试。
播放本地文件时不需要网络连接。

## 预览

| 竖屏 · 568 × 1232 | 横屏 · 1232 × 568 |
| --- | --- |
| ![竖屏音乐播放器](assets/previews/music-portrait.png) | ![横屏音乐播放器](assets/previews/music-landscape.png) |

## 功能

- **本地音乐库：**递归扫描音乐目录，根据文件名和文件夹推导并显示歌曲信息，
  文件变化后可以重新扫描。
- **播放控制：**播放和暂停、上一首和下一首、拖动进度、调节音量。
- **播放模式：**随机播放、循环播放，并在浏览收藏歌曲时保留完整播放队列。
- **搜索与筛选：**搜索歌曲名、艺人和专辑，在全部歌曲与收藏歌曲之间切换。
- **响应式界面：**竖屏和横屏复用同一套 LVGL 视图；宽屏并排显示资料库与播放器，
  窄屏通过 Now Playing 和 Library 分段切换。
- **偏好持久化：**将音量、随机、循环和收藏歌曲保存到用户的 XDG 配置目录。
- **内置样本：**提供三首原创演示音乐和 Espressif ADF 格式样本，用于开发和设备测试。

默认扫描位置为 $HOME/Music、/root/music、/mnt/sdcard/music、
/mnt/music 和 /media/music。设置 LILYGO_MUSIC_DIR 后只扫描指定目录。
显式指定的目录为空或不存在时会显示空音乐库；只有在默认目录都没有歌曲时才会使用
内置示例。扫描最多递归 8 层，最多收录 2,048 首歌曲。

支持 MP3、WAV、FLAC、M4A、AAC、OGG、Opus、AIFF、AC3、MP4、TS 和 WMA。
实际解码和音频输出能力由 PATH 中的 mpv 提供。WAV 时长可以在扫描时读取，
其他格式的时长由 mpv 在开始播放后返回。

偏好设置保存在 $XDG_CONFIG_HOME/lilygo-ui-music/preferences；未设置 XDG_CONFIG_HOME 时使用 $HOME/.config/lilygo-ui-music/preferences。

## 开发环境

项目身份、版本、打包元数据和构建输出设置统一来自 lpm.toml。
CMakePresets.json 定义了 host-simulator 和 cm0-cross 两个预设。
所有命令都应从仓库根目录执行。

| 项目 | 值 |
| --- | --- |
| 包名 / 可执行文件 | lilygo-ui-music |
| 应用 ID | cc.lilygo.ui.Music |
| 版本 | 0.1.0 |
| 许可证 | MIT |
| 设备架构 | AArch64 / ARM64 |

### 获取源码

```shell
git clone --recurse-submodules https://github.com/LILYGO-UI/lilygo-ui-music.git
cd lilygo-ui-music
```

已有 checkout 需要初始化锁定版本的 AppKit 源码 SDK：

```shell
git submodule update --init --recursive
```

应用通过 find_package(LilyGoUI CONFIG REQUIRED) 使用
third_party/cm0-appkit 中的 AppKit。桌面模拟器不需要单独安装二进制 AppKit。

### 主机依赖

需要 CMake 3.21 或更新版本、CTest 和 CPack、支持 C++17 的 C/C++ 编译器、
make、pkg-config、patch、SDL2、FreeType、LPM 和 mpv。模拟器使用 SDL2
提供显示，运行播放和相关测试时需要在 PATH 中找到 mpv。

#### Debian 12 / Ubuntu 24.04 或更新版本

```shell
sudo apt update
sudo apt install build-essential cmake git pkg-config patch libsdl2-dev \
      libfreetype6-dev mpv ca-certificates python3 python3-venv python3-pip
```

#### macOS

如尚未安装，请先安装 Xcode Command Line Tools，再通过
[Homebrew](https://brew.sh/) 安装依赖：

```
xcode-select --install
brew install git cmake pkg-config patch sdl2 freetype mpv python
```

请确认 Homebrew 的 bin 目录已经加入 PATH。

#### LPM

CMake 配置时会调用 lpm metadata --format cmake 读取项目元数据。建议在项目内的
Python 环境中安装 LPM：

```shell
python3 -m venv .venv
. .venv/bin/activate
python -m pip install "git+https://github.com/LILYGO-UI/lpm.git"
lpm --version
```

每次打开新终端时执行 . .venv/bin/activate。CMake 会搜索 .venv/bin 和 PATH；
也可以在配置时通过 -DLPM_EXECUTABLE=/absolute/path/to/lpm 指定已有的 LPM。

配置前可检查主机环境：

```shell
cmake --version
python --version
pkg-config --modversion sdl2 freetype2
lpm metadata --format cmake
mpv --version
```

### 设备交叉编译器

只构建桌面模拟器时可以跳过本节。CM0 构建需要在开发机上准备 AArch64 Linux
交叉编译器。

**Debian / Ubuntu：**

```shell
sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
```

**Apple Silicon macOS：**

```shell
brew install aarch64-unknown-linux-gnu
```

BSP 工具链要求以下命令位于 PATH：

```shell
aarch64-linux-gnu-gcc --version
aarch64-linux-gnu-g++ --version
```

SDK 主机和编译器要求请参考
[CM0BspBuilder](https://github.com/LILYGO-UI/CM0BspBuilder)。BSP 提供目标端头文件
和库，但不会安装主机侧交叉编译器。

## 构建与测试

### 桌面模拟器

使用 LPM 配置、构建并启动模拟器：

```shell
lpm start --foreground
```

复用已有构建而不重新编译：

```shell
lpm start --no-build --foreground
```

模拟器构建目录为 build/host-simulator。指定音乐目录启动：

```
LILYGO_MUSIC_DIR="$HOME/Music" lpm start --no-build --foreground
```

应用会在启动时读取该目录。添加或删除文件后，点击 Library 标题栏右侧的刷新按钮。

### 测试

配置并构建主机模拟器，然后运行完整测试套件：

```shell
cmake --preset host-simulator
cmake --build --preset host-simulator --parallel
ctest --preset host-simulator
```

| 测试 | 覆盖内容 |
| --- | --- |
| music-core | 歌曲扫描、队列切换、随机/循环、收藏、偏好设置，以及真实 mpv IPC 和错误路径 |
| music-view-model | 播放命令、筛选、搜索、时间格式化和 LVGL Subject 更新 |
| music-render-portrait | 568 × 1232 竖屏渲染与交互 |
| music-render-landscape | 1232 × 568 横屏渲染与交互 |

渲染测试会在 build/host-simulator 下生成 music-portrait.ppm 和
music-landscape.ppm。修改 UI 后请同时检查两张图片；断言通过不能替代视觉检查。

### CM0 设备构建

设备预设面向 CM0 Raspberry Pi OS AArch64，使用 Release 构建，关闭主机测试，
并将安装前缀设为 /usr。交叉编译并生成 Debian 包：

```shell
cmake --preset cm0-cross
cmake --build --preset cm0-cross --parallel
cpack --config build/cm0-cross/CPackConfig.cmake -B dist
```

首次配置会下载 CM0 BSP 0.1.0，校验固定的 SHA-256，并将 sysroot 缓存到
.cache/cm0-bsp/。后续构建会复用该缓存。首次下载需要访问 GitHub；配置时可用
-DCM0_BSP_CACHE_DIR=/absolute/path/to/cache 更改缓存位置。

生成的 ARM64 .deb 位于 dist/。软件包包含可执行文件、Launcher manifest、图标、
Desktop/AppStream 元数据和共享字体许可说明，不包含 assets/audio/ 或预览图。
目标系统需要提供包括 mpv 和 lilygo-ui-appkit-dev >= 0.1.0 在内的运行依赖；
设备测试时请另外将音乐文件上传到目标用户的 Music 目录。

<details>
<summary>底层 CMake 命令</summary>

    cmake --preset host-simulator
    cmake --build --preset host-simulator --parallel
    ctest --preset host-simulator

    cmake --preset cm0-cross
    cmake --build --preset cm0-cross --parallel
    cpack --config build/cm0-cross/CPackConfig.cmake -B dist

</details>

### 常见问题

| 现象 | 处理方式 |
| --- | --- |
| lpm is required to read project metadata | 激活 .venv，将 LPM 加入 PATH，或设置 LPM_EXECUTABLE。 |
| 找不到 LilyGoUI 或 LVGL | 执行 git submodule update --init --recursive，并从仓库根目录配置。 |
| 缺少 SDL2 或 FreeType | 安装主机开发包；交叉编译时检查目标 sysroot。 |
| 找不到 Homebrew 库 | 使用 cmake --preset host-simulator -DCMAKE_PREFIX_PATH="$(brew --prefix)" 配置。 |
| mpv not found 或播放失败 | 安装 mpv，确认它在 PATH 中，并检查音频输出设备。 |
| 音乐库为空 | 检查 LILYGO_MUSIC_DIR 和默认音乐目录。显式指定的空目录会保持空状态。 |
| 设备打包无法下载 BSP | 检查 GitHub 访问和磁盘空间，然后重新运行 cm0-cross 配置。 |

## 文档

| 文档 | 内容 |
| --- | --- |
| [工程总览](docs/00-overview.md) | 构建约束、项目元数据、工作流和仓库结构 |
| [UI 开发规范](docs/01-user-interface.md) | MVVM 边界、响应式布局、字体、生命周期和 UI 验收 |
| [Music 实现说明](docs/02-music.md) | 界面行为、扫描、偏好设置、mpv IPC 和验证细节 |
| [演示音频说明](assets/audio/README.md) | 内置样本、生成脚本和第三方音频署名 |

## 致谢

本项目使用 [LILYGO UI AppKit](https://github.com/LILYGO-UI/cm0-appkit)、
[LVGL](https://lvgl.io/) 和 [mpv](https://mpv.io/)。界面与播放行为参考了
[T-Display-K230](https://github.com/Xinyuan-LilyGO/T-Display-K230/tree/main/k230_launcher)
中的音乐应用，但本项目使用独立的 C++ MVVM 实现，不复制该 Launcher 的源码或音频管道。

三首 Little Moments WAV 是本项目原创演示素材。长时双声道样本来自
[Espressif ADF Audio Samples](https://docs.espressif.com/projects/esp-adf/en/latest/audio-samples.html#two-channel-audio)，
并保留 [assets/audio/README.md](assets/audio/README.md) 中记录的署名信息。
