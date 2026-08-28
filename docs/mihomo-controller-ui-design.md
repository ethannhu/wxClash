# Mihomo Core 桌面控制器 UI 设计文档

## 1. 文档信息

| 项目 | 内容 |
| --- | --- |
| 产品名称 | Mihomo Core 桌面控制器（暂定） |
| UI 技术 | C++ / wxWidgets |
| 目标平台 | Windows 桌面，优先兼容 Windows 7 |
| 后端对象 | Mihomo Core External Controller API |
| API 文档 | <https://wiki.metacubex.one/api/> |
| 文档状态 | 初步设计 |

## 2. 产品目标

开发一个基于 wxWidgets 的原生桌面控制器，用于连接、监控和操作 Mihomo Core。产品应当以用户任务为中心组织界面，而不是把 API 接口逐项暴露为按钮。

主要目标：

1. 清晰展示内核连接状态、运行模式、实时流量和活动连接。
2. 快速完成策略组切换、节点选择和延迟测试。
3. 提供连接、规则、订阅和日志诊断能力。
4. 安全地执行配置重载、缓存清理、更新和重启等维护操作。
5. 保证持续数据更新时界面仍然流畅，不阻塞 wxWidgets 主线程。

## 3. 产品范围

### 3.1 Mihomo API 能力

- 查询内核版本和运行配置。
- 获取实时流量、内存、日志和活动连接。
- 查询策略组、代理节点、延迟历史和存活状态。
- 选择节点并执行节点或策略组延迟测试。
- 查询、临时禁用规则，更新规则集合。
- 更新代理集合并执行健康检查。
- 查询 DNS、清理 DNS/FakeIP 缓存。
- 重载配置、更新 GEO 数据、更新或重启内核。

### 3.2 控制器自身能力

以下功能不属于 Mihomo External Controller API，需要由桌面程序单独实现：

- 启动、停止并监控 `mihomo.exe` 进程。
- 管理本地配置文件和配置方案。
- 设置 Windows 系统代理。
- 配置开机启动和最小化到托盘。
- 保存控制地址、API 密钥和本地用户偏好。
- 内核崩溃检测、自动重启和 API 自动重连。

> Mihomo 的 Rule/Global/Direct 运行模式与 Windows 系统代理开关是两个不同概念，界面中必须分开呈现。

## 4. 设计原则

### 4.1 任务导向

一级导航按用户任务划分为“概览、代理、连接、规则、日志、设置”，而不是按 REST 资源名称划分。

### 4.2 高频操作常驻

以下内容应在大多数页面保持可见：

- 内核在线状态。
- 当前运行模式。
- 当前上传、下载速度。
- API 连接状态和最后更新时间。

### 4.3 渐进展示信息

代理主列表只展示节点名称、类型、存活状态和延迟。UDP、TFO、MPTCP、SMUX、provider 等详细属性放入详情区域或右键菜单，避免表格过宽。

### 4.4 危险操作隔离

关闭全部连接、重新加载配置、重启内核和更新内核等操作放入独立维护区域，并要求用户确认。

### 4.5 原生桌面体验

- 支持键盘导航、右键菜单、列排序和多选。
- 使用系统字体和原生控件。
- 正确适配 DPI、窗口缩放和 Windows 7 主题。
- 不依赖嵌入式 Web 页面实现核心界面。

## 5. 总体布局

```text
┌────────────────────────────────────────────────────────────┐
│ Mihomo    内核已连接     ↓ 8.4 MB/s  ↑ 620 KB/s           │
│                         [规则] [全局] [直连]               │
├──────────────┬─────────────────────────────────────────────┤
│ 概览         │                                             │
│ 代理         │                                             │
│ 连接         │                当前业务页面                 │
│ 规则         │                                             │
│ 日志         │                                             │
│ 设置         │                                             │
├──────────────┴─────────────────────────────────────────────┤
│ API 127.0.0.1:9090       内核 v1.x       更新于：刚刚     │
└────────────────────────────────────────────────────────────┘
```

### 5.1 wxWidgets 组件层次

