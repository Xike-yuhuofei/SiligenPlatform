# External Migration Observation

更新时间：`2026-03-22`

## 1. 适用范围

本文档定义正式发布前 `G7` 仓外交付物观察的当前 SOP。目标是确认仓外已部署环境、发布包、运维脚本和回滚包已经完全切到当前 canonical 入口，而不是继续依赖历史 alias、compat shell 或错误配置根。

历史 `Wave 4*` 观察过程与 closeout 证据保留在 `docs/process-model/`，仅用于追溯；当前正式执行口径以本文为准。

当前仓内已确认：

- `dxf-pipeline` compat shell 已退出
- legacy DXF CLI alias 已退出
- `config\machine_config.ini` root alias 已删除

因此当前仓外交付物观察只接受两类结论：

- `Go`：仓外交付物只使用 canonical 入口
- `No-Go`：仍残留 legacy 入口，需要替换仓外交付物或脚本

不允许的处理方式：

- 恢复 `dxf-pipeline` compat shell
- 恢复 legacy CLI alias
- 重新引入 `config\machine_config.ini` alias 文件

## 2. 证据落位

当前正式观察证据应整理到独立批次目录，不要覆写历史批次证据。例如：

```text
tests/reports/verify/external-observation-<yyyyMMdd-HHmmss>/
  intake/
  observation/
```

目录约束：

- `intake/` 只记录外部输入的原始路径、采集时间、目录清单或 hash、扫描命令。
- `observation/` 只记录对真实外部输入执行后的扫描结果与 `Go/No-Go` 裁决。
- 原始发布包、现场脚本包、回滚包或 SOP 快照仍保留在原位置，不复制进仓库。

推荐执行入口：

```powershell
.\scripts\validation\register-external-observation-intake.ps1 -Scope field-scripts -SourcePath <field-script-root>
.\scripts\validation\register-external-observation-intake.ps1 -Scope release-package -SourcePath <unpacked-release-root>
.\scripts\validation\register-external-observation-intake.ps1 -Scope rollback-package -SourcePath <rollback-root>
.\scripts\validation\run-external-migration-observation.ps1 -PythonExe <target-python> -ReportRoot tests\reports\verify\external-observation-<yyyyMMdd-HHmmss>
```

说明：

- 若 `run-external-migration-observation.ps1` 未显式传入某个路径，会回退读取当前 `ReportRoot\intake\*.json` 中记录的 `source_path`。
- `register-external-observation-intake.ps1` 会同时生成 `.md` 与 `.json` sidecar，供人工查看和脚本复用。
- 若 `source_path` 指向单文件归档而不是解包目录，观察脚本会明确报 `input-gap`，要求先提供可扫描目录。

建议最少保留：

- 命令输出或日志摘录
- 发布包目录清单或截图
- 回滚包/备份包目录清单或截图
- 最终 `Go/No-Go` 结论摘要

## 3. 检查项

### 3.1 外部 DXF launcher 环境

检查对象：

- 外部 Python 虚拟环境
- 现场工作站 PATH / wrapper / launcher
- 调用 `engineering-data-*` 的自动化脚本

建议动作：

```powershell
python -c "import engineering_data"
python -m engineering_data.cli.dxf_to_pb --help
python -m engineering_data.cli.export_simulation_input --help
python -m engineering_data.cli.path_to_trajectory --help
python scripts/engineering-data/generate_preview.py --help
where.exe engineering-data-dxf-to-pb
where.exe engineering-data-export-simulation-input
where.exe engineering-data-path-to-trajectory
where.exe engineering-data-generate-preview
```

人工确认：

- 目标 Python 解释器能导入 `engineering_data`
- 目标 Python 解释器能运行 canonical module CLI，与 workspace preview wrapper
- 不再调用 `dxf-to-pb`
- 不再调用 `path-to-trajectory`
- 不再调用 `export-simulation-input`
- 不再调用 `generate-dxf-preview`
- 不再导入 `dxf_pipeline.*` 或 `proto.dxf_primitives_pb2`
- console script 可见性只做附加记录，默认不作为单独阻断项

判定：

- 目标 Python 无法运行 canonical module CLI 即 `No-Go`
- 任一 legacy CLI / import 仍存在即 `No-Go`

### 3.2 已部署脚本与现场运维脚本

检查对象：

- 现场启动脚本
- 运维/巡检/升级脚本
- 已发布包内的 wrapper、bat、ps1、py、scr 及文本型配置脚本

建议动作：

```powershell
rg -n "dxf-to-pb|path-to-trajectory|export-simulation-input|generate-dxf-preview|dxf_pipeline|machine_config\.ini" <unpacked-release-root> -g '*.ps1' -g '*.py' -g '*.bat' -g '*.cmd' -g '*.scr' -g '*.ini' -g '*.json' -g '*.yaml' -g '*.yml' -g '*.xml' -g '*.cfg' -g '*.conf' -g '*.toml' -g '*.txt' -g '*.md'
```

人工确认：

- 只允许出现 `config\machine\machine_config.ini`
- 不允许出现 `config\machine_config.ini`
- 不允许把 legacy DXF 名称写成默认命令

判定：

- 任一脚本把 legacy 入口当默认值即 `No-Go`

### 3.3 发布包内容

检查对象：

- 工作区发布包
- Python wheel / venv
- 现场拷贝包

人工确认：

- 发布包内不存在 `config\machine_config.ini`
- 发布包内不存在 `dxf_pipeline` 源码或 shim
- 发布包内不存在 `proto\dxf_primitives_pb2.py`
- 发布说明只引用 canonical CLI 与 `docs/runtime/external-dxf-editing.md`

判定：

- 任一已删除 alias / compat shell 再次出现在交付物中即 `No-Go`

### 3.4 回滚包与备份内容

检查对象：

- 回滚包
- 现场备份包
- 回滚 SOP

人工确认：

- 回滚只恢复 `config\machine\machine_config.ini`
- 回滚只恢复 `data\recipes\`
- 回滚 control app 时只恢复 canonical build root 产物
- SOP 中不再写 `config\machine_config.ini` bridge

判定：

- 若回滚说明仍依赖 alias 文件或 legacy DXF 入口即 `No-Go`

## 4. 命中问题时的动作

1. 阻断“仓外迁移完成”宣告。
2. 阻断受影响发布包继续下发。
3. 用当前 canonical 包、canonical 配置和 canonical CLI 替换仓外交付物。
4. 重新执行本观察期文档中的对应检查项。

## 5. 最终结论模板

```text
observation_result = Go | No-Go
scope = external launcher | field scripts | release package | rollback package
evidence = <path or operator note>
next_action = <replace package / fix script / rerun observation>
```
