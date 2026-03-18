# 跨格式映射

## Canonical 边界

- `.pb` canonical：`proto/v1/dxf_primitives.proto`
- Preview Artifact canonical：`schemas/v1/preview-artifact.schema.json`
- Simulation Input canonical：`schemas/v1/simulation-input.schema.json`

## 重要声明

- Preview canonical 是 `dxf-pipeline` CLI `--json` 输出的 `PreviewArtifact`。
- HTML 预览文件中的内嵌脚本数据不是 preview canonical。
- Simulation input canonical 是 `packages/simulation-engine` 当前 loader / bridge 可直接读取的段级 JSON，不是 `.path.json` 或 `.traj.json`。

## `.pb` -> Simulation Input JSON

| `.pb` 概念 | `.pb` 字段 | JSON 字段 | 规则 |
| --- | --- | --- | --- |
| 直线 | `Primitive.line` | `segments[].type = "LINE"` | `start/end` 坐标直接映射 |
| 圆弧 | `Primitive.arc.start_angle_deg` / `end_angle_deg` | `segments[].start_angle` / `end_angle` | 必须显式从角度制转换到弧度制 |
| 圆 | `Primitive.circle` | 两个 `ARC` segment | 当前实现拆成两个半圆弧，不能静默改成单个自定义 `CIRCLE` |
| spline | `Primitive.spline` | 多个 `LINE` segment | 当前 consumer 不支持 spline，必须显式线性化 |
| ellipse | `Primitive.ellipse` | 多个 `LINE` segment | 当前 consumer 不支持 ellipse，必须显式线性化 |
| contour(line/arc) | `Primitive.contour.elements[]` | `LINE` / `ARC` | 能保留 line/arc 时优先保留 |
| contour(spline) | `Primitive.contour.elements[].spline` | 多个 `LINE` segment | 不能静默丢弃 |
| point | `Primitive.point` | 无 `segments` 输出 | 当前 `packages/simulation-engine` consumer 仅接受 path segments；跳过 point 时必须通过导出 note/document 显式说明 |

## 命名不一致映射

| 概念 | `.pb` 命名 | JSON 命名 | 说明 |
| --- | --- | --- | --- |
| 起始角 | `start_angle_deg` | `start_angle` | 单位不同，`.pb` 为度，JSON 为弧度 |
| 结束角 | `end_angle_deg` | `end_angle` | 单位不同，`.pb` 为度，JSON 为弧度 |
| 触发点位置 | 无直接字段 | `trigger_position` | 由上游坐标点投影为累计路径长度 mm |

## Preview JSON -> HMI

`apps/hmi-app` 当前读取以下字段：

- `preview_path`
- `entity_count`
- `segment_count`
- `point_count`
- `total_length_mm`
- `estimated_time_s`

canonical preview 还额外包含：

- `width_mm`
- `height_mm`

这两个字段当前可被消费者忽略，但不能从 canonical 中静默删除。
