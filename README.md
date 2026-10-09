# WxClash

## CMake 编译

项目使用 CMake 管理构建和依赖。CMake 会在配置阶段通过 `FetchContent` 下载固定版本的 wxWidgets、yaml-cpp 和 nlohmann/json；wxWidgets 的 Git 子模块也会自动递归下载。

需要安装：

- CMake 3.20 或更高版本
- Git
- C++ 编译器
- Ninja

Linux（Debian/Ubuntu）：

```bash
sudo apt install build-essential cmake ninja-build git libgtk-3-dev
```

配置和编译：

```bash
cmake --preset release
cmake --build --preset release
./builddir/WxClash
```

Windows + VS2022 Build Tools：

在 `x64 Native Tools Command Prompt for VS 2022` 中执行。VS2022 的 C++ Build Tools 环境只提供编译器和链接器，构建后端仍统一使用 Ninja：

```bat
cmake --preset release
cmake --build --preset release
```

所有平台都使用 Ninja；不要使用 Unix Makefiles 或 Visual Studio generator。

Windows 配置为 x64、Win7 最低版本和 `/MD` 动态 MSVC Runtime。wxWidgets、yaml-cpp 仍然构建为静态库；发布时需要提供兼容的 Visual C++ Redistributable。

如果依赖下载失败，可以预先设置 CMake 的 FetchContent 源码目录，例如：

```bash
cmake -S . -B builddir -G Ninja \
  -DFETCHCONTENT_SOURCE_DIR_WXWIDGETS=/path/to/wxWidgets
```
