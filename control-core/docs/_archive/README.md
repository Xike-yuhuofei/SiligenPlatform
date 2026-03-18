# docs/_archive(过程文档归档区)

**用途**: 存放 Claude Code 自动生成、会议记录、探索草稿、一次性分析、临时排查记录等"过程产物"。

**原则**
- 这里的内容**不作为权威**,可能过时、冲突、未经验证。
- 任何内容若被证明长期有效,应"提炼"进 `docs/library/`(高级文档库)。

**建议归档方式**
- 按月分桶: `docs/_archive/2026-01/xxx.md`
- 或按主题: `docs/_archive/connectivity/xxx.md`

**提炼到 library 的标准(三分法)**

将文档稳定性分为三类,避免主观争议:

* **Strong(代码/配置可证明)**: API文档、配置项、IO映射、启动命令、参数默认值
  → 进 `library/reference` 或 `library/guides`
  → 优先让它"可从代码生成/校验"

* **Medium(流程/操作经验)**: 部署步骤、排障步骤、现场SOP、测试指南
  → 进 `library/guides` 或 `library/runbook`
  → 必须写清前置条件与适用范围(Scope)

* **Weak(一次性推演/讨论)**: 探索草稿、对比分析、会议记录、临时排查记录
  → 留 `_archive`
  → 最多在 library 里引用链接,不当权威

**一句话:** 能被事实验证的进 library; 只能靠当时语境成立的留 archive。
