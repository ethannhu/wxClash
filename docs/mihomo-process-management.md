# mihomo 内核进程管理设计

## 1. 目标

由主程序统一管理内置 `mihomo.exe`，提供启动、停止、重启、状态查询和异常恢复能力。进程管理模块只负责内核进程生命周期，不直接承载代理配置编辑和业务 UI 逻辑。

## 2. 目录与运行参数

建议运行目录如下：

```text
runtime/
  mihomo.exe
  config.yaml
  logs/
    mihomo-2026-08-28.log
```

启动参数：

```text
mihomo.exe -d <runtime目录> -f <config.yaml>
```

路径必须使用绝对路径，并通过进程启动 API 的参数结构传递，避免拼接未经校验的命令行字符串。

## 3. 模块划分

```text
KernelManager       对外接口、状态机、操作串行化
  ├─ ProcessRunner   创建和结束 mihomo.exe，管理句柄
  ├─ LogRedirector   重定向 stdout/stderr 到日志文件
  ├─ HealthMonitor   检查进程、Controller API 和端口
  └─ RestartPolicy   崩溃检测及自动重启策略
```

UI 只调用 `KernelManager`，不得直接操作系统进程或保存进程句柄。

## 4. 对外接口设计

```cpp
enum class KernelState {
    Stopped,
    Starting,
    Running,
    Stopping,
    Restarting,
    Crashed,
    Error
};

struct KernelStatus {
    KernelState state;
    uint32_t pid = 0;
    int exitCode = -1;
    std::chrono::milliseconds uptime{0};
    bool controllerReady = false;
    std::string lastError;
};

class KernelManager {
public:
    bool Start();
    bool Stop(std::chrono::milliseconds timeout = 3000ms);
    bool Restart();
    KernelStatus Status() const;

    void SetStatusCallback(std::function<void(const KernelStatus&)> callback);
};
```

所有操作应在内部串行执行。重复调用 `Start`、`Stop` 或 `Restart` 时返回当前操作结果，不得创建多个 mihomo 进程。

## 5. 状态机

```text
Stopped ──Start──> Starting ──健康检查成功──> Running
   ▲                  │                         │
   │                  └─失败──> Error           │ Stop
   │                                            ▼
   └────────────── Stopping <───────────────────┘

Running ──进程异常退出──> Crashed ──自动重启──> Restarting
Restarting ──成功──> Running
Restarting ──失败──> Error
```

状态变更必须通过统一的 `PublishStatus()` 通知 UI。状态回调不得在持有内部互斥锁时执行，避免死锁。

## 6. 启动流程

1. 检查 `mihomo.exe` 是否存在且为普通文件。
2. 检查配置文件存在，并执行配置校验。
3. 检查 Controller、HTTP、SOCKS 端口是否可用。
4. 打开日志文件，继承其 stdout/stderr 句柄。
5. 创建子进程并保存进程句柄、PID、启动时间。
6. 进入 `Starting`，轮询 Controller API，直到服务可用。
7. 成功后进入 `Running`；超时则执行停止流程并进入 `Error`。

默认启动超时建议为 10 秒。启动失败必须保留退出码和最后 200 行日志摘要。

Windows 实现建议使用 `CreateProcessW`，并将 mihomo 进程加入 Job Object。主程序退出时关闭 Job Object，确保不会遗留孤儿内核进程。

## 7. 停止与重启流程

### 停止

1. 状态切换为 `Stopping`。
2. 优先通过 Controller API 请求 mihomo 优雅退出。
3. 等待进程结束，最多等待 3 秒。
4. 超时后调用 `TerminateProcess`，记录强制结束原因。
5. 回收进程句柄，清空 PID 和运行时间，进入 `Stopped`。

### 重启

```text
Restarting
  → Stop
  → 等待端口释放
  → 校验配置
  → Start
  → Running 或 Error
```

重启期间禁止并发执行第二次重启。若停止失败，不应直接创建新进程，避免端口和句柄冲突。

## 8. 监控设计

`HealthMonitor` 使用定时器每 2 秒执行一次检查：

- 进程句柄是否仍有效；
- 进程是否已退出及退出码；
- Controller API 是否能在超时内响应；
- 关键监听端口是否存在。

连续 3 次检查失败后标记为异常。进程退出时立即进入 `Crashed`，不等待下一次轮询。

监控线程只负责报告和触发操作，不直接修改 UI。所有 UI 更新通过主线程事件或 wxWidgets 的事件队列完成。

## 9. 自动重启策略

默认策略：

- 仅对非用户主动停止的退出执行自动重启；
- 5 分钟内最多重启 3 次；
- 每次重启间隔至少 1 秒；
- 超过次数后进入 `Error`，等待用户手动操作；
- 配置错误、可执行文件缺失时不自动重启。

用户主动执行 `Stop` 时设置 `manualStop = true`，进程退出后不得被监控线程重新拉起。

## 10. 线程安全与资源回收

- `KernelManager` 使用 mutex 保护状态、PID 和进程句柄。
- 启动、停止、重启使用单独的串行任务队列。
- 进程句柄、Job Object、日志句柄使用 RAII 封装。
- 关闭主程序时先停止监控，再停止 mihomo，最后回收资源。
- 所有外部 API 调用设置连接和响应超时，禁止无限等待。

## 11. 错误码建议

```cpp
enum class KernelError {
    None,
    ExecutableNotFound,
    ConfigNotFound,
    ConfigInvalid,
    PortUnavailable,
    ProcessCreateFailed,
    StartupTimeout,
    ControllerUnavailable,
    StopTimeout,
    UnexpectedExit
};
```

错误信息应同时写入应用日志和 `KernelStatus.lastError`，UI 显示可读文本，日志保留底层错误码。

## 12. 测试要求

- 正常启动、停止、重启；
- 可执行文件或配置文件缺失；
- 配置格式错误；
- 端口被占用；
- mihomo 启动后立即崩溃；
- Controller API 无响应；
- 连续崩溃达到自动重启上限；
- 主程序退出后不存在残留 mihomo 进程；
- 重复点击启动、停止、重启不会产生竞态或多实例。

## 13. 实现顺序

1. `ProcessRunner`：进程创建、句柄管理、日志重定向。
2. `KernelManager`：状态机及串行操作。
3. `HealthMonitor`：进程和 Controller 健康检查。
4. 自动重启和错误码。
5. UI 状态展示及操作按钮。
6. 集成测试和异常场景测试。
