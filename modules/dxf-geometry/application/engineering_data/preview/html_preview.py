from __future__ import annotations

# pyright: reportPrivateImportUsage=false

import hashlib
import html
import math
from dataclasses import dataclass
from pathlib import Path
from uuid import uuid4

import ezdxf
from ezdxf.path import make_path

from engineering_data.contracts.preview import PreviewArtifact, PreviewRequest


_MIN_TOLERANCE_MM = 0.05
_DEFAULT_PADDING_MM = 5.0


@dataclass(frozen=True)
class _Polyline:
    points: list[tuple[float, float]]
    layer: str
    closed: bool = False


def _polyline_length(points: list[tuple[float, float]]) -> float:
    total = 0.0
    for index in range(1, len(points)):
        start = points[index - 1]
        end = points[index]
        total += math.hypot(end[0] - start[0], end[1] - start[1])
    return total


def _unique_points(points: list[tuple[float, float]]) -> list[tuple[float, float]]:
    result: list[tuple[float, float]] = []
    for point in points:
        current = (float(point[0]), float(point[1]))
        if result and math.isclose(result[-1][0], current[0], abs_tol=1e-9) and math.isclose(
            result[-1][1], current[1], abs_tol=1e-9
        ):
            continue
        result.append(current)
    return result


def _flatten_entity(entity, tolerance_mm: float) -> list[tuple[float, float]]:
    entity_type = entity.dxftype()
    if entity_type == "POINT":
        location = entity.dxf.location
        return [(float(location.x), float(location.y))]

    try:
        path = make_path(entity)
    except Exception:
        return []

    try:
        vertices = list(path.flattening(distance=max(tolerance_mm, _MIN_TOLERANCE_MM)))
    except Exception:
        return []

    points = [(float(vertex.x), float(vertex.y)) for vertex in vertices]
    return _unique_points(points)


def _is_closed_entity(entity, points: list[tuple[float, float]]) -> bool:
    if len(points) < 2:
        return False
    if entity.dxftype() in {"CIRCLE", "ELLIPSE"}:
        return True
    closed_flag = getattr(entity, "closed", None)
    if callable(closed_flag):
        try:
            return bool(closed_flag())
        except Exception:
            pass
    if closed_flag is not None:
        return bool(closed_flag)
    return math.isclose(points[0][0], points[-1][0], abs_tol=1e-6) and math.isclose(
        points[0][1], points[-1][1], abs_tol=1e-6
    )


def _collect_polylines(input_path: Path, tolerance_mm: float) -> list[_Polyline]:
    document = ezdxf.readfile(str(input_path))
    polylines: list[_Polyline] = []
    for entity in document.modelspace():
        points = _flatten_entity(entity, tolerance_mm)
        if not points:
            continue
        layer_name = getattr(entity.dxf, "layer", "0") or "0"
        closed = _is_closed_entity(entity, points)
        polylines.append(_Polyline(points=points, layer=layer_name, closed=closed))
    return polylines


def _downsample_polylines(polylines: list[_Polyline], max_points: int) -> list[_Polyline]:
    if max_points <= 0:
        return polylines

    total_points = sum(len(polyline.points) for polyline in polylines)
    if total_points <= max_points:
        return polylines

    stride = max(2, math.ceil(total_points / max_points))
    reduced: list[_Polyline] = []
    for polyline in polylines:
        if len(polyline.points) <= 2:
            reduced.append(polyline)
            continue
        sampled = polyline.points[::stride]
        if sampled[-1] != polyline.points[-1]:
            sampled.append(polyline.points[-1])
        if polyline.closed and sampled[0] != sampled[-1]:
            sampled.append(sampled[0])
        reduced.append(_Polyline(points=_unique_points(sampled), layer=polyline.layer, closed=polyline.closed))
    return reduced


def _compute_bounds(polylines: list[_Polyline]) -> tuple[float, float, float, float]:
    xs = [point[0] for polyline in polylines for point in polyline.points]
    ys = [point[1] for polyline in polylines for point in polyline.points]
    if not xs or not ys:
        raise ValueError("DXF 中没有可用于预览的几何实体")
    return min(xs), min(ys), max(xs), max(ys)


