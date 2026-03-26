# apps

`apps/` 只承载进程入口、装配面和发布壳，不承载一级业务 owner。

## 当前进程入口

| 目录 | 角色 |
|---|---|
| `apps/planner-cli/` | 规划侧 CLI 入口 |
| `apps/runtime-service/` | 运行时宿主入口 |
| `apps/runtime-gateway/` | 网关与 TCP 入口 |
| `apps/hmi-app/` | HMI 进程入口 |
| `apps/trace-viewer/` | 追溯与证据查看入口 |

## 约束

- 可复用业务能力进入 `modules/` 或 `shared/`。
- 不再保留 `control-*` 目录作为默认入口。
- 新增进程入口和发布脚本必须优先落在当前目录体系。