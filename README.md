# WxClash

## Linux 编译运行

安装构建工具和 wxWidgets 开发包（Debian/Ubuntu）：

```bash
sudo apt install build-essential meson ninja-build libwxgtk3.2-dev
```

配置、编译并运行：

```bash
meson setup builddir --buildtype=release
meson compile -C builddir
./builddir/WxClash
```

Meson 通过 `wx-config` 查找 wxWidgets，并链接 `core`、`base` 和 `net` 组件。若系统安装的是其他 wxWidgets GTK 版本，只要该版本提供这些组件即可。

重新配置已有构建目录：

```bash
meson setup builddir --reconfigure
```