```text
wxApp
└── MainFrame : wxFrame
    ├── TopBarPanel : wxPanel
    │   ├── CoreStatusView
    │   ├── TrafficSummaryView
    │   └── ModeSelector
    ├── NavigationPanel : wxPanel
    ├── MainBook : wxSimplebook
    │   ├── OverviewPage
    │   ├── ProxiesPage
    │   ├── ConnectionsPage
    │   ├── RulesPage
    │   ├── LogsPage
    │   └── SettingsPage
    └── wxStatusBar
```

固定式导航建议使用 `wxSimplebook` 管理页面。第一版不使用复杂的可停靠窗口系统，以降低布局和状态保存成本。

## 6. 页面设计

### 6.1 概览页

#### 页面目标

让用户在打开程序后立即确认内核是否正常、当前流量是否异常，以及是否存在大量活动连接。

#### 页面内容

- 下载速度和累计下载量。
- 上传速度和累计上传量。
- 活动连接数。
- Mihomo 内存占用。
- 最近 60～120 秒流量曲线。
- 当前运行模式和内核版本。
- DNS 查询、DNS 缓存清理和 FakeIP 缓存清理快捷入口。

#### API 映射

| 功能 | API |
| --- | --- |
| 实时流量 | `GET/WS /traffic` |
| 实时内存 | `GET/WS /memory` |
| 活动连接数 | `GET/WS /connections` |
| 内核版本 | `GET /version` |
| 查询运行配置 | `GET /configs` |
| 清理 DNS 缓存 | `POST /cache/dns/flush` |
| 清理 FakeIP 缓存 | `POST /cache/fakeip/flush` |

#### wxWidgets 建议

- 指标区域：`wxPanel + wxStaticText`。
- 曲线图：自定义 `wxPanel::OnPaint()`，使用 `wxAutoBufferedPaintDC` 绘制。
- 更新频率：流量和内存按 API 推送频率更新，绘图保留固定长度环形缓冲区。

### 6.2 代理与策略组页

#### 页面目标

快速完成策略组定位、节点测速和节点选择。这是控制器最核心的操作页面。

#### 布局

```text
┌────────────────┬───────────────────────────────────────────┐
│ 策略组         │ 节点名称       类型        延迟     状态 │
├────────────────┼───────────────────────────────────────────┤
│ 节点选择       │ 日本 01        VMess       86 ms    ●   │
│ 自动选择       │ 香港 02        Trojan     112 ms    ●   │
│ 故障转移       │ 新加坡 01      Hysteria2  168 ms    ●   │
│ 流媒体         │ 美国 03        VLESS       超时      ×   │
└────────────────┴───────────────────────────────────────────┘
```

#### 交互逻辑

1. 左侧选择一个策略组。
2. 右侧根据策略组的 `all` 字段显示成员。
3. 根据 `now` 字段高亮当前节点。
4. 双击节点或点击“选择”切换节点。
5. 支持测试当前节点、当前策略组或全部可见节点。
6. 选中节点后在详情区域显示协议能力、provider、接口、延迟历史等信息。
7. 搜索框按节点名称、类型和 provider 过滤。

#### API 映射

| 功能 | API |
| --- | --- |
| 获取策略组 | `GET /group` |
| 获取代理和策略组 | `GET /proxies` |
| 选择节点 | `PUT /proxies/{proxy-name}` |
| 清除固定选择 | `DELETE /proxies/{proxy-name}` |
| 测试单个节点 | `GET /proxies/{proxy-name}/delay` |
| 测试策略组 | `GET /group/{group-name}/delay` |

#### wxWidgets 建议

- 策略组：`wxDataViewListCtrl` 或 `wxListBox`。
- 节点表格：`wxDataViewCtrl`。
- 搜索：`wxSearchCtrl`。
- 详情：右侧可折叠 `wxPanel` 或双击打开 `wxDialog`。
- 批量测速应限制并发数，防止一次创建过多请求。

### 6.3 活动连接页

#### 页面目标

观察当前连接走向，定位应用程序、规则和代理链路问题，并允许关闭指定连接。

