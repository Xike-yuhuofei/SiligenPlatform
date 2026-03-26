# NOISSUE Wave 4B Architecture Plan - Current-Fact Doc Canonicalization and External Observation

- 状态：Prepared
- 日期：2026-03-22
- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 工作流上下文：`ticket=NOISSUE`，`timestamp=20260322-210606`
- 关联 scope：`docs/process-model/plans/NOISSUE-wave4b-scope-20260322-210606.md`

## 1. 目标结构

```text
active docs
  -> README / WORKSPACE / onboarding / runtime / troubleshooting / HIL README
      -> current-fact only
      -> repo-root relative commands
      -> canonical config path only

external observation
  -> docs/runtime/external-migration-observation.md
      -> external launcher checks
      -> field script checks
      -> release package checks
      -> rollback package checks

verification evidence
  -> integration/reports/verify/wave4b-closeout/docs
  -> integration/reports/verify/wave4b-closeout/observation
  -> integration/reports/verify/wave4b-closeout/legacy-exit
```

## 2. 组件职责

### 2.1 活跃文档层

1. 只描述当前仓内事实和 canonical 入口。
2. 对历史 `control-core`、`dxf-pipeline` 只保留“历史/迁移背景”语义，不再写成当前依赖。
3. 对运行命令统一写成“在仓库根执行”的相对路径形式。

### 2.2 观察期文档层

1. `docs/runtime/external-migration-observation.md` 是仓外确认的唯一入口。
2. 文档必须把仓外检查拆成 launcher、脚本、发布包、回滚包四组。
3. 命中 legacy 入口后的动作必须固定为阻断和替换，不允许恢复 compat shell。

### 2.3 证据层

1. 文本扫描报告负责证明活跃文档 current-fact 已收口。
2. `legacy-exit` 报告负责证明仓内门禁仍维持 DXF compat shell 和 alias 退出状态。
3. 观察期准备摘要负责说明仓外人工确认的目标、证据路径和未完成边界。

## 3. 失败模式与补救

1. 失败模式：文档仍保留绝对路径或 alias 文案
   - 补救：按活跃文档清单统一改为 repo-root 相对命令和 canonical 路径。
2. 失败模式：把仓内 closeout 误写成仓外迁移完成
   - 补救：所有发布/部署/回滚文档增加“仓外需人工确认”的边界说明。
3. 失败模式：现场因发现 legacy 入口而要求恢复 compat shell
   - 补救：按观察期文档阻断发布并替换仓外交付物，不回退仓内实现。

## 4. 回滚边界

1. 本阶段只涉及文档和报告文件。
2. 若表述调整需要回退，只回退本阶段新增或修改的文档，不影响产品代码和门禁实现。
