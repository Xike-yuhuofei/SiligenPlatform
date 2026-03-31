# Consumer Access Contract

## 1. 目的

定义模块外消费者在 `ARCH-203` 中允许和禁止的接入面，避免 owner 收口后继续通过宿主 concrete 实现、内部头路径或 helper target 回流到旧边界。

## 2. 消费者规则

| Consumer | Allowed Surface | Forbidden Surface | Migration Target | Verification |
| --- | --- | --- | --- | --- |
| `apps/hmi-app` | `modules/hmi-application` 的 contract / use case / state object | `hmi_client.client.*` 被模块反向 import；宿主直接持有 owner 规则 | 宿主只保留 UI 事件、渲染、消息提示、外部调用装配 | HMI 单测、formal gateway contract guard |
| `apps/planner-cli` | `motion-planning`、`process-path`、`topology-feature` 的 canonical contracts/facade | 直连 `domain/*` 内部路径、shadow adapter re-export | 统一切到 module public surface | CLI 编译、owner include 审查 |
| `apps/runtime-gateway` | `runtime-execution` application/runtime public surface | 旧 workflow use case、内部 implementation header、helper target | builder/facade 只依赖 `M9` 公共面 | gateway 编译、boundary script |
| `apps/runtime-service` | `runtime-execution` host/application contracts 与受控 module contracts | app 层继续做 `M4/M9` live owner、直连 workflow 内部路径 | app 只保留装配和 runtime 宿主职责 | root build/test、runtime host tests |
| `modules/job-ingest` | `process-planning` 的 canonical contracts / facade | 直连 `process-planning/domain/configuration/*` concrete | 通过 M4 正式边界消费 planning/config 事实 | forbidden include 检查、target graph 审查 |
| `modules/dxf-geometry` | `process-path` / `process-planning` 的 canonical contracts / adapters seam | 自持 `M6` path source adapter、副本 adapter、直连 `process-planning/domain/configuration/*` concrete | DXF 输入链只通过声明的 input seam 交接 | duplicate adapter 检查、forbidden include 检查 |
| `modules/workflow` 作为编排消费者 | 下游模块 facade / contract | 下游模块内部 domain/service private path | 只作为编排层消费稳定边界 | forbidden include 检查、process-runtime-core tests |
| `scripts/validation` / `tests` | canonical root entry points 与明示 reports path | 依赖未记录的本地私有路径、隐藏 fallback | 所有门禁都回到 canonical roots | root gate 报告、脚本参数审查 |

## 3. 消费者迁移原则

1. 消费者改接必须先于兼容壳删除。
2. 任何消费者若仍依赖 forbidden surface，对应兼容壳不得宣告退出。
3. 一个消费者一次只切一个 seam，避免跨多个 owner 一次性失控。
4. `job-ingest` 与 `dxf-geometry` 视为一等消费者，不允许因其位于 `modules/` 下就被排除在 cutover 之外。

## 4. 阻断条件

出现以下任一情况时，对应波次必须阻断：

1. 消费者仍通过内部路径访问 canonical owner。
2. 消费者需要新增 helper target 才能工作。
3. 消费者切换后只能靠 sibling workspace 的未跟踪资产维持验证。
4. app 侧 compat re-export、owner 级单测或无关链接依赖仍被当作正式接入面。
