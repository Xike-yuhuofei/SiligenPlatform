# Legacy Freeze Policy

更新时间：`2026-03-18`

## 1. 目的

本策略用于冻结 legacy 路径的使用方式，确保团队切到 canonical 工作流后，不再把历史路径重新扩成默认入口。

冻结不是删除。

冻结的含义是：

- 允许当前必要的兼容壳继续存在。
- 允许当前唯一可运行路径继续被例外使用。
- 不允许新增以 legacy 路径为默认入口的开发、测试、审查、自动化和文档约定。

## 2. 冻结范围

以下路径全部纳入冻结范围：

- `hmi-client`
- `dxf-pipeline`
- `control-core/apps/control-runtime`
- `control-core/apps/control-tcp-server`
- `control-core/build/bin/*`
- `control-core/src/domain`
- `control-core/src/application`
- `control-core/modules/process-core`
- `control-core/modules/motion-core`
- `control-core/modules/device-hal`
- `control-core/modules/shared-kernel`
- `control-core/config/*`
- `control-core/data/recipes/*`

## 3. 冻结后的默认规则

### 3.1 默认禁止

从本策略生效起，以下行为默认禁止：

1. 新 README、Runbook、ADR、脚本说明把 legacy 路径写成第一入口。
2. 新脚本、CI、自动化直接从 legacy 目录起跑，而不经过根级入口。
3. 新测试默认从 `hmi-client/src`、`dxf-pipeline/src` 读取源码。
4. 新 CMake / include / link 新增对 `control-core/...` 的硬编码引用，而不声明其为阻塞项或兼容项。
5. 新功能默认落在 `hmi-client`、`dxf-pipeline`、`control-core/src/*`、`control-core/modules/*`。

### 3.2 允许的改动类型

对 frozen legacy 路径，只允许以下四类改动：

1. 兼容壳转发修复。
2. 当前唯一运行链路的阻塞修复。
3. 发布、联调、HIL、现场支持所需的资产同步或路径同步。
4. 为下线 legacy 路径做核对、标注、收口准备。

除此之外，默认不应继续在 legacy 路径扩新逻辑。

## 4. 兼容壳的使用许可

兼容壳允许存在，但只能按下面的方式使用：

| 路径 | 还能不能用 | 什么时候能用 | 谁可以用 | 默认状态 |
|---|---|---|---|---|
| `hmi-client/scripts/*` | 能用 | 接住历史命令或验证转发是否正常 | 应用维护者、测试负责人 | 兼容壳，不是默认入口 |
| `dxf-pipeline` | 能用 | HMI DXF 预览仍依赖、旧 CLI/import 兼容仍依赖时 | 工程数据维护者、HMI 维护者 | 兼容壳，不是默认入口 |
| `control-core/apps/control-runtime` | 能用 | 解释阻塞、维护 alias、修复 runtime 迁移阻塞时 | 运行时维护者 | 兼容壳，不是默认入口 |
| `control-core/apps/control-tcp-server` | 能用 | 维护 legacy `main.cpp` 或构建接线时 | 运行时维护者 | 兼容壳，不是默认入口 |
| `control-core/src/adapters/tcp` / `modules/control-gateway` | 能用 | 维护 legacy target alias 或兼容测试时 | 运行时维护者 | 兼容壳，不是默认入口 |

## 5. 唯一可运行路径的例外

以下对象虽然属于 legacy，但当前仍是唯一可运行路径或唯一真实产物来源，因此保留例外资格：

| legacy 对象 | 例外规则 |
|---|---|
| `control-core/build/bin/**/siligen_tcp_server.exe` | 可以被 `apps/control-tcp-server/run.ps1` 间接调用；不能直接写回团队默认命令。 |
| `control-core/build/bin/**/siligen_cli.exe` | 可以被 `apps/control-cli/run.ps1` 间接调用；不能直接写回团队默认命令。 |
| `control-core/src/domain` / `src/application` / `modules/process-core` / `modules/motion-core` | 当前仍是 process/runtime 真实实现面；允许维护，但每次改动都必须说明 canonical 替代尚未完成。 |
| `control-core/modules/device-hal` / `modules/shared-kernel` | 当前仍是 device/shared-kernel 真实实现面；允许维护，但不得新增新的默认依赖面。 |
| `control-core/config/machine_config.ini` | 当前仍可能被 fallback 读取；允许发布和现场同步，但默认编辑入口仍是 `config/machine/machine_config.ini`。 |
| `control-core/data/recipes/*` | 当前仍可能是旧链路真实资产来源；允许同步和核对，但默认公开路径仍是 `data/recipes/*`。 |
| `hmi-client/src` | 当前只允许做 mirror 差异核对和兼容下线准备；不作为新功能默认落点。 |

## 6. 评审与准入规则

每个涉及 frozen legacy 路径的提交，都必须满足以下准入条件：

1. 提交说明写明该路径属于 `compatibility shell` 还是“唯一可运行路径例外”。
2. 提交说明写明当前调用方是谁。
3. 提交说明写明为什么不能改到 canonical 路径。
4. 若是兼容壳扩边界，必须同步更新 `docs/architecture/compatibility-shell-audit.md`。
5. 若新增 legacy 默认引用，评审应直接拒绝，除非文档中新增了明确例外规则。

## 7. 退出冻结的条件

某个 frozen legacy 路径只有在满足全部条件后，才能从默认例外名单中移除：

1. canonical 路径已经承接真实实现或真实产物。
2. 根级入口已经不再回退到该 legacy 路径。
3. 测试、文档、自动化都已改到 canonical 路径。
4. `docs/architecture/canonical-paths.md` 与 `docs/architecture/compatibility-shell-audit.md` 已同步更新。

在满足这些条件前，禁止直接删除兼容壳。

## 8. 团队执行口径

团队统一口径如下：

1. legacy 目录不是默认入口。
2. compatibility shell 不是默认入口。
3. 只有 canonical 路径和根级脚本才是团队默认工作方式。
4. 例外可以存在，但必须被点名、被限制、被审查。