#### 建议列

| 列 | 内容 |
| --- | --- |
| 目标 | 目标域名/IP 和端口 |
| 进程 | 进程名称或路径 |
| 网络 | TCP/UDP 等网络信息 |
| 规则 | 命中的规则类型和内容 |
| 代理链 | `chains`/`providerChains` |
| 上传/下载 | 当前连接累计流量 |
| 开始时间 | 连接建立时间 |

#### 交互

- 按目标、进程、规则或代理链搜索。
- 点击列头排序。
- 双击查看完整 metadata。
- 右键关闭当前连接。
- 支持多选并关闭所选连接。
- “关闭全部连接”必须弹出确认框。

#### API 映射

| 功能 | API |
| --- | --- |
| 实时连接数据 | `GET/WS /connections` |
| 关闭指定连接 | `DELETE /connections/{id}` |
| 关闭所有连接 | `DELETE /connections` |

#### 性能要求

连接数量可能较多，应使用虚拟数据模型：

- 以连接 `id` 作为稳定主键。
- 更新已有记录，不要每秒销毁并重建整个控件。
- 500～1000 ms 批量提交一次界面刷新。
- 排序和过滤在数据模型层完成。

### 6.4 规则与规则集合页

#### 页面内容

- 规则索引、类型、匹配内容和目标策略。
- 规则条目数量。
- 命中次数、未命中次数和最后命中时间。
- 临时启用/禁用规则。
- 规则集合状态和手动更新操作。

#### API 映射

| 功能 | API |
| --- | --- |
| 获取规则 | `GET /rules` |
| 临时禁用规则 | `PATCH /rules/disable` |
| 获取规则集合 | `GET /providers/rules` |
| 更新规则集合 | `PUT /providers/rules/{provider-name}` |

> 规则禁用属于临时运行状态，内核重启后会失效。界面应明确提示，避免用户误认为配置文件已被修改。

### 6.5 代理集合页

可以作为“代理”页面中的二级标签，也可以在功能较多时升级为独立页面。

#### 页面内容

- 集合名称、类型、更新时间和节点数量。
- 更新当前或全部代理集合。
- 对集合执行健康检查。
- 展开查看集合中的代理节点及延迟。

#### API 映射

| 功能 | API |
| --- | --- |
| 获取所有代理集合 | `GET /providers/proxies` |
| 获取指定集合 | `GET /providers/proxies/{provider-name}` |
| 更新指定集合 | `PUT /providers/proxies/{provider-name}` |
| 集合健康检查 | `GET /providers/proxies/{provider-name}/healthcheck` |
| 测试集合内节点 | `GET /providers/proxies/{provider-name}/{proxy-name}/healthcheck` |

### 6.6 实时日志页

#### 页面内容

- 实时日志列表。
- Info、Warning、Error、Debug 级别过滤。
- 暂停自动滚动。
- 清空当前显示。
- 文本搜索和日志导出。
- 结构化日志字段查看。

#### API 映射

- `GET/WS /logs`
- 建议请求 `?format=structured`，利用 `time`、`level`、`message` 和 `fields` 字段显示及过滤。

#### wxWidgets 建议

- 简单实现：多行只读 `wxTextCtrl`。
- 需要颜色和大日志量：`wxStyledTextCtrl`。
- 后台线程接收日志，按时间或条数合并后批量追加。
- 设置显示行数上限，例如保留最近 5,000～20,000 行，防止长期运行占用过多内存。

### 6.7 设置与内核维护页

建议使用 `wxNotebook` 或分组面板划分为以下区域。

#### 连接设置

- Controller API 地址。
- API Secret。
- 连接超时和自动重连。
- “测试连接”按钮。

#### 运行配置

- `mode`。
- `log-level`。
- `allow-lan`。
- `ipv6`。
- HTTP/SOCKS/Mixed Port。
- TUN 相关运行配置。

#### 配置文件

- 当前配置路径。
- 选择配置文件。
- 强制重新加载配置。
- 配置重载失败时显示完整错误信息。

#### 内核维护

