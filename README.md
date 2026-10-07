# WxClash

## Linux 编译运行

安装构建工具和系统开发包（Debian/Ubuntu）：

```bash
sudo apt install build-essential meson ninja-build libwxgtk3.2-dev libyaml-cpp-dev
```

配置、编译并运行：

```bash
meson setup builddir --buildtype=release
meson compile -C builddir
./builddir/WxClash
```

Meson 优先使用系统中的 wxWidgets 和 `yaml-cpp`。如果系统未安装这些依赖，Meson 会根据 `subprojects/*.wrap` 自动下载固定版本，并通过 CMake 子项目构建它们。wxWidgets 链接 `core`、`base` 和 `net` 组件。

重新配置已有构建目录：

```bash
meson setup builddir --reconfigure
```
