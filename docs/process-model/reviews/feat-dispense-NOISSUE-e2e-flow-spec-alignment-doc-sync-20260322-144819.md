# Doc Sync Review

- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 时间戳：20260322-144819
- 范围：Wave 2A bridge 收口 / Wave 2B 取证口径同步

## 1. 本次同步的文档

1. `docs/process-model/plans/NOISSUE-wave2a-path-bridge-20260322-140814.md`
   - 修正“bridge 前基线”和“当前工作树事实”混写问题
   - 明确 bridge 当前已有 4 个 wrapper，其中前 2 个解除默认入口阻塞，后 2 个稳定集成脚本入口
2. `docs/architecture/performance-baseline.md`
   - 将工作区默认采样入口改为 `tools/engineering-data-bridge/scripts/*`
   - 补充“owner 仍是 `packages/engineering-data`”说明
3. `docs/architecture/dxf-pipeline-strangler-progress.md`
   - 补充工作区稳定脚本入口已锚定到 bridge
4. `docs/architecture/dxf-pipeline-final-cutover.md`
   - 将工作区脚本入口改成 bridge
   - 把 owner 入口单独列出，避免误判为 owner 已迁移
5. `packages/engineering-data/README.md`
   - 增加工作区 bridge 入口和 owner 入口的区分
6. `packages/engineering-data/docs/compatibility.md`
   - 补充 bridge 在工作区内的合法调用地位

## 2. 新增治理工件

1. `NOISSUE-wave2a-closeout-20260322-144819.md`
2. `NOISSUE-test-plan-20260322-144819.md`
3. `NOISSUE-path-inventory-20260322-144819.md`
4. `NOISSUE-blocker-register-20260322-144819.md`
5. `NOISSUE-wave2b-readiness-20260322-144819.md`

## 3. 口径结论

1. 当前工作区默认入口已经切到 `tools/engineering-data-bridge`
2. `packages/engineering-data` 仍是实现 owner，不存在“owner 已迁出”的文档暗示
3. `Wave 2A` 与 `Wave 2B` 的边界已分开：前者是默认入口收口，后者仍是 `No-Go` 的切换前取证

## 4. 待保持的约束

1. 后续若修改 root build graph 或 runtime 默认资产路径，必须新增独立文档同步，不可复用本次 `Wave 2A` 口径
2. 任何文档若描述“当前默认入口”，都必须优先写 bridge 路径，而不是 owner 内部脚本路径
