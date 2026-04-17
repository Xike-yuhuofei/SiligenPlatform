# Doc Sync Review

- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 时间戳：20260322-194649
- 范围：Wave 3A TCP/Gateway boundary first closeout
- 上游收口：`docs/process-model/plans/NOISSUE-wave2b-closeout-20260322-191927.md`

## 1. 本次同步的文件

1. `apps/control-tcp-server/README.md`
   - 把 app shell 角色写清为公开 builder/host 的薄入口，避免 README 继续暗示 legacy gateway 实现可被 app 直接吸入。
2. `apps/hmi-app/tests/unit/test_gateway_launch.py`
   - 补充 launch contract 优先级与坏 spec 失败场景，给 `Wave 3A` closeout 提供可复核的单测证据。
3. `docs/architecture/build-and-test.md`
   - 将 `legacy-exit` 口径从“gateway/tcp alias 注册点”修正为“gateway/tcp alias 防回流门禁”。
4. `docs/architecture/canonical-paths.md`
   - 明确 `transport-gateway` 只暴露 canonical target，不再保留 gateway/tcp legacy alias。
5. `docs/architecture/directory-responsibilities.md`
   - 把 `packages/transport-gateway` 从“活跃兼容壳”修正为“仅保留历史审计语义”。
6. `docs/architecture/governance/migration/legacy-exit-checks.md`
   - 将 `test_transport_gateway_compatibility.py` 的存在理由修正为 legacy anti-regression，而不是继续支撑兼容壳。
7. `packages/process-runtime-core/README.md`
   - 删除“legacy gateway/tcp alias 也由 package 侧导出”的过时描述，避免 README 与 CMake 终态冲突。
8. `packages/transport-gateway/CMakeLists.txt`
   - 删除 3 个 legacy alias target，落实 canonical-only 构建接口。
9. `packages/transport-gateway/README.md`
   - 把 README 与实际导出 target 对齐，明确 legacy alias 已删除且不得回流。
10. `packages/transport-gateway/tests/README.md`
    - 更新测试命令与覆盖点描述，改为验证 canonical target 与 legacy anti-regression。
11. `packages/transport-gateway/tests/test_transport_gateway_compatibility.py`
    - 从“alias 应存在”改为“alias 不得出现”，与架构文档、README 和实现终态统一。

## 2. 本次同步的原因

1. 当前相关 diff 为 `11 files changed, 114 insertions(+), 29 deletions(-)`，已构成一个完整且可收口的 `Wave 3A` 文档同步批次。
2. 若不补齐本次同步，工作区会同时存在两套冲突口径：
   - 代码已删除 gateway/tcp legacy alias
   - 部分文档仍把这些 alias 视为活跃兼容壳
3. `Wave 3A` 已完成串行验证并落盘到 `integration/reports/verify/wave3a-closeout/`，因此文档必须把“实现终态”和“验证终态”一起固化。

## 3. 刻意未改的内容

1. 未改 `control-runtime` / `control-cli` 的 owner 切分文档，因为这不在 `Wave 3A` scope。
2. 未改 `docs/runtime/` 或 `docs/troubleshooting/`，因为本阶段没有引入新的运行参数、部署路径或故障处理流程。
3. 未创建 `Wave 3B` 的实施文档，因为当前裁决仍是 `Wave 3B planning = Go`、`Wave 3B implementation = No-Go`。
4. 未触碰与当前收尾无关的脏文件，避免在不干净工作树中扩大写集。

## 4. 结论

1. `Wave 3A` 的 README、architecture 文档、兼容测试和 closeout 证据现在已对齐到同一 canonical-only 结论。
2. 当前文档同步已足以支撑 reviewer 判断：
   - gateway/tcp legacy alias 已退出活跃设计
   - 防回流测试与阶段验证证据均已落盘
   - 后续只能进入 `Wave 3B` 规划，不能跳过规划直接实施
