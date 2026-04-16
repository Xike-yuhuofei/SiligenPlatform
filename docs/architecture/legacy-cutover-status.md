# Legacy Cutover Status

更新时间：`2026-04-10`

本文是 `SiligenSuite` 当前 legacy / bridge / cutover 状态的正式收束页。  
它只保留仍然有效的 live 结论；执行过程、专项审计和阶段性 closeout 已统一下沉到 [history/closeouts/](./history/closeouts/README.md) 与 [history/progress/](./history/progress/README.md)。

## 1. 当前正式结论

- 工作区当前正式根仍为 `apps/`、`modules/`、`shared/`、`docs/`、`samples/`、`tests/`、`scripts/`、`config/`、`data/`、`deploy/`
- `packages/`、`integration/`、`tools/`、`examples/` 已退出默认 owner 与默认入口，由 layout / legacy-exit 门禁防回流
- `specs/` 与 `.specify/` 只允许作为本地缓存存在，不参与当前正式基线
- legacy path 不是默认入口；兼容壳与历史保留路径只允许按冻结策略受控存在
- 当前 bridge/legacy contract 可以通过，但 broader owner 收口尚未全部完成，因此整体 closeout 仍不得宣称完全完成

## 2. 当前状态分层

### 已收口为正式默认口径

- 根级 build/test/CI/local validation gate 已统一切到单轨入口
- `control-apps` canonical binary 路径已成为默认产物入口
- `dxf-pipeline` 已退出默认运行链路
- runtime contracts alias shell 已从 canonical required surface 中移除

### 仍需按冻结策略管理

- `control-core/modules/device-hal`
- `control-core/src/shared`
- `control-core/src/infrastructure`
- `control-core/third_party`

这些对象当前仍影响 broader owner 收口或 shared/device seam，不应被误写成“已经完全退出”。

## 3. 当前正式引用规则

1. 需要当前状态判断时，优先引用本文
2. 需要目录级删除/防回流规则时，引用 `governance/migration/legacy-deletion-gates.md` 与 `governance/migration/legacy-exit-checks.md`
3. 需要查看某次 cutover、删除或迁移执行证据时，进入 [history/closeouts/](./history/closeouts/README.md)
4. 需要查看专项进展或实施清单时，进入 [history/progress/](./history/progress/README.md)

## 4. 当前仍未完成项

- `runtime-execution` / `runtime-service` 一侧的 broader owner 收口与文档同步尚未结束
- device/shared seam 仍有历史承接面，不能把当前状态外推为“legacy 全量清零”
- 历史执行文档中的旧路径、旧工作树和旧 evidence 只保留审计价值，不再承担正式入口职责
