# DXF 预处理方案（R12 / 旧式 POLYLINE）

日期：2026-02-03

## 目标
- 输出格式：DXF R12
- 实体类型：仅旧式 `POLYLINE`
- 全部 Z=0（二维化）
- 无 BLOCK/INSERT 引用（全部炸开）
- 无 SPLINE（全部拟合为 POLYLINE）
- 无重叠线段（去重 + 共线合并）
- 保留图层与颜色
- 线型强制为 `CONTINUOUS`
- 脚本位置：`scripts/dxf_preprocess_r12.py`

## 配置（用户提供）
```xml
<?xml version="1.0" encoding="UTF-8"?>
<DXF_Preprocessing_Config>
    <ToleranceSettings>
        <ChordalTolerance unit="DrawingUnit">
            <Value>0.005</Value>
            <Range min="0.001" max="0.01"/>
        </ChordalTolerance>

        <MaxSegmentLength unit="DrawingUnit">
            <Value>5.0</Value>
            <Range min="1.0" max="10.0"/>
        </MaxSegmentLength>

        <OverkillTolerance unit="DrawingUnit">
            <Value>0.001</Value>
            <Range min="0.001" max="0.01"/>
        </OverkillTolerance>

        <AngularTolerance unit="Degree">
            <Value>0.1</Value>
        </AngularTolerance>
    </ToleranceSettings>

    <GlobalConstraints>
        <OutputFormat>R12</OutputFormat>
        <EntityType>POLYLINE_OLD_STYLE</EntityType>
        <FlattenZ>true</FlattenZ>
        <ExplodeBlocks>true</ExplodeBlocks>
        <ForceLinetype>CONTINUOUS</ForceLinetype>
        <PreserveLayer>true</PreserveLayer>
        <PreserveColor>true</PreserveColor>
    </GlobalConstraints>
</DXF_Preprocessing_Config>
```

可选扩展字段（不影响现有配置）：
- `MinSegmentLength`：最小线段长度过滤（单位同 DrawingUnit）


## 处理流程（高层）
1. 读取 DXF，获取 Modelspace。
2. 递归炸开 INSERT/BLOCK，生成仅含直接图元的实体流。
3. 统一二维化：所有顶点/控制点 Z=0；清除 elevation / thickness / extrusion。
4. 所有曲线（SPLINE/ARC/CIRCLE/ELLIPSE）按弦高误差与最大段长拟合为线段。
5. 全部线段按图层+颜色分组。
6. 端点吸附（OverkillTolerance），消除浮点噪声。
7. 去重与共线合并（AngularTolerance + OverkillTolerance）。
8. 可选：过滤过短线段（MinSegmentLength）。
9. 拓扑连接：将首尾相接线段合并为多点 POLYLINE。
10. 以旧式 `POLYLINE` 写出 R12 DXF：
   - 图层与颜色保留
   - 线型强制 `CONTINUOUS`
   - 仅输出 POLYLINE

## 关键算法细节
- **曲线离散**：
  - 使用弦高误差（ChordalTolerance）进行曲线细分。
  - 对分段结果再按 MaxSegmentLength 进行二次细分，保证最大段长。
- **端点吸附**：
  - 端点按 OverkillTolerance 进行网格化吸附（round(x/eps)*eps）。
- **退化线段过滤**：
  - 端点吸附后过滤长度 ≤ OverkillTolerance 的线段，避免零长度线段。
- **容差一致性**：
  - 若 OverkillTolerance 大于 ChordalTolerance，脚本会给出警告提示。
- **去重/合并**：
  - 去重：端点排序后哈希，剔除重复线段。
  - 共线合并：按方向角（AngularTolerance）与法向偏距（OverkillTolerance）分组，
    在投影轴上合并重叠区间，输出最少线段。
- **拓扑连接**：
  - 按端点邻接关系将线段串接为长链，输出多顶点 POLYLINE（可闭合）。

## 参考脚本（Python + ezdxf）
> 说明：该脚本按上方配置执行预处理，输出 R12 旧式 POLYLINE。

