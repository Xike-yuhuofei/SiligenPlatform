# 迁移状态

## 已完成

- 从旧仓库迁入 `src/`、`tests/`、`scripts/`、`docs/`、`third_party/` 等主控核心目录。
- 排除 `src/adapters/hmi`，保持 HMI 与控制核心解耦。
- 完成新位置下的 CMake 配置验证。

## 过渡项

- 本地 `proto/` 副本已移除，构建改为优先消费 `../dxf-pipeline/proto/`。
- 旧仓库中的一些文档仍带有单仓时代描述，后续需逐步收敛到本子项目语境。

## 下一步建议

- 已将明显属于 HMI 的计划文档迁移到 `hmi-client/docs/migrated-from-control-core/`。
- 评估 `proto/` 从 `dxf-pipeline` 统一生成与同步的方案。
- 按功能域逐步整理测试与文档归属。

## 最近清理进展

- 已将明显偏 DXF 离线算法、预处理和轨迹优化的文档迁移到 `dxf-pipeline/docs/migrated-from-control-core/`。
- `control-core` 当前保留的 DXF 文档更聚焦于执行链路、架构修复与运行时问题。

## Editor 文档筛查结果

- 已完成对 control-core/docs 的第二轮 editor 向文档筛查。
- 未发现需要继续迁移到 dxf-editor 的明确文档真源；editor 主文档仍以原工具目录迁入内容为主。