def _transform_point(point: tuple[float, float], min_x: float, min_y: float, max_y: float, padding: float) -> tuple[float, float]:
    x = (point[0] - min_x) + padding
    y = (max_y - point[1]) + padding
    return x, y


def _layer_color(layer_name: str) -> str:
    palette = (
        "#0f766e",
        "#0ea5e9",
        "#d97706",
        "#dc2626",
        "#7c3aed",
        "#16a34a",
        "#db2777",
    )
    digest = hashlib.sha1(layer_name.encode("utf-8")).digest()
    index = digest[0] % len(palette)
    return palette[index]


def _svg_markup(
    polylines: list[_Polyline],
    title: str,
    total_length_mm: float,
    estimated_time_s: float,
    bounds: tuple[float, float, float, float],
) -> str:
    min_x, min_y, max_x, max_y = bounds
    width = max(max_x - min_x, 1.0)
    height = max(max_y - min_y, 1.0)
    padding = max(_DEFAULT_PADDING_MM, max(width, height) * 0.04)
    svg_width = width + padding * 2.0
    svg_height = height + padding * 2.0

    paths: list[str] = []
    points_markup: list[str] = []
    for polyline in polylines:
        transformed = [
            _transform_point(point, min_x=min_x, min_y=min_y, max_y=max_y, padding=padding)
            for point in polyline.points
        ]
        if len(transformed) == 1:
            x, y = transformed[0]
            points_markup.append(
                f"<circle cx=\"{x:.3f}\" cy=\"{y:.3f}\" r=\"1.8\" fill=\"{_layer_color(polyline.layer)}\" />"
            )
            continue
        path_d = " ".join(
            [f"M {transformed[0][0]:.3f} {transformed[0][1]:.3f}"]
            + [f"L {x:.3f} {y:.3f}" for x, y in transformed[1:]]
        )
        paths.append(
            f"<path d=\"{path_d}\" class=\"trace\" style=\"--trace-color:{_layer_color(polyline.layer)}\" />"
        )

    stats_length = f"{total_length_mm:.2f} mm"
    stats_time = f"{estimated_time_s:.2f} s" if estimated_time_s > 0.0 else "-"
    return f"""<!DOCTYPE html>
<html lang="zh-CN">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>{html.escape(title)}</title>
  <style>
    :root {{
      color-scheme: light;
      --bg: #f6f3ea;
      --panel: rgba(255, 255, 255, 0.88);
      --ink: #14213d;
      --muted: #5b6475;
      --grid: rgba(20, 33, 61, 0.08);
      --frame: rgba(20, 33, 61, 0.16);
    }}
    * {{
      box-sizing: border-box;
    }}
    body {{
      margin: 0;
      min-height: 100vh;
      font-family: "Segoe UI", "Microsoft YaHei UI", sans-serif;
      color: var(--ink);
      background:
        radial-gradient(circle at top left, rgba(14, 165, 233, 0.18), transparent 28%),
        radial-gradient(circle at bottom right, rgba(217, 119, 6, 0.16), transparent 24%),
        linear-gradient(135deg, #fcfbf7 0%, var(--bg) 100%);
    }}
    main {{
      display: grid;
      grid-template-columns: minmax(220px, 300px) minmax(0, 1fr);
      gap: 16px;
      padding: 18px;
    }}
    .panel {{
      background: var(--panel);
      border: 1px solid rgba(255, 255, 255, 0.72);
      border-radius: 18px;
      box-shadow: 0 16px 40px rgba(15, 23, 42, 0.12);
      backdrop-filter: blur(12px);
    }}
    .summary {{
      padding: 20px;
    }}
    h1 {{
      margin: 0 0 10px;
      font-size: 1.35rem;
      line-height: 1.3;
    }}
    .meta {{
      display: grid;
      gap: 12px;
      margin-top: 18px;
    }}
    .card {{
      padding: 12px 14px;
      border-radius: 14px;
      background: rgba(255, 255, 255, 0.78);
      border: 1px solid rgba(20, 33, 61, 0.08);
    }}
    .label {{
      display: block;
      margin-bottom: 6px;
      font-size: 0.78rem;
      letter-spacing: 0.08em;
      text-transform: uppercase;
      color: var(--muted);
    }}
    .value {{
      font-size: 1.12rem;
      font-weight: 600;
    }}
    .canvas {{
      position: relative;
      overflow: hidden;
      min-height: 78vh;
      padding: 10px;
    }}
    svg {{
      width: 100%;
      height: 100%;
      min-height: 76vh;
      border-radius: 16px;
      background:
        linear-gradient(var(--grid) 1px, transparent 1px),
        linear-gradient(90deg, var(--grid) 1px, transparent 1px),
        #fffdf7;
      background-size: 24px 24px;
      border: 1px solid var(--frame);
    }}
    .trace {{
      fill: none;
      stroke: var(--trace-color, #0f766e);
      stroke-width: 1.2;
      stroke-linecap: round;
      stroke-linejoin: round;
      vector-effect: non-scaling-stroke;
    }}
    .footer {{
      margin-top: 16px;
      font-size: 0.88rem;
      color: var(--muted);
      line-height: 1.5;
    }}
    @media (max-width: 960px) {{
      main {{
        grid-template-columns: 1fr;
      }}
      .canvas {{
        min-height: 60vh;
      }}
      svg {{
        min-height: 58vh;
      }}
    }}
  </style>
</head>
<body>
  <main>
    <section class="panel summary">
      <h1>{html.escape(title)}</h1>
      <div class="meta">
        <div class="card">
          <span class="label">总长度</span>
          <span class="value">{stats_length}</span>
        </div>
        <div class="card">
          <span class="label">预估时间</span>
          <span class="value">{stats_time}</span>
        </div>
        <div class="card">
          <span class="label">包围盒</span>
          <span class="value">{width:.2f} × {height:.2f} mm</span>
        </div>
        <div class="card">
          <span class="label">几何对象</span>
          <span class="value">{len(polylines)}</span>
        </div>
      </div>
      <div class="footer">
        该预览由 engineering-data 本地生成，展示责任由 hmi-client 承接。
      </div>
    </section>
    <section class="panel canvas">
      <svg viewBox="0 0 {svg_width:.3f} {svg_height:.3f}" role="img" aria-label="{html.escape(title)}">
        <g>
          {''.join(paths)}
          {''.join(points_markup)}
        </g>
      </svg>
    </section>
  </main>
</body>
</html>
"""