- 清理 DNS/FakeIP 缓存。
- 更新 GEO 数据。
- 更新 Mihomo Core。
- 重启内核。
- Debug 模式下执行 GC 或打开 pprof 页面。

#### API 映射

| 功能 | API |
| --- | --- |
| 获取配置 | `GET /configs` |
| 修改运行配置 | `PATCH /configs` |
| 重新加载配置 | `PUT /configs?force=true` |
| 更新 GEO 数据 | `POST /configs/geo` 或 `POST /upgrade/geo` |
| 更新内核 | `POST /upgrade` |
| 重启内核 | `POST /restart` |
| 主动 GC | `PUT /debug/gc` |

### 6.8 DNS 工具

DNS 查询适合作为概览页快捷工具或独立工具对话框。

输入：

- 域名。
- 记录类型，例如 A、AAAA、CNAME、MX。

输出：

- DNS 状态码。
- Answer、Authority、Additional 记录。
- TTL 和返回数据。

对应 API：`GET /dns/query?name={name}&type={type}`。

## 7. 全局交互状态

界面至少应明确区分以下状态：

| 状态 | UI 表现 |
| --- | --- |
| 未配置 | 引导用户填写 API 地址和密钥 |
| 正在连接 | 状态图标和“正在连接”文本，不阻塞界面 |
| 已连接 | 显示内核版本和实时数据 |
| 认证失败 | 明确提示检查 Secret，不无限重试 |
| 连接断开 | 保留最后一次数据并标记为过期，启动退避重连 |
| API 不兼容 | 显示实际版本，并禁用不支持的功能 |
| 请求执行中 | 仅禁用相关按钮，避免锁定整个页面 |
| 操作失败 | 显示接口返回错误，允许复制详细信息 |

## 8. 线程与数据更新模型

wxWidgets 主线程只负责控件创建、事件处理和绘制。HTTP、WebSocket 和文件读取都应在后台执行。

```text
UI 主线程
   │
   ├── REST 命令 ──> MihomoApiClient ──> Mihomo API
   │
   └── wxThreadEvent <── MihomoStreamClient <── WebSocket
                              ├── traffic
                              ├── memory
                              ├── connections
                              └── logs
```

建议组件：

```text
MihomoApiClient
├── GetVersion()
├── Get/Patch/ReloadConfig()
├── GetProxies()/SelectProxy()/TestDelay()
├── GetRules()/DisableRules()
├── UpdateProvider()/HealthCheck()
├── CloseConnection()
└── Restart()/Upgrade()/FlushCache()

MihomoStreamClient
├── TrafficStream
├── MemoryStream
├── ConnectionsStream
└── LogsStream

ApplicationServices
├── SystemProxyManager
├── ConfigProfileManager
└── CredentialStore
```

后台对象通过 `wxThreadEvent` 或 `wxQueueEvent()` 向页面发送不可变的数据快照。禁止后台线程直接操作 wxWidgets 控件。

## 9. API 客户端要求

### 9.1 认证

所有需要认证的请求添加：

```http
Authorization: Bearer <secret>
```

API 密钥不应：

- 在界面中长期明文显示。
- 写入普通日志。
- 拼接到 URL 查询参数中。
- 存入未加保护的配置文件。

Windows 平台优先使用 Credential Manager 或 DPAPI 保存 Secret。

### 9.2 路径处理

调用带配置路径的接口时，如果路径不在 Mihomo 工作目录，需要正确配置 `SAFE_PATHS`。界面应在用户选择外部配置路径时提示这一限制。

### 9.3 请求取消

- 页面关闭或切换 API 地址时取消未完成请求。
- 程序退出时先终止流连接，再销毁页面。
- 延迟测试应支持超时，并区分“节点不可用”和“测试请求失败”。

## 10. 本地进程和托盘设计

### 10.1 内核运行状态

建议区分：

- 内核进程未启动。
- 内核进程已启动但 API 尚未就绪。
- API 已连接。
- 内核由外部程序管理，仅连接远程 API。

### 10.2 托盘菜单

建议提供：