```python
import math
import xml.etree.ElementTree as ET

import ezdxf
from ezdxf.disassemble import recursive_decompose
from ezdxf.path import make_path
from ezdxf.math import Vec3

MIN_BEZIER_SEGMENTS = 4


def read_config(xml_path):
    root = ET.parse(xml_path).getroot()

    def f(path, default):
        node = root.find(path)
        return float(node.text) if node is not None and node.text else default

    return {
        "chordal": f("./ToleranceSettings/ChordalTolerance/Value", 0.005),
        "max_seg": f("./ToleranceSettings/MaxSegmentLength/Value", 5.0),
        "overkill": f("./ToleranceSettings/OverkillTolerance/Value", 0.001),
        "ang": f("./ToleranceSettings/AngularTolerance/Value", 0.1),
        "min_seg": f("./ToleranceSettings/MinSegmentLength/Value", 0.0),
    }


def warn_if_tolerance(cfg):
    if cfg["overkill"] > cfg["chordal"]:
        print(
            "WARNING: OverkillTolerance is larger than ChordalTolerance; "
            "this may over-merge geometry."
        )
    if cfg["min_seg"] > 0.0 and cfg["min_seg"] > cfg["max_seg"]:
        print(
            "WARNING: MinSegmentLength is larger than MaxSegmentLength; "
            "this may drop most segments."
        )


def snap_point(p, eps):
    if eps <= 0:
        return Vec3(p.x, p.y, 0.0)
    return Vec3(round(p.x / eps) * eps, round(p.y / eps) * eps, 0.0)


def densify(points, max_seg):
    if len(points) < 2 or max_seg <= 0:
        return points[:]
    out = [points[0]]
    for i in range(1, len(points)):
        a = out[-1]
        b = points[i]
        dist = (b - a).magnitude
        if dist <= max_seg:
            out.append(b)
            continue
        n = int(math.ceil(dist / max_seg))
        for k in range(1, n + 1):
            out.append(a + (b - a) * (k / n))
    return out


def path_to_segments(path, chordal, max_seg, snap_eps):
    pts = list(path.flattening(chordal, segments=MIN_BEZIER_SEGMENTS))
    if not pts:
        return []
    if path.is_closed and (pts[0] - pts[-1]).magnitude > snap_eps:
        pts.append(pts[0])

    pts = [Vec3(p.x, p.y, 0.0) for p in pts]
    pts = densify(pts, max_seg)

    segs = []
    for i in range(1, len(pts)):
        a = pts[i - 1]
        b = pts[i]
        if (b - a).magnitude <= snap_eps:
            continue
        segs.append((a, b))
    return segs


def resolve_color(entity, doc):
    color = entity.dxf.get("color", 256)
    layer_name = entity.dxf.get("layer", "0")
    if color in (0, 256):
        if doc.layers.has_entry(layer_name):
            return doc.layers.get(layer_name).dxf.color
        return 7
    return color


def filter_degenerate(segments, eps):
    if eps <= 0:
        return segments
    out = []
    for a, b in segments:
        if (b - a).magnitude > eps:
            out.append((a, b))
    return out


def filter_short_segments(segments, min_len):
    if min_len <= 0:
        return segments
    out = []
    for a, b in segments:
        if (b - a).magnitude >= min_len:
            out.append((a, b))
    return out


def quantize(value, eps):
    if eps <= 0:
        return value
    return int(round(value / eps))


def dedup_segments(segments, eps):
    if not segments:
        return []
    seen = set()
    out = []
    for a, b in segments:
        if (b - a).magnitude <= eps:
            continue
        ax = quantize(a.x, eps)
        ay = quantize(a.y, eps)
        bx = quantize(b.x, eps)
        by = quantize(b.y, eps)
        if (ax, ay, bx, by) > (bx, by, ax, ay):
            ax, ay, bx, by = bx, by, ax, ay
        key = (ax, ay, bx, by)
        if key in seen:
            continue
        seen.add(key)
        out.append((a, b))
    return out


def build_chains(segments, eps):
    if not segments:
        return []
    adj = {}
    keys = []
    for i, (a, b) in enumerate(segments):
        ka = (quantize(a.x, eps), quantize(a.y, eps))
        kb = (quantize(b.x, eps), quantize(b.y, eps))
        keys.append((ka, kb))
        adj.setdefault(ka, []).append((i, 0))
        adj.setdefault(kb, []).append((i, 1))

    visited = [False] * len(segments)
    chains = []

    def key_of(p):
        return (quantize(p.x, eps), quantize(p.y, eps))

    for i, (a, b) in enumerate(segments):
        if visited[i]:
            continue
        ka, kb = keys[i]
        if len(adj.get(ka, [])) != 2:
            start_key = ka
        elif len(adj.get(kb, [])) != 2:
            start_key = kb
        else:
            start_key = ka

        if start_key == kb:
            a, b = b, a
            ka, kb = kb, ka

        chain = [a, b]
        visited[i] = True
        current_key = kb

        while True:
            next_items = [item for item in adj.get(current_key, []) if not visited[item[0]]]
            if not next_items:
                break
            next_seg, _ = next_items[0]
            na, nb = segments[next_seg]
            nka, nkb = keys[next_seg]
            if current_key == nkb:
                na, nb = nb, na
                nka, nkb = nkb, nka
            chain.append(nb)
            visited[next_seg] = True
            current_key = nkb

        closed = key_of(chain[0]) == key_of(chain[-1])
        chains.append((chain, closed))

    return chains


def collapse_points(points, eps):
    if not points:
        return points
    out = [points[0]]
    for p in points[1:]:
        if (p - out[-1]).magnitude > eps:
            out.append(p)
    return out


def merge_collinear(segments, angle_tol_deg, dist_tol):
    if not segments:
        return []
    angle_tol = math.radians(max(angle_tol_deg, 1e-6))
    groups = {}
    for p0, p1 in segments:
        dx = p1.x - p0.x
        dy = p1.y - p0.y
        length = math.hypot(dx, dy)
        if length <= dist_tol:
            continue
        angle = math.atan2(dy, dx)
        if angle < 0:
            angle += math.pi
        if angle >= math.pi:
            angle -= math.pi
        angle_bin = int(round(angle / angle_tol))

        ux, uy = dx / length, dy / length
        nx, ny = -uy, ux
        d = nx * p0.x + ny * p0.y
        offset_bin = int(round(d / dist_tol)) if dist_tol > 0 else 0

        groups.setdefault((angle_bin, offset_bin), []).append((p0, p1, ux, uy, d))

    merged = []
    for _, segs in groups.items():
        ux, uy, d = segs[0][2], segs[0][3], segs[0][4]
        intervals = []
        for p0, p1, _, _, _ in segs:
            t0 = ux * p0.x + uy * p0.y
            t1 = ux * p1.x + uy * p1.y
            if t1 < t0:
                t0, t1 = t1, t0
            intervals.append((t0, t1))
        intervals.sort()

        cur_s, cur_e = intervals[0]
        for s, e in intervals[1:]:
            if s <= cur_e + dist_tol:
                cur_e = max(cur_e, e)
            else:
                merged.append((cur_s, cur_e, ux, uy, d))
                cur_s, cur_e = s, e
        merged.append((cur_s, cur_e, ux, uy, d))

    out = []
    for t0, t1, ux, uy, d in merged:
        nx, ny = -uy, ux
        p0 = Vec3(ux * t0 + nx * d, uy * t0 + ny * d, 0.0)
        p1 = Vec3(ux * t1 + nx * d, uy * t1 + ny * d, 0.0)
        out.append((p0, p1))
    return out


def copy_layers(src, dst):
    for layer in src.layers:
        name = layer.dxf.name
        color = layer.dxf.color
        if not dst.layers.has_entry(name):
            dst.layers.add(name, dxfattribs={"color": color, "linetype": "CONTINUOUS"})


def main():
    import argparse

    ap = argparse.ArgumentParser()
    ap.add_argument("--config", required=True)
    ap.add_argument("--input", required=True)
    ap.add_argument("--output", required=True)
    args = ap.parse_args()

    cfg = read_config(args.config)
    warn_if_tolerance(cfg)

    src = ezdxf.readfile(args.input)
    entities = recursive_decompose(src.modelspace())

    seg_by_key = {}
    for e in entities:
        try:
            path = make_path(e)
        except TypeError:
            continue

        layer = e.dxf.get("layer", "0")
        color = resolve_color(e, src)

        paths = path.sub_paths() if getattr(path, "has_sub_paths", False) else [path]
        for sp in paths:
            segs = path_to_segments(sp, cfg["chordal"], cfg["max_seg"], cfg["overkill"])
            if not segs:
                continue
            seg_by_key.setdefault((layer, color), []).extend(segs)

    out = ezdxf.new("R12")
    copy_layers(src, out)
    msp = out.modelspace()

    for (layer, color), segs in seg_by_key.items():
        snapped = [(snap_point(a, cfg["overkill"]), snap_point(b, cfg["overkill"])) for a, b in segs]
        snapped = filter_degenerate(snapped, cfg["overkill"])
        snapped = dedup_segments(snapped, cfg["overkill"])
        merged = merge_collinear(snapped, cfg["ang"], cfg["overkill"])
        merged = dedup_segments(merged, cfg["overkill"])
        merged = filter_short_segments(merged, cfg["min_seg"])
        chains = build_chains(merged, cfg["overkill"])

        for points, closed in chains:
            points = collapse_points(points, cfg["overkill"])
            if closed and len(points) > 2 and (points[0] - points[-1]).magnitude <= cfg["overkill"]:
                points = points[:-1]
            if len(points) < 2:
                continue
            pl = msp.add_polyline2d(
                [(p.x, p.y) for p in points],
                dxfattribs={"layer": layer, "color": color, "linetype": "CONTINUOUS"},
                close=closed,
            )
            pl.dxf.elevation = 0.0

    out.saveas(args.output)


if __name__ == "__main__":
    main()
```

## 验证要点
- ENTITIES 中不出现 INSERT/BLOCK/SPLINE。
- 所有 POLYLINE 顶点 Z=0。
- 线型为 CONTINUOUS。
- 图层/颜色保留。
- 不存在重叠线段（同向同线的区间已合并）。

## 已知限制
- 对极端复杂实体（如 ACIS/特定代理对象）可能被忽略。
- 曲线离散会产生近似误差，误差由 ChordalTolerance 控制。
- R12 对非 ASCII 图层名兼容性较差，必要时需做图层名映射。