def generate_preview(request: PreviewRequest) -> PreviewArtifact:
    if not request.input_path.exists():
        raise FileNotFoundError(f"DXF 文件不存在: {request.input_path}")

    polylines = _collect_polylines(request.input_path, request.curve_tolerance_mm)
    if not polylines:
        raise ValueError(f"DXF 中没有可用于预览的几何实体: {request.input_path}")

    reduced_polylines = _downsample_polylines(polylines, request.max_points)
    bounds = _compute_bounds(reduced_polylines)
    total_length = sum(_polyline_length(polyline.points) for polyline in reduced_polylines)
    estimated_time = total_length / request.speed_mm_s if request.speed_mm_s > 0 else 0.0

    request.output_dir.mkdir(parents=True, exist_ok=True)
    if request.deterministic_name:
        preview_name = f"{request.input_path.stem}-preview.html"
    else:
        preview_name = f"{request.input_path.stem}-{uuid4().hex[:8]}-preview.html"
    preview_path = request.output_dir / preview_name
    preview_path.write_text(
        _svg_markup(
            polylines=reduced_polylines,
            title=request.title,
            total_length_mm=total_length,
            estimated_time_s=estimated_time,
            bounds=bounds,
        ),
        encoding="utf-8",
    )

    min_x, min_y, max_x, max_y = bounds
    return PreviewArtifact(
        preview_path=str(preview_path),
        entity_count=len(reduced_polylines),
        segment_count=sum(max(0, len(polyline.points) - 1) for polyline in reduced_polylines),
        point_count=sum(len(polyline.points) for polyline in reduced_polylines),
        total_length_mm=total_length,
        estimated_time_s=estimated_time,
        width_mm=max_x - min_x,
        height_mm=max_y - min_y,
    )
