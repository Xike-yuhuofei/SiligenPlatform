# Trajectory Compare

该目录从旧仓库 `tools/trajectory_compare/` 迁移而来，用于对 `control-core` 的 `siligen_cli dxf-plan` 预览结果进行批量对比。

## 当前布局

- `run_compare.py`：批量运行对比脚本。
- `compare_config.json`：默认对比配置。
- 输出目录默认写入 `benchmarks/reports/trajectory_compare/`。

## 与拆分后仓库的关系

- CLI 可执行文件默认指向 `../control-core/build/bin/siligen_cli.exe`。
- CLI 运行工作目录默认指向 `../control-core`。
- 默认样本使用 `tests/fixtures/imported/uploads-dxf/archive/Demo.dxf`。

## 运行示例

```powershell
python benchmarks/trajectory_compare/run_compare.py
```

## 说明

- 运行前需要先构建 `control-core`，确保 `siligen_cli.exe` 存在。
- 当前脚本仍保持历史用途，后续可视情况继续收敛到 benchmark 工具链。
