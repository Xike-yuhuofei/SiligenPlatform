# hmi-client

`hmi-client` 现在是 `apps/hmi-app` 的兼容壳目录，不再承载 canonical HMI 实现。

## Canonical 路径

- 根级 canonical app：`D:\Projects\SiligenSuite\apps\hmi-app`

## 当前角色

- 保留历史项目名、兼容脚本与迁移说明
- `scripts/run.ps1` 与 `scripts/test.ps1` 会转发到 canonical app
- 后续需要说明 HMI 入口位置时，应优先引用根级 canonical 路径

## 当前迁移状态

- 真实运行源码已收口到 `D:\Projects\SiligenSuite\apps\hmi-app`
- legacy 目录不再负责后端自动拉起与依赖治理
- legacy `tests/`、`pyproject.toml`、`requirements.txt`、`packaging/` 已在第一批清理中移除，避免继续从旧目录发起安装、测试或打包
- legacy `.github/` 下的 workflow/template 文件已在第二批清理中移除，CI 统一走根级 `.github/workflows/workspace-validation.yml`

## 目录说明

- `scripts/run.ps1`：转发到 `apps/hmi-app/run.ps1`
- `scripts/test.ps1`：转发到 `apps/hmi-app/scripts/test.ps1`
- `scripts/build.ps1`：对 `apps/hmi-app/src` 执行最小编译检查，不再构建 legacy `src/`
- `src/`、`docs/`、`assets/` 仍保留为迁移参考与兼容观察面
- 其他 legacy 安装/测试面已删除，不再作为入口保留
- legacy 本地 CI 壳文件已删除，不再保留可执行的子目录级 workflow

## 常用命令

```powershell
./scripts/run.ps1
./scripts/test.ps1
```

根级统一入口：

```powershell
D:\Projects\SiligenSuite\apps\hmi-app\run.ps1
```

## 兼容说明

- 新的显式 launcher 契约见 `D:\Projects\SiligenSuite\apps\hmi-app\README.md`
- 旧的 `SILIGEN_CONTROL_CORE_ROOT`、`SILIGEN_TCP_SERVER_EXE` 路径猜测逻辑不再是 canonical 行为
- 本目录不再是工作区层面的 canonical app 目录
- 禁止新增从本目录 `src/` 发起新的 import 或 editable install
- `tests/`、`pyproject.toml`、`requirements.txt`、`packaging/` 已移除；如确需恢复，必须先更新根级审计与清理计划
