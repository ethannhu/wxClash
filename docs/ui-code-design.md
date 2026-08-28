# WxClash UI 代码设计与第一版实现

## 1. 目标

本方案将 [Mihomo Core 桌面控制器 UI 设计](mihomo-controller-ui-design.md) 落地为一个可编译的 wxWidgets UI 壳层。第一版只解决窗口结构、页面导航、主要控件选型和后续数据接入边界，不实现 API 客户端、WebSocket 或托盘功能。

设计依据：

- `3rdparty/wxWidgets/docs/doxygen/overviews/sizer.h`：使用 Sizer 进行自适应布局。
- `3rdparty/wxWidgets/docs/doxygen/overviews/bookctrl.h`：使用 `wxSimplebook` 进行程序控制的页面切换。
- `3rdparty/wxWidgets/docs/doxygen/mainpages/cat_classes.h`：大型表格使用 `wxDataViewCtrl` 系列控件。
- `3rdparty/wxWidgets/docs/doxygen/overviews/high_dpi.md`：避免固定坐标和物理像素布局。

## 2. 第一版范围

已实现：

- `MainFrame` 主窗口。
- 顶部 Mihomo 状态、流量和运行模式区域。
- 左侧固定导航。
- `概览`、`代理`、`连接`、`规则`、`日志`、`设置` 六个页面。
- 概览指标卡片和流量曲线占位区域。
- 代理组、节点、连接和规则的表格占位数据。
- 设置页面的连接设置和运行配置布局。
- 底部状态栏。

暂不实现：

- HTTP/WebSocket API 客户端。
- 后台线程和 `wxThreadEvent` 数据分发。
- Windows 系统代理和托盘。
- 配置持久化和 Secret 安全存储。

## 3. 布局结构

```text
MainFrame
└── verticalSizer
    ├── TopBarPanel
    ├── contentSizer
    │   ├── NavigationPanel
    │   └── wxSimplebook
    │       ├── OverviewPage
    │       ├── ProxiesPage
    │       ├── ConnectionsPage
    │       ├── RulesPage
    │       ├── LogsPage
    │       └── SettingsPage
    └── wxStatusBar
```

页面切换只由 `MainFrame` 负责。导航栏只发送页面索引，不直接依赖具体页面类型。

## 4. 源码职责

| 文件 | 职责 |
| --- | --- |
| `src/main.cpp` | 创建 `wxApp` 和 `MainFrame` |
| `src/ui/main_frame.h` | 主窗口声明、页面索引和控件成员 |
| `src/ui/main_frame.cpp` | 主窗口布局、页面创建、导航和状态栏行为 |
| `docs/ui-code-design.md` | UI 实现约束和后续接入边界 |

第一版不为每个页面创建单独类。等页面开始接入真实数据后，再按页面复杂度拆分为 `OverviewPage`、`ProxiesPage` 等类，避免当前阶段产生大量空壳文件。

## 5. 后续数据接入

后端接入时，建议新增：

```text
src/api/mihomo_api_client.h/.cpp
src/api/mihomo_stream_client.h/.cpp
```

UI 页面只接收不可变数据快照。HTTP、WebSocket 和文件操作不得在 UI 线程执行；后台完成后使用 `wxThreadEvent` 或 `wxQueueEvent()` 通知主窗口，再由主线程更新控件。

第一版的静态表格数据应替换为数据模型，但不改变页面布局。连接列表需要以连接 `id` 为稳定主键，并按 500～1000 ms 批量刷新。

## 6. 构建

```powershell
cmake --build out/build/x64-debug --config Debug
```

当前实现继续使用 wxWidgets `core` 和 `base` target，不启用 AUI、STC 或其他可选模块。
