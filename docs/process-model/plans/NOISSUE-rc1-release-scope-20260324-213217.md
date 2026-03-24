# RC Scope Freeze

## 1. 基本信息

1. 目标版本：`0.1.0-rc.1`
2. 分支：`refactor/arch/NOISSUE-architecture-refactor-spec`
3. `branchSafe`：`refactor-arch-NOISSUE-architecture-refactor-spec`
4. `ticket`：`NOISSUE`
5. 冻结时间：`2026-03-24 21:32:17 +08:00`
6. 审查基线：
   - `HEAD`：`9a04f8e1e5b179550582852552db8add180678aa`
   - `origin/main`：`9ac9f5aa`
   - 分支相对 `origin/main`：`ahead 3 / behind 0`

## 2. 本次冻结口径

1. 本轮 `RC` 不按“干净分支增量”定义范围，而按 `2026-03-24 21:32:17 +08:00` 的当前工作树快照定义候选基线。
2. 进入 premerge、QA、release-check 的对象，是“当前仓内快照是否能形成可验证的候选版”，不是“当前分支是否已经达到可直接合并/打 tag 的提交洁净度”。
3. 本轮只推进仓内 `RC evidence` 生成，不默认包含以下动作：
   - 创建 tag
   - push / PR
   - 仓外交付包复核
   - 现场脚本观察期复核
   - 把当前快照声明为正式稳定版

## 3. 当前工作树快照

1. `git diff --stat HEAD`：`172 files changed, 6119 insertions(+), 4845 deletions(-)`
2. `git status --short` 共 `351` 条状态项，结构如下：
   - staged：`11`
   - unstaged：`340`
   - untracked：`351`
3. 变更主要集中在以下根目录：
   - `packages`：`84`
   - `apps`：`20`
   - `docs`：`19`
   - `.agents`：`11`
   - `tools`：`7`
   - `integration`：`5`
   - `shared`：`4`
   - `config`：`4`
   - `scripts`：`3`
4. 暂存区当前主要覆盖 `tools/`、`shared/`、`scripts/`、`cmake/`、`docs/`、`specs/` 以及根级 `build/test/ci` 入口，说明当前候选版不只是单一代码修补，而是工作区模板重构、验证脚本切换与文档收口的组合快照。

## 4. 纳入本轮 RC 的内容

1. `workspace template refactor` 主线相关代码、脚本、文档和规范资产。
2. 根级构建/测试/验证入口的切换与联动更新。
3. `shared/`、`tools/`、`scripts/`、`cmake/` 等新根或重构根引入的仓内门禁资产。
4. 与本轮仓内验证直接相关的 `CHANGELOG.md`、review 报告、release evidence。

## 5. 明确排除项

1. 仓外发布包、回滚包和现场脚本的再次观察，不作为本轮仓内 `RC` 通过条件。
2. `hardware smoke` 不作为本轮候选版必须 gate，除非后续执行命令时显式补跑。
3. 本轮不对“当前工作树中的每一项变更都已具备独立提交边界”作出正向声明。

## 6. 冻结约束

1. 从本文件生成后，到本轮 QA / release-check 结束前，不应继续向当前工作树引入新的无关改动。
2. 若验证过程中必须修复阻断项，只允许修复 gate 失败的直接原因，并在 QA / release 报告中显式记录。
3. 若后续工作树继续扩大，则本文件只代表 `2026-03-24 21:32:17 +08:00` 的候选版审查口径，新增部分需要单独补审。

## 7. 风险说明

1. 当前候选版的最大风险不是“缺少基线提交”，而是“工作树过宽且混合 staged / unstaged / untracked”，因此 RC 证据只能证明当前快照可验证，不能替代后续白名单提交与最终 merge 审查。
2. 当前候选版同时包含产品改动与流程资产改动，后续若要做对外发布，需要进一步拆分“发布必要项”和“流程治理项”。
3. `CHANGELOG.md` 在本文件生成时仍停留在过时的 `Unreleased` 条目，必须在执行 `release-check.ps1` 前完成修正。
