# Contract: 工作区冻结文档集契约

## 1. 合同标识

- Contract ID: `dsp-e2e-spec-docset-v1`
- Scope: `D:\Projects\SS-dispense-align\docs\architecture\dsp-e2e-spec\`
- Reference Pack: `D:\Projects\SS-dispense-align\docs\architecture\dsp-e2e-spec\`

## 2. 目的

定义当前工作区 9 轴正式文档集的最小交付面、评审责任和冻结条件，确保参考模板与当前工作区交付物之间保持一一对应，不发生缺轴、合并失真或路径漂移。

## 3. 文档面定义

| 轴 | 参考源 | 目标文档 | 必须覆盖的主题 | 主评审角色 |
|---|---|---|---|---|
| S01 | `dsp-e2e-spec-s01-stage-io-matrix.md` | `dsp-e2e-spec-s01-stage-io-matrix.md` | S0-S16 阶段输入/输出、成功事件、失败事件、回退入口 | 架构 + 测试 |
| S02 | `dsp-e2e-spec-s02-stage-responsibility-acceptance.md` | `dsp-e2e-spec-s02-stage-responsibility-acceptance.md` | 每阶段职责、owner、验收条件、禁止短路 | 架构 |
| S03 | `dsp-e2e-spec-s03-stage-errorcode-rollback.md` | `dsp-e2e-spec-s03-stage-errorcode-rollback.md` | 失败分层、错误码、回退目标、人工放行边界 | 架构 + 业务 |
| S04 | `dsp-e2e-spec-s04-stage-artifact-dictionary.md` | `dsp-e2e-spec-s04-stage-artifact-dictionary.md` | 对象字典、字段、引用、版本、owner | 架构 |
| S05 | `dsp-e2e-spec-s05-module-boundary-interface-contract.md` | `dsp-e2e-spec-s05-module-boundary-interface-contract.md` | 模块边界、命令、事件、禁止越权 | 架构 |
| S06 | `dsp-e2e-spec-s06-repo-structure-guide.md` | `dsp-e2e-spec-s06-repo-structure-guide.md` | 当前工作区 canonical roots、owner 落位、support/vendor 隔离 | 架构 + 平台 |
| S07 | `dsp-e2e-spec-s07-state-machine-command-bus.md` | `dsp-e2e-spec-s07-state-machine-command-bus.md` | 状态机、命令总线、幂等、归档、阻断 | 架构 + 运行时 |
| S08 | `dsp-e2e-spec-s08-system-sequence-template.md` | `dsp-e2e-spec-s08-system-sequence-template.md` | 主成功链、阻断链、回退链、恢复链、归档链时序 | 架构 + 业务 |
| S09 | `dsp-e2e-spec-s09-test-matrix-acceptance-baseline.md` | `dsp-e2e-spec-s09-test-matrix-acceptance-baseline.md` | 测试矩阵、验收基线、证据要求、失败链覆盖 | 测试 |

陪伴索引文档:

| 文档 | 参考源 | 目标文档 | 作用 |
|---|---|---|---|
| S10 | `dsp-e2e-spec-s10-frozen-directory-index.md` | `dsp-e2e-spec-s10-frozen-directory-index.md` | 索引 9 轴、术语清单、路径和审阅顺序，不替代任何正式轴 |

## 4. 内容义务

每个目标文档必须同时包含以下内容面:

1. 当前工作区事实来源，至少一个真实路径或现存文档证据。
2. 与参考模板的对应关系和差距说明。
3. 目标 owner、禁止越权规则和完成判定。
4. 若存在 legacy 术语，必须仅在迁移映射节出现一次，并明确其作废后的正式名称。
5. 关联的 build/test/验收证据或应补充的证据路径。

## 5. 强制一致性规则

1. `ExecutionPackageBuilt` 与 `ExecutionPackageValidated` 必须在 S01、S02、S03、S07、S08、S09 六个轴中保持同样的拆分语义。
2. `PreflightBlocked` 必须在 S01、S02、S03、S07、S08、S09 中统一表达为执行门禁阻断，而不是规划失败。
3. `WorkflowArchived` 只能由追溯/归档 owner 产出，不能在任何轴中被描述为“日志副作用”。
4. `ArtifactSuperseded` 必须在对象字典、模块契约、状态机、时序图、测试矩阵中都是正式事实。
5. 9 轴文档中的模块名、对象名、状态名、关键事件名必须全部使用参考模板规范名。

## 6. 评审接口

| 角色 | 必须回答的问题 | 阻断条件 |
|---|---|---|
| 架构 | 每个阶段、对象、模块是否有唯一 owner 和清晰边界 | 任一主链对象多 owner 或阶段职责重叠 |
| 业务 | 业务逻辑纠偏是否都能回链到模板对齐或验收缺口 | 出现无法解释为何必须调整的行为变更 |
| 测试 | 关键链路是否都能回链到阶段、对象、状态、事件、失败码 | 只覆盖主成功链，缺失阻断/回退/恢复/归档链 |
| 平台 | 仓库路径是否仍以 canonical roots 为默认入口 | 新增 legacy 默认路径或 support 目录变成 owner |

## 7. 冻结完成判定

文档集只有在以下条件同时满足时才可标记为 `frozen`:

1. S01-S09 目标文档全部存在并通过对应主评审角色审阅。
2. S10 索引已列出所有正式文档、术语、当前事实来源和评审顺序。
3. 根级 `build.ps1`、`test.ps1`、`ci.ps1` 的验证口径已在 S06/S09 中被明确引用。
4. 所有 legacy 术语仅保留在迁移映射节，不再作为正式别名。
