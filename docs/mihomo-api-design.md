# Mihomo REST API 控制设计

## 1. 范围

本程序只通过 Mihomo External Controller API 控制内核，不管理 mihomo 进程。Mihomo 由用户或系统服务单独启动。

官方 API：<https://wiki.metacubex.one/api/>

## 2. 组件

```text
MainFrame
   └── MihomoApiClient
          └── wxSocketClient（HTTP）
```

`MihomoApiClient` 负责地址、认证、超时和 HTTP 响应解析；业务代码只传入 API 路径和 JSON 数据。

## 3. 配置与认证

```cpp
struct ApiConfig {
    std::string baseUrl = "http://127.0.0.1:9090";
    std::string secret;
    int timeoutMs = 3000;
};
```

每个请求添加：

```http
Authorization: Bearer <secret>
Content-Type: application/json
Connection: close
```

Controller 默认只连接本机地址；Secret 不写入日志和错误信息。

Debug 构建中，每次请求完成后将完整 HTTP 原始响应输出到 wxWidgets 调试日志，并同步追加到 Logs 页面；请求中的 Secret 不输出。生产版本可通过日志级别关闭调试输出。

Debug 构建中，每次请求完成后使用 `wxLogDebug` 原样输出完整 HTTP 响应，包含状态行、响应头和响应体；请求中的 Secret 不输出。生产版本可通过日志级别关闭该输出。

## 4. 最小接口

| 功能 | 方法 | 路径 |
|---|---|---|
| 版本 | GET | `/version` |
| 运行配置 | GET | `/configs` |
| 修改配置 | PATCH | `/configs` |
| 重载配置 | PUT | `/configs?force=true` |
| 重启内核 | POST | `/restart` |
| 代理列表 | GET | `/proxies` |
| 流量 | GET | `/traffic` |
| 内存 | GET | `/memory` |
| 连接 | GET | `/connections` |
| 关闭连接 | DELETE | `/connections/{id}` |
| DNS 缓存 | POST | `/cache/dns/flush` |

策略组切换使用 `PUT /proxies/{name}`，请求体为 `{"name":"节点名"}`。实时数据初版使用 REST 轮询，后续需要更实时再接入官方支持的 WebSocket。

## 5. 错误处理

- HTTP `2xx` 视为成功。
- `401` 表示 Secret 错误。
- `404` 表示路径或资源不存在。
- `5xx` 表示 Mihomo 错误。
- 连接失败和超时统一显示为未连接。

请求执行必须放在后台线程，完成后通过 wxWidgets 事件通知 UI，不能阻塞主线程。
