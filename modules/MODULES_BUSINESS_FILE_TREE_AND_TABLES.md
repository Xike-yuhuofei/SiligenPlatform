# modules 业务文件树与实现文件说明

状态：已退役为非 authoritative 占位页。

## 说明

- 该文件曾承载自动生成的模块文件树与业务说明。
- 多轮边界审查已确认，这类全仓快照无法稳定反映当前 owner、构建图和 live surface。
- 因此本路径不再承载模块边界、owner 判定或实现真值，也不再保留历史生成正文。

## 当前正式真值来源

- `docs/architecture/modules-owner-boundary.md`
- `docs/architecture/dsp-e2e-spec/`
- 各模块自己的 `README.md`
- 各模块自己的 `module.yaml`
- 各模块自己的 `CMakeLists.txt`

## 使用规则

- 不得再把本页用于 owner 判定、边界审查、freeze 评审或实现盘点。
- 如需一次性的文件树盘点，必须生成 dated review artifact，作为审查附件保存，不得回写本页充当长期真值。
