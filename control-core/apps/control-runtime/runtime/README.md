## runtime

`apps/control-runtime/runtime` 已降级为 legacy 文档壳。

真实运行时适配器实现位于：

- `D:\Projects\SiligenSuite\packages\runtime-host\src\runtime/events`
- `D:\Projects\SiligenSuite\packages\runtime-host\src\runtime/scheduling`
- `D:\Projects\SiligenSuite\packages\runtime-host\src\runtime/configuration`
- `D:\Projects\SiligenSuite\packages\runtime-host\src\runtime/storage/files`

当前已收口到这里的能力：

- `events/`：进程内事件发布与订阅
- `scheduling/`：后台任务调度与线程池执行

迁移原则：

- 这些能力服务于 runtime 装配，不进入 `process-core` / `motion-core`
- `src/infrastructure/adapters/system/runtime/*` 只保留兼容入口，避免一次性改完全部旧 include
- 当前默认仍直接使用这些 canonical target（`siligen_event_adapters` / `siligen_task_scheduler`）；本目录不再承接源码
