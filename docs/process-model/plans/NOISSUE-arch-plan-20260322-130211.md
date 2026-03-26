# NOISSUE 架构方案 - 整仓目录重构前置准备（适配版候选骨架）

- 状态：Prepared
- 日期：2026-03-22
- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 关联范围文档：NOISSUE-scope-20260322-130211.md
- 范围结论：以 NOISSUE-scope-20260322-130211.md 为冻结基线

## 1. 当前代码状态复核

1. 当前 canonical 根骨架已经冻结为：
   - `apps/`
   - `packages/`
   - `integration/`
   - `tools/`
   - `docs/`
   - `config/`
   - `data/`
   - `examples/`
2. 当前 build/test/CI 已围绕这些路径固化：
   - `build.ps1`、`test.ps1`、`ci.ps1`
   - `.github/workflows/workspace-validation.yml`
   - suite：`apps`、`packages`、`integration`、`protocol-compatibility`、`simulation`
3. 当前运行资产入口已固化：
   - 机器配置：`config/machine/machine_config.ini`
   - 配方与 schema：`data/recipes/`、`data/schemas/recipes/`
   - DXF 离线脚本：`packages/engineering-data/scripts/dxf_to_pb.py`
   - 报告目录：`integration/reports/*`
4. 当前 `control-core` 仍是整仓目录重构的主要阻塞源：
   - 顶层 CMake/source-root owner
   - `src/shared`
   - `src/infrastructure`
   - `modules/device-hal`
   - `third_party`
5. 当前工作树存在未跟踪的候选规格文档目录：
   - `docs/architecture/点胶机端到端流程规格说明/`
   - 在未纳入版本控制前，该目录只作为参考输入，不作为正式门禁基线

## 2. 目标骨架（适配版候选）

本次冻结的目标骨架不是字面照搬候选目录，而是保留现有运行与治理目录，仅让业务代码 owner 向候选结构收敛：

```text
repo/
├─ apps/                    # 保留现有 app 入口与外部命名
├─ modules/                 # 新的业务链 owner 区
│  ├─ workflow/
│  ├─ job-ingest/
│  ├─ dxf-geometry/
│  ├─ topology-feature/
│  ├─ process-planning/
│  ├─ coordinate-alignment/
│  ├─ process-path/
│  ├─ motion-planning/
│  ├─ dispense-packaging/
│  ├─ runtime-execution/
│  ├─ trace-diagnostics/
│  └─ hmi-application/
├─ shared/                  # 长期目标：contracts/kernel/testing/logging 等稳定公共层
├─ packages/                # 过渡期保留；phase 0-2 可继续存在，不能新增长期 owner
├─ config/                  # 保留现有 canonical 运行配置
├─ data/                    # 保留现有 canonical 版本化资产
├─ examples/                # 保留现有示例与回归输入
├─ integration/             # 保留现有集成验证与报告目录
├─ tools/                   # 保留现有构建/脚本/codegen 治理入口
├─ docs/
├─ tests/                   # 过渡期保留，继续承接现有根级 CMake 聚合测试
├─ .github/
└─ third_party/             # 在 control-core source-root 迁出前，不做结构调整
```

### 2.1 已冻结的结构决策

1. `apps/*` 不改对外命名。
2. `config/`、`data/`、`examples/`、`integration/`、`tools/`、`docs/` 保持根级 canonical 地位。
3. `modules/` 是后续业务 owner 的目标承载区。
4. `shared/` 是 contracts/kernel/testing/logging 等稳定公共层的长期目标承载区。
5. `packages/` 在 phase 0-2 作为过渡承载区保留，不再新增长期 canonical owner。
6. `tests/` 在顶层 CMake/source-root 迁出前保留为过渡聚合测试目录。

## 3. 当前目录到目标骨架的映射

### 3.1 Apps 层（外部入口保持不变）

| 当前路径 | 目标归宿 | 波次 | 决策 |
|---|---|---:|---|
| `apps/control-runtime` | 保留 `apps/control-runtime` | 3 | 继续做 runtime 外部入口壳，不改对外命名 |
| `apps/control-tcp-server` | 保留 `apps/control-tcp-server` | 3 | 继续做 TCP server 外部入口壳，不改对外命名 |
| `apps/control-cli` | 保留 `apps/control-cli` | 3 | 继续做 CLI 外部入口壳，不改对外命名 |
| `apps/hmi-app` | 保留 `apps/hmi-app` + 引入 `modules/hmi-application` owner | 2B-3 | HMI 壳层留在 app，展示型应用逻辑迁向业务模块 |

### 3.2 业务 owner（迁向 `modules/`）

| 当前路径 | 目标归宿 | 波次 | 决策 |
|---|---|---:|---|
| `packages/engineering-data` | `modules/job-ingest`、`modules/dxf-geometry`、`modules/topology-feature`、`modules/process-planning`、`modules/coordinate-alignment`、`modules/process-path` | 2A | 按离线工程链切分，不再把“工程数据处理”作为单一长期大包 |
| `packages/process-runtime-core` | `modules/workflow`、`modules/process-planning`、`modules/process-path`、`modules/motion-planning`、`modules/dispense-packaging`、`modules/runtime-execution` | 2B | 按业务链 owner 拆分，不再维持单一“强收拢业务内核”长期形态 |
| `packages/runtime-host` | `modules/runtime-execution/runtime/` | 2B | 宿主生命周期、安全、后台任务归到 runtime-execution 的 runtime 子层 |
| `packages/device-adapters` | `modules/runtime-execution/adapters/` | 2B | 设备适配最终并入 runtime-execution 的 adapters 子层 |
| `packages/traceability-observability` | `modules/trace-diagnostics` + `shared/logging` | 2B-3 | 追溯事实进入业务模块，稳定日志能力进入 shared |

