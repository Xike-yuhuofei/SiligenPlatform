# ADR-003 Versioning And Release

- 状态：`Accepted`
- 日期：`2026-03-18`

## 背景

`SiligenSuite` 已经建立根仓 canonical 工作流，但仍缺少正式版本治理基线。

当前风险不在“会不会写 tag 名”，而在以下几件事长期缺位：

1. 版本号还没有以根仓为唯一中心
2. apps、contracts、integration 报告没有被纳入同一套 release gate
3. legacy 兼容产物仍会影响交付，但没有被正式写进 release 和 rollback 口径
4. 根仓当前 `No commits yet`，意味着连第一个可打 tag 的不可变锚点都还没建立

如果现在不冻结版本治理规则，后续最容易发生的问题是：

- 子项目各自说自己“可发布”，但根仓没有统一版本
- 只有 tag，没有 release evidence、changelog 和 rollback 快照
- 把 `control-core` 或其他 legacy 产物单独当作发布基线，绕开根仓治理

## 决策

### 1. 根仓版本号是唯一对外版本

从本 ADR 生效起，`SiligenSuite` 对内对外都以根仓版本号为唯一正式版本号。

含义：

- HMI、DXF editor、contracts、integration 报告都挂在同一个根仓版本之下
- 不再允许由 `control-core` 单独决定整个交付版本
- 子目录可以保留内部兼容版本信息，但不能替代根仓 release version

### 2. 采用 root-centric SemVer

根仓版本格式统一为：

- 正式版：`MAJOR.MINOR.PATCH`
- 候选版：`MAJOR.MINOR.PATCH-rc.N`

当前阶段采用 `0.x` 起步。

判定规则：

- `MAJOR`：根仓级 breaking release
- `MINOR`：发布面、入口面、契约面、交付面发生可感知变化
- `PATCH`：不改变外部发布契约的修复和治理修补
- `-rc.N`：用于锁定重构中期的候选发布基线

### 3. Tag 采用根仓 annotated tag

根仓 release tag 统一命名为：

- `siligensuite/vMAJOR.MINOR.PATCH`
- `siligensuite/vMAJOR.MINOR.PATCH-rc.N`

只允许在根仓提交上创建 annotated tag。

### 4. CHANGELOG 是唯一版本日志

`CHANGELOG.md` 作为根仓版本日志的唯一人类可读入口。

要求：

1. 每个 tag 必须对应一个 changelog 条目
2. changelog 必须覆盖 apps、contracts、integration、known issues、rollback notes
3. `Unreleased` 是日常累积区，打 tag 前必须整理到目标版本条目

### 5. Release Gate 必须覆盖 apps、contracts、integration

正式发版前，必须同时通过或显式记录以下三类证据：

1. apps
   - 根级 app 入口 dry-run / smoke 结论
2. contracts
   - `packages` suite
   - `protocol-compatibility` suite
   - 若 `editor-contracts` 有变更，则增加人工复核
3. integration
   - `integration\reports\releases\<version>\ci\workspace-validation.{md,json}`

### 6. Rollback Point 采用四件套

一个可回滚的 release 不是只有 tag，而是以下四件套：

1. annotated tag
2. 对应提交 hash
3. release evidence 目录
4. 配套快照

配套快照至少包括：

- `config\`
- `data\recipes\`
- `control-core\build\bin\<Config>\`
- `dxf-pipeline\`（如发布范围包含 DXF 预览）

### 7. 当前状态只允许定义候选基线，不假设已有稳定版

由于当前存在以下事实：

- 根仓还没有提交基线
- `apps/control-runtime` 仍返回 `BLOCKED`
- `control-tcp-server` 与 `control-cli` 仍依赖 legacy 产物
- 现场交付仍依赖手工拷贝和外部环境

因此当前 ADR 不认定“已经存在稳定正式版”，只认定“可以先建立候选发布治理基线”。

## 结果

本 ADR 生效后，团队要按以下方式推进版本治理：

1. 先补根仓首个可打 tag 的提交基线
2. 用 `CHANGELOG.md` 管理 `Unreleased` 与目标版本条目
3. 通过根级 `release-check.ps1` 生成 release evidence
4. 有阻塞项时，只允许 `-rc.N`
5. 阻塞项清零后，才允许无后缀正式版

## 不采纳的方案

### 1. 继续由 `control-core` 单独决定版本

不采纳原因：

- 根仓已经有统一入口、统一文档、统一报告，不应再让 legacy 子树代管整仓版本

### 2. 只打 tag，不维护 changelog 和 rollback 快照

不采纳原因：

- 这种做法没有形成可审查、可交付、可回滚的 release 基线

### 3. 先发一个“默认稳定版”，再慢慢补 gate

不采纳原因：

- 当前事实并不支持“稳定版”结论，会误导团队和现场

## 关联文档

- `docs/runtime/release-process.md`
- `docs/architecture/workspace-baseline.md`
- `docs/architecture/build-and-test.md`
- `docs/onboarding/developer-workflow.md`