- 显示主窗口。
- 启动/停止内核。
- Rule/Global/Direct 快速切换。
- Windows 系统代理开关。
- 当前策略组和常用节点选择。
- 退出控制器。

关闭主窗口时可以根据用户设置选择“退出”或“最小化到托盘”。

## 11. wxWidgets 控件选型

| 场景 | 推荐控件 |
| --- | --- |
| 主页面切换 | `wxSimplebook` |
| 导航栏 | `wxPanel + wxButton/wxToggleButton` |
| 大型列表 | `wxDataViewCtrl` |
| 简单列表 | `wxDataViewListCtrl` |
| 搜索 | `wxSearchCtrl` |
| 分组设置 | `wxNotebook` 或 `wxPropertyGrid` |
| 普通日志 | 只读多行 `wxTextCtrl` |
| 高级日志 | `wxStyledTextCtrl` |
| 流量图 | 自绘 `wxPanel` |
| 确认操作 | `wxMessageDialog` |
| 文件选择 | `wxFilePickerCtrl` |
| 托盘 | `wxTaskBarIcon` |
| 页面布局 | `wxBoxSizer`、`wxFlexGridSizer` |

## 12. 错误处理与安全要求

1. 所有危险操作显示目标和影响范围。
2. 关闭全部连接、重启和更新操作要求确认。
3. UI 中不得将请求失败等同于内核崩溃。
4. 网络断开后使用指数退避重连，并允许手动立即重连。
5. API 地址、配置路径和版本信息可以复制，Secret 默认不可复制和显示。
6. 日志导出前允许用户选择是否包含进程路径、域名和 IP 等隐私信息。
7. 更新内核期间禁止重复触发更新，但其他只读页面仍可操作。

## 13. 性能目标

| 指标 | 建议目标 |
| --- | --- |
| 主窗口启动 | 1 秒内显示界面，连接过程异步进行 |
| 普通操作反馈 | 点击后 100 ms 内显示执行状态 |
| 流量图刷新 | 每秒一次 |
| 连接表刷新 | 500～1000 ms 批量刷新 |
| 日志追加 | 批量追加，不为每条日志单独触发重绘 |
| 大列表 | 10,000 条记录下仍可搜索、排序和滚动 |
| 断线恢复 | 自动退避重连，并显示最后数据的时间戳 |

## 14. 开发阶段规划

### 第一阶段：MVP

- API 地址和 Secret 配置。
- 内核版本及在线状态。
- Rule/Global/Direct 模式切换。
- 实时流量和内存概览。
- 策略组和代理节点选择。
- 单节点及策略组延迟测试。
- 活动连接查看和关闭。
- 实时日志。
- 基本运行配置修改。

### 第二阶段：管理能力

- 代理集合和规则集合更新。
- 规则搜索、命中统计和临时禁用。
- DNS 查询工具和缓存管理。
- 配置文件方案管理。
- Windows 系统代理和托盘控制。

### 第三阶段：高级能力

- 内核、GEO 数据更新。
- 延迟历史图表和节点排序策略。
- 日志导出和隐私清理。
- Debug/pprof 工具入口。
- 多个本地或远程 Mihomo 实例管理。

## 15. MVP 验收标准

1. 输入正确的 API 地址和 Secret 后能够连接并显示版本。
2. API 断开时界面不冻结，并能自动恢复连接。
3. 能够切换 Rule、Global、Direct 模式并读取最新状态。
4. 能够列出策略组和节点，正确显示当前选择。
5. 能够测试延迟并切换 Selector 节点。
6. 能够持续显示流量、内存和活动连接。
7. 能够关闭单个连接和全部连接。
8. 能够显示并过滤结构化实时日志。
9. 所有 HTTP/WebSocket 操作均不阻塞 UI 主线程。
10. Secret 不出现在普通日志、状态栏和错误复制文本中。

## 16. 参考资料

- Mihomo API：<https://wiki.metacubex.one/api/>
- Mihomo 项目：<https://github.com/MetaCubeX/mihomo>
- wxWidgets 文档：<https://docs.wxwidgets.org/>