### 3.3 公共层（迁向 `shared/`）

| 当前路径 | 目标归宿 | 波次 | 决策 |
|---|---|---:|---|
| `packages/application-contracts` | `shared/contracts/application/` | 1 | 应用契约长期不再挂在 `packages/` |
| `packages/device-contracts` | `shared/contracts/device/` | 1 | 设备契约长期不再挂在 `packages/` |
| `packages/engineering-contracts` | `shared/contracts/engineering/` | 1 | 工程契约长期不再挂在 `packages/` |
| `packages/shared-kernel` | `shared/kernel/` | 1 | 公共基础内核长期进入 shared |
| `packages/test-kit` | `shared/testing/` | 1 | 工作区级测试支撑长期进入 shared |

### 3.4 过渡技术 owner（phase 0-2 保留，phase 3 再切）

| 当前路径 | 中间策略 | 最终归宿 | 决策 |
|---|---|---|---|
| `packages/transport-gateway` | phase 0-2 继续保留为过渡技术 owner | `apps/control-tcp-server/internal/` + `shared/contracts/application/` | 不把网关实现强行塞入业务模块，先保持稳定，再在 app boundary 内收口 |
| `packages/simulation-engine` | phase 0-2 继续保留 | 后续单独决策；本轮不要求并入 `modules/` | 仿真是跨链路支撑能力，不在本轮目录重构中强行归并 |

### 3.5 不参与目录翻转的根级资产

| 当前路径 | 处理策略 | 原因 |
|---|---|---|
| `config/` | 保留 | 已是默认机器配置 canonical 路径 |
| `data/` | 保留 | 已是默认配方/schema/baseline canonical 路径 |
| `examples/` | 保留 | 已承接样例与演示输入 |
| `integration/` | 保留 | 已承接场景验证与报告目录 |
| `tools/` | 保留 | 已承接 build/test/install/codegen 脚本 |
| `.github/` | 保留 | CI 工作流已深度绑定当前 suite 和报告目录 |
| `third_party/` | 保留 | 顶层 source-root 与 vendor owner 尚未迁出 |

## 4. 数据流 / 调用流（准备阶段）

本阶段不迁目录，只冻结后续目录重构必须依赖的事实与门禁：

```text
候选规格文档
  -> 目录目标骨架冻结
  -> 当前 canonical 路径盘点
  -> 当前 owner / hardcoded path / build-test-ci gate 盘点
  -> 当前 -> 目标 映射表冻结
  -> 阻塞项与回滚单元冻结
  -> 基线验证命令冻结
  -> 后续物理迁移波次输入
```

## 5. 失败模式与补救策略

1. 失败模式：在准备阶段就把 `packages/*` 与 `modules/*` 视为双 canonical owner
   - 补救：phase 0-2 明确 `packages/*` 只做过渡 owner；新增长期 owner 一律登记到 `modules/*` 或 `shared/*`
2. 失败模式：把 `config/`、`data/`、`examples/` 回卷进候选 `shared/` / `samples/` 语义
   - 补救：在 scope 和 test-plan 中把这些目录标记为“不可翻转根级资产”
3. 失败模式：未完成 `control-core` 残余依赖迁出就推进目录切换
   - 补救：把 `src/shared`、`src/infrastructure`、`modules/device-hal`、`third_party`、source-root owner 明确列为 wave blocker
4. 失败模式：目录迁移方案只改文档，不覆盖 build/test/CI 和运行时路径
   - 补救：每个波次的进入条件必须包含根级命令、dry-run、`legacy-exit` 与报告目录验证
5. 失败模式：候选规格目录未受版本控制却被当作正式基线
   - 补救：在工件中显式记录该目录当前仅是参考输入，不进入门禁白名单

## 6. 发布与回滚策略

### 6.1 迁移波次

1. Wave 0：冻结工件
   - 输出 scope / arch-plan / test-plan
   - 冻结目录映射、阻塞项、基线命令
2. Wave 1：公共层 owner 固化
   - contracts / kernel / testing 的目标归宿与命名一次定死
3. Wave 2A：离线工程链切分
   - 从 `packages/engineering-data` 切到 `modules/job-ingest` ~ `modules/process-path`
4. Wave 2B：运行时链切分
   - 从 `packages/process-runtime-core`、`packages/runtime-host`、`packages/device-adapters`、`packages/traceability-observability` 切到 `modules/*`
5. Wave 3：app boundary 与 gateway 切换
   - `apps/*` shell 与 boundary internal 收口
6. Wave 4：文档与门禁收口
   - 回写架构、运行、排障、onboarding、release 文档

### 6.2 回滚单元

1. 目录波次回滚单位：
   - owner 映射
   - build/test gate
   - 入口脚本
   - 文档口径
2. 不允许只回滚目录，不回滚 build/test/CI 与文档。
3. 任何波次若导致：
   - `legacy-exit` 回归
   - root build/test/CI 失效
   - app dry-run 不再指向 canonical
   - `config/`、`data/`、`examples/` 默认路径失效
   则整波次回滚。

## 7. 实施前必须补齐的准备清单

1. 新建本任务专用 scope 文档，不能复用其他 `NOISSUE` scope。
2. 补齐当前路径依赖清单：
   - CMake
   - PowerShell
   - Python
   - GitHub Actions
   - 运行/排障文档
3. 生成目录映射表并冻结默认决策。
4. 冻结公共层归宿策略：
   - contracts
   - kernel
   - testing
   - logging
5. 冻结过渡期保留目录：
   - `packages/`
   - `tests/`
   - `third_party/`
6. 冻结每个波次的进入条件与回滚条件。
