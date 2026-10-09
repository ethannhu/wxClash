# WxClash

基于 wxWidgets 的原生桌面客户端，用于连接和管理 [Mihomo](https://github.com/MetaCubeX/mihomo)；支持配置查看、运行状态监控和常用控制操作。

## 特性

- 原生跨平台桌面界面（Windows / Linux）
- CMake + Ninja 统一构建
- Windows 以 Win7 为最低系统版本


## 使用说明

首次启动后，打开 **Settings**：

1. 指定 Mihomo/Clash **core** 可执行文件路径。
2. 指定要使用的配置文件路径。
3. 保存设置后再启动或连接 core。

运行前请检查应用数据目录是否包含 core 运行所需的数据文件，例如：

- `geoip` 数据库
- `country.mmdb`
- 配置中引用的规则集或其他外部资源

如果出现地理位置解析失败、规则匹配异常或 core 启动后立即退出，请先确认 core 路径、配置文件路径和应用数据目录中的必要数据是否完整，并检查配置文件中的路径是否正确。

## 构建要求

- CMake 3.20+
- Ninja
- Git（CMake 配置阶段会下载依赖）
- 支持 C++20 的编译器

Linux（Debian / Ubuntu）：

```bash
sudo apt install build-essential cmake ninja-build git libgtk-3-dev
```

Windows：安装 Visual Studio 2022 Build Tools，并选择 **Desktop development with C++** 工作负载，确保包含 MSVC、Windows SDK、CMake 和 Ninja。然后打开 **x64 Native Tools Command Prompt for VS 2022**。

> 首次配置需要访问 GitHub 下载 wxWidgets、yaml-cpp 和 nlohmann/json；请确保网络可用。

## 构建


```bash
cmake --preset release
cmake --build --preset release
```

生成文件位于 `builddir/`：

- Linux：`builddir/WxClash`
- Windows：`builddir/WxClash.exe`

## 构建配置

- CMake 在配置阶段自动获取固定版本的 wxWidgets、yaml-cpp 和 nlohmann/json。
- wxWidgets 和 yaml-cpp 构建为静态库；nlohmann/json 为头文件库。
- MSVC 使用 `/MD` 动态运行时，因此 Windows 发布环境需要安装兼容版本的 Visual C++ Redistributable。
- Windows 目标架构为 x64，最低支持 Windows 7。

如果需要使用本地 wxWidgets 源码，可覆盖 FetchContent 来源：

```bash
cmake --preset release \
  -DFETCHCONTENT_SOURCE_DIR_WXWIDGETS=/path/to/wxWidgets
```
