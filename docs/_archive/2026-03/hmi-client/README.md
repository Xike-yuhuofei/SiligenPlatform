# hmi-client Archive

本目录保存已从工作区删除的 `hmi-client/` 历史材料。

## 删除结论

- `hmi-client/` 已不再是工作区内的源码、脚本或测试入口。
- HMI 的 canonical 目录是 `D:\Projects\SiligenSuite\apps\hmi-app`。
- 对外 Python 分发名和命令入口仍保留为 `hmi-client`，但它们只对应 canonical app，不再对应 legacy 目录。

## 本次归档内容

- 原 `hmi-client/docs/**/*`
- 原 `hmi-client/CHANGELOG.md`
- 原 `hmi-client/COMMIT_CONVENTIONS.md`
- 原 `hmi-client/CONTRIBUTING.md`
- 原 `hmi-client/PROJECT_BOUNDARY_RULES.md`
- 原 `hmi-client/README.md`，已重命名为 `legacy-README.md`

## 未归档内容

- `src/`：与 canonical `apps/hmi-app/src` 重复，且 canonical 版本更完整
- `scripts/`：legacy wrapper，已退出工作区
- `.github/`、`.gitignore`、`.editorconfig`、`.git.nested-backup/`：低价值壳文件
- `logs/`、空 `assets/`：运行噪音或空目录

## 回滚面

- 工作区短期备份：`D:\Projects\SiligenSuite\tmp\legacy-removal-backups\20260319-151820\hmi-client`
- 长期历史：Git 历史 + 本 archive
