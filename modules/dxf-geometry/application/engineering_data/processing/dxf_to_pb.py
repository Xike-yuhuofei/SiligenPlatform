# pyright: reportPrivateImportUsage=false

from __future__ import annotations

import argparse
from collections import Counter
from dataclasses import dataclass, field
import json
import math
from pathlib import Path
import sys
from typing import Any, Iterable, cast

import ezdxf

from engineering_data.contracts.dxf_validation_report import build_validation_report
from engineering_data.proto import dxf_primitives_pb2 as pb

MIN_BEZIER_SEGMENTS = 4
STANDARD_DXF_VERSION = "AC1015"
STANDARD_DXF_VERSION_LABEL = "R2000"
MAX_INSERT_DEPTH = 8

READABLE_DXF_VERSIONS = {
    "AC1009": "R12",
    "AC1015": "R2000",
    "AC1018": "R2004",
    "AC1021": "R2007",
    "AC1024": "R2010",
    "AC1027": "R2013",
    "AC1032": "R2018",
}

STANDARD_CORE_ENTITY_TYPES = {"LINE", "ARC", "CIRCLE", "LWPOLYLINE", "POLYLINE", "POINT"}
STANDARD_HIGH_ORDER_ENTITY_TYPES = {"SPLINE", "ELLIPSE"}
IGNORED_ENTITY_TYPES = {"TEXT", "MTEXT", "DIMENSION", "LEADER", "MLEADER"}
REJECTED_ENTITY_TYPES = {
    "XLINE",
    "RAY",
    "IMAGE",
    "PDFUNDERLAY",
    "HATCH",
    "3DFACE",
    "REGION",
    "BODY",
    "SURFACE",
    "MESH",
    "PROXY",
}

INSUNITS_TO_SCALE = {
    1: 25.4,
    2: 304.8,
    3: 1609344.0,
    4: 1.0,
    5: 10.0,
    6: 1000.0,
    7: 1000000.0,
    8: 25.4e-6,
    9: 0.0254,
    10: 914.4,
    11: 1.0e-7,
    12: 1.0e-6,
    13: 1.0e-3,
    14: 100.0,
    15: 10000.0,
    16: 100000.0,
    17: 1.0e12,
    18: 1.495978707e14,
    19: 9.4607e18,
    20: 3.0857e19,
}

INSUNITS_TO_NAME = {
    1: "inch",
    2: "foot",
    3: "mile",
    4: "mm",
    5: "cm",
    6: "m",
    7: "km",
    8: "microinch",
    9: "mil",
    10: "yard",
    11: "angstrom",
    12: "nanometer",
    13: "micron",
    14: "decimeter",
    15: "decameter",
    16: "hectometer",
    17: "gigameter",
    18: "astronomical_unit",
    19: "light_year",
    20: "parsec",
}

FATAL_IMPORT_ERROR_CODES = {
    "DXF_E_FILE_NOT_FOUND",
    "DXF_E_FILE_OPEN_FAILED",
    "DXF_E_INVALID_SIGNATURE",
    "DXF_E_CORRUPTED_FILE",
    "DXF_E_UNSUPPORTED_VERSION",
    "DXF_E_UNIT_UNSUPPORTED",
    "DXF_E_NO_VALID_ENTITY",
}


@dataclass
class ImportIssue:
    code: str
    message: str


@dataclass
class ImportContext:
    warnings: list[ImportIssue] = field(default_factory=list)
    errors: list[ImportIssue] = field(default_factory=list)
    ignored_entity_types: Counter[str] = field(default_factory=Counter)
    rejected_entity_types: Counter[str] = field(default_factory=Counter)
    resolved_units: str = "mm"
    resolved_unit_scale: float = 1.0

    def add_warning(self, code: str, message: str) -> None:
        if any(issue.code == code and issue.message == message for issue in self.warnings):
            return
        self.warnings.append(ImportIssue(code=code, message=message))

    def add_error(self, code: str, message: str) -> None:
        if any(issue.code == code and issue.message == message for issue in self.errors):
            return
        self.errors.append(ImportIssue(code=code, message=message))


def resolve_color(entity, doc) -> int:
    color = entity.dxf.get("color", 256)
    layer_name = entity.dxf.get("layer", "0")
    if color in (0, 256):
        if doc.layers.has_entry(layer_name):
            return int(doc.layers.get(layer_name).dxf.color)
        return 7
    return int(color)


def to_point2d(x, y):
    return pb.Point2D(x=float(x), y=float(y))


def distance(a, b) -> float:
    return math.hypot(b[0] - a[0], b[1] - a[1])


def snap_value(value, eps):
    if eps <= 0:
        return value
    return round(value / eps) * eps


def snap_point(point, eps):
    if eps <= 0:
        return point
    return (snap_value(point[0], eps), snap_value(point[1], eps))


def apply_scale(point, scale):
    return (point[0] * scale, point[1] * scale)


def transform_point(point, scale, snap_eps):
    return snap_point(apply_scale(point, scale), snap_eps)


def densify(points, max_seg):
    if len(points) < 2 or max_seg <= 0:
        return points[:]
    out = [points[0]]
    for i in range(1, len(points)):
        a = out[-1]
        b = points[i]
        dist = distance(a, b)
        if dist <= max_seg:
            out.append(b)
            continue
        n = int(math.ceil(dist / max_seg))
        for k in range(1, n + 1):
            out.append((a[0] + (b[0] - a[0]) * (k / n), a[1] + (b[1] - a[1]) * (k / n)))
    return out


def collapse_points(points, min_seg):
    if not points:
        return []
    if min_seg <= 0:
        return points
    out = [points[0]]
    for p in points[1:]:
        if distance(p, out[-1]) > min_seg:
            out.append(p)
    return out


def bulge_to_arc(start, end, bulge):
    if abs(bulge) <= 1e-9:
        return None
    if abs(bulge) > 1000.0:
        return None
    dx = end[0] - start[0]
    dy = end[1] - start[1]
    chord = math.hypot(dx, dy)
    if chord <= 1e-9:
        return None
    theta = 4.0 * math.atan(bulge)
    theta_abs = abs(theta)
    if theta_abs <= 1e-9:
        return None
    sin_half = math.sin(theta_abs / 2.0)
    if abs(sin_half) <= 1e-9:
        return None
    radius = chord / (2.0 * sin_half)
    tan_half = math.tan(theta_abs / 2.0)
    offset = 0.0 if abs(tan_half) <= 1e-9 else (chord / (2.0 * tan_half))

    ux = -dy / chord
    uy = dx / chord
    sign = 1.0 if bulge >= 0.0 else -1.0
    mid = ((start[0] + end[0]) * 0.5, (start[1] + end[1]) * 0.5)
    center = (mid[0] + ux * offset * sign, mid[1] + uy * offset * sign)

    start_angle = math.degrees(math.atan2(start[1] - center[1], start[0] - center[0]))
    end_angle = math.degrees(math.atan2(end[1] - center[1], end[0] - center[0]))
    clockwise = bulge < 0.0
    return center, radius, start_angle, end_angle, clockwise


def build_polyline_contour(points, closed):
    contour = pb.ContourPrimitive()
    count = len(points)
    if count < 2:
        return contour
    seg_count = count if closed else count - 1
    for i in range(seg_count):
        j = (i + 1) % count
        start = points[i]
        end = points[j]
        elem = pb.ContourElement()
        elem.type = pb.CONTOUR_LINE
        elem.line.start.CopyFrom(to_point2d(start[0], start[1]))
        elem.line.end.CopyFrom(to_point2d(end[0], end[1]))
        contour.elements.append(elem)
    contour.closed = bool(closed)
    return contour


def build_contour(points, bulges, closed, context: ImportContext):
    contour = pb.ContourPrimitive()
    count = len(points)
    if count < 2:
        return contour
    seg_count = count if closed else count - 1
    for i in range(seg_count):
        j = (i + 1) % count
        start = points[i]
        end = points[j]
        bulge = bulges[i] if i < len(bulges) else 0.0
        elem = pb.ContourElement()
        if abs(bulge) <= 1e-9:
            elem.type = pb.CONTOUR_LINE
            elem.line.start.CopyFrom(to_point2d(start[0], start[1]))
            elem.line.end.CopyFrom(to_point2d(end[0], end[1]))
        else:
            arc = bulge_to_arc(start, end, bulge)
            if arc is None:
                context.add_error(
                    "DXF_E_INVALID_ARC_PARAM",
                    "Polyline bulge arc parameters are invalid; import stayed preview-only.",
                )
                elem.type = pb.CONTOUR_LINE
                elem.line.start.CopyFrom(to_point2d(start[0], start[1]))
                elem.line.end.CopyFrom(to_point2d(end[0], end[1]))
            else:
                center, radius, start_deg, end_deg, clockwise = arc
                elem.type = pb.CONTOUR_ARC
                elem.arc.center.CopyFrom(to_point2d(center[0], center[1]))
                elem.arc.radius = float(radius)
                elem.arc.start_angle_deg = float(start_deg)
                elem.arc.end_angle_deg = float(end_deg)
                elem.arc.clockwise = bool(clockwise)
        contour.elements.append(elem)
    contour.closed = bool(closed)
    return contour


def add_meta(bundle, entity_id, entity_type, segment_index, closed, layer, color):
    meta = bundle.metadata.add()
    meta.entity_id = int(entity_id)
    meta.entity_type = int(entity_type)
    meta.entity_segment_index = int(segment_index)
    meta.entity_closed = bool(closed)
    meta.layer = str(layer)
    meta.color = int(color)


def add_line(bundle, start, end, meta_args):
    prim = bundle.primitives.add()
    prim.type = pb.PRIMITIVE_LINE
    prim.line.start.CopyFrom(to_point2d(start[0], start[1]))
    prim.line.end.CopyFrom(to_point2d(end[0], end[1]))
    add_meta(bundle, *meta_args)


def add_arc(bundle, center, radius, start_deg, end_deg, clockwise, meta_args):
    prim = bundle.primitives.add()
    prim.type = pb.PRIMITIVE_ARC
    prim.arc.center.CopyFrom(to_point2d(center[0], center[1]))
    prim.arc.radius = float(radius)
    prim.arc.start_angle_deg = float(start_deg)
    prim.arc.end_angle_deg = float(end_deg)
    prim.arc.clockwise = bool(clockwise)
    add_meta(bundle, *meta_args)


def add_circle(bundle, center, radius, meta_args):
    prim = bundle.primitives.add()
    prim.type = pb.PRIMITIVE_CIRCLE
    prim.circle.center.CopyFrom(to_point2d(center[0], center[1]))
    prim.circle.radius = float(radius)
    prim.circle.start_angle_deg = 0.0
    prim.circle.clockwise = False
    add_meta(bundle, *meta_args)


def add_spline(bundle, control_points, closed, meta_args):
    prim = bundle.primitives.add()
    prim.type = pb.PRIMITIVE_SPLINE
    for p in control_points:
        prim.spline.control_points.append(to_point2d(p[0], p[1]))
    prim.spline.closed = bool(closed)
    add_meta(bundle, *meta_args)


def add_ellipse(bundle, center, major_axis, ratio, start_param, end_param, meta_args):
    prim = bundle.primitives.add()
    prim.type = pb.PRIMITIVE_ELLIPSE
    prim.ellipse.center.CopyFrom(to_point2d(center[0], center[1]))
    prim.ellipse.major_axis.CopyFrom(to_point2d(major_axis[0], major_axis[1]))
    prim.ellipse.ratio = float(ratio)
    prim.ellipse.start_param = float(start_param)
    prim.ellipse.end_param = float(end_param)
    prim.ellipse.clockwise = False
    add_meta(bundle, *meta_args)


def add_point(bundle, position, meta_args):
    prim = bundle.primitives.add()
    prim.type = pb.PRIMITIVE_POINT
    prim.point.position.CopyFrom(to_point2d(position[0], position[1]))
    add_meta(bundle, *meta_args)


def add_contour(bundle, contour, meta_args):
    prim = bundle.primitives.add()
    prim.type = pb.PRIMITIVE_CONTOUR
    prim.contour.CopyFrom(contour)
    add_meta(bundle, *meta_args)


def process_polyline_entity(entity, entity_id, bundle, scale, snap_eps, layer, color, context: ImportContext):
    points = []
    bulges = []
    vertices_attr = getattr(entity, "vertices", [])
    vertices_value = vertices_attr() if callable(vertices_attr) else vertices_attr
    vertices = list(cast(Iterable[object], vertices_value))
    for v in vertices:
        vertex = cast(Any, v)
        loc = vertex.dxf.location
        points.append(transform_point((float(loc.x), float(loc.y)), scale, snap_eps))
        bulges.append(float(vertex.dxf.get("bulge", 0.0)))
    closed = bool(entity.is_closed)
    contour = build_contour(points, bulges, closed, context)
    if len(contour.elements) == 0:
        return False
    add_contour(bundle, contour, (entity_id, pb.DXF_ENTITY_LWPOLYLINE, 0, closed, layer, color))
    return True


def process_lwpolyline_entity(entity, entity_id, bundle, scale, snap_eps, layer, color, context: ImportContext):
    points = []
    bulges = []
    closed = bool(getattr(entity, "closed", False))
    for x, y, b in entity.get_points("xyb"):
        points.append(transform_point((float(x), float(y)), scale, snap_eps))
        bulges.append(float(b))
    contour = build_contour(points, bulges, closed, context)
    if len(contour.elements) == 0:
        return False
    add_contour(bundle, contour, (entity_id, pb.DXF_ENTITY_LWPOLYLINE, 0, closed, layer, color))
    return True


def process_core_entity(entity, doc, entity_id, bundle, scale, opts, context: ImportContext):
    etype = entity.dxftype()
    layer = entity.dxf.get("layer", "0")
    color = resolve_color(entity, doc)
    snap_eps = opts["snap"] if opts["snap_enabled"] else 0.0

    if etype == "LINE":
        start = transform_point((entity.dxf.start.x, entity.dxf.start.y), scale, snap_eps)
        end = transform_point((entity.dxf.end.x, entity.dxf.end.y), scale, snap_eps)
        if distance(start, end) <= 1e-9:
            context.add_error("DXF_E_ZERO_LENGTH_SEGMENT", "Detected zero-length LINE entity.")
            return False
        add_line(bundle, start, end, (entity_id, pb.DXF_ENTITY_LINE, 0, False, layer, color))
        return True

    if etype == "ARC":
        center = transform_point((entity.dxf.center.x, entity.dxf.center.y), scale, snap_eps)
        radius = float(entity.dxf.radius) * scale
        if radius <= 1e-9:
            context.add_error("DXF_E_INVALID_ARC_PARAM", "Detected ARC with non-positive radius.")
            return False
        start_angle = float(entity.dxf.start_angle)
        end_angle = float(entity.dxf.end_angle)
        add_arc(bundle, center, radius, start_angle, end_angle, False, (entity_id, pb.DXF_ENTITY_ARC, 0, False, layer, color))
        return True

    if etype == "CIRCLE":
        center = transform_point((entity.dxf.center.x, entity.dxf.center.y), scale, snap_eps)
        radius = float(entity.dxf.radius) * scale
        if radius <= 1e-9:
            context.add_error("DXF_E_INVALID_ARC_PARAM", "Detected CIRCLE with non-positive radius.")
            return False
        add_circle(bundle, center, radius, (entity_id, pb.DXF_ENTITY_CIRCLE, 0, True, layer, color))
        return True

    if etype == "LWPOLYLINE":
        return process_lwpolyline_entity(entity, entity_id, bundle, scale, snap_eps, layer, color, context)

    if etype == "POLYLINE":
        return process_polyline_entity(entity, entity_id, bundle, scale, snap_eps, layer, color, context)

    if etype == "POINT":
        pos = transform_point((entity.dxf.location.x, entity.dxf.location.y), scale, snap_eps)
        add_point(bundle, pos, (entity_id, pb.DXF_ENTITY_POINT, 0, False, layer, color))
        return True

    return False


def process_entity(entity, doc, entity_id, bundle, scale, opts, context: ImportContext):
    return process_core_entity(entity, doc, entity_id, bundle, scale, opts, context)


def classify_entity_type(entity) -> str:
    etype = entity.dxftype()
    if etype in STANDARD_CORE_ENTITY_TYPES:
        return "core"
    if etype in STANDARD_HIGH_ORDER_ENTITY_TYPES:
        return "high_order"
    return "unsupported"


def is_higher_than_standard(version: str) -> bool:
    readable = list(READABLE_DXF_VERSIONS.keys())
    if version not in readable or STANDARD_DXF_VERSION not in readable:
        return False
    return readable.index(version) > readable.index(STANDARD_DXF_VERSION)


def resolve_unit_scale(doc, normalize_units: bool, context: ImportContext) -> float | None:
    insunits = int(doc.header.get("$INSUNITS", 0) or 0)
    if insunits == 0:
        context.resolved_units = "unspecified"
        context.resolved_unit_scale = 0.0
        context.add_error("DXF_E_UNIT_REQUIRED_FOR_PRODUCTION", "DXF unit missing; production import requires explicit unit.")
        return None

    if insunits not in INSUNITS_TO_SCALE:
        context.resolved_units = f"insunits_{insunits}"
        context.resolved_unit_scale = 0.0
        context.add_error("DXF_E_UNIT_UNSUPPORTED", f"Unsupported DXF unit code: {insunits}.")
        return None

    context.resolved_units = INSUNITS_TO_NAME.get(insunits, "mm")
    context.resolved_unit_scale = float(INSUNITS_TO_SCALE[insunits])
    return context.resolved_unit_scale if normalize_units else 1.0


def expand_entities(entity, context: ImportContext, depth: int = 0):
    etype = entity.dxftype()
    if etype == "INSERT":
        context.rejected_entity_types[etype] += 1
        context.add_error("DXF_E_BLOCK_INSERT_NOT_EXPLODED", "DXF contains INSERT/BLOCK content; input governance rejects unexpanded blocks.")
        return []

    if etype in IGNORED_ENTITY_TYPES:
        context.ignored_entity_types[etype] += 1
        warning_code = "DXF_W_TEXT_IGNORED" if etype in {"TEXT", "MTEXT"} else "DXF_W_DIMENSION_IGNORED"
        context.add_warning(warning_code, f"Ignored non-production DXF entity type: {etype}.")
        return []

    if etype in REJECTED_ENTITY_TYPES:
        context.rejected_entity_types[etype] += 1
        if etype in {"3DFACE", "BODY", "SURFACE", "MESH"}:
            specific_code = "DXF_E_3D_ENTITY_FOUND"
        elif etype == "HATCH":
            specific_code = "DXF_E_HATCH_NOT_SUPPORTED"
        else:
            specific_code = "DXF_E_UNSUPPORTED_ENTITY"
        context.add_error(specific_code, f"Rejected DXF entity type: {etype}.")
        return []

    if etype == "SPLINE":
        context.rejected_entity_types[etype] += 1
        context.add_error("DXF_E_SPLINE_NOT_SUPPORTED", "SPLINE is not supported by DXF input governance v1.")
        return []

    if etype == "ELLIPSE":
        context.rejected_entity_types[etype] += 1
        context.add_error("DXF_E_ELLIPSE_NOT_SUPPORTED", "ELLIPSE is not supported by DXF input governance v1.")
        return []

    if classify_entity_type(entity) == "unsupported":
        context.rejected_entity_types[etype] += 1
        context.add_error("DXF_E_UNSUPPORTED_ENTITY", f"Unsupported DXF entity type: {etype}.")
        return []

    return [entity]


def build_bundle(doc, source_path, schema_version, tolerances, context: ImportContext):
    bundle = pb.PathBundle()
    header = bundle.header
    header.schema_version = int(schema_version)
    header.source_path = str(source_path)
    header.unit_scale = float(context.resolved_unit_scale)
    header.units = str(context.resolved_units)
    header.tolerance.chordal = float(tolerances["chordal"])
    header.tolerance.max_seg = float(tolerances["max_seg"])
    header.tolerance.snap = float(tolerances["snap"])
    header.tolerance.angular = float(tolerances["angular"])
    header.tolerance.min_seg = float(tolerances["min_seg"])
    return bundle


def finalize_import_diagnostics(bundle: pb.PathBundle, context: ImportContext):
    if not bundle.primitives and not any(issue.code == "DXF_E_NO_VALID_ENTITY" for issue in context.errors):
        context.add_error("DXF_E_NO_VALID_ENTITY", "No valid production geometry could be imported from the DXF.")

    warning_codes = [issue.code for issue in context.warnings]
    error_codes = [issue.code for issue in context.errors]
    preview_ready = bool(bundle.primitives)

    if error_codes:
        classification = pb.IMPORT_RESULT_FAILED
    elif warning_codes:
        classification = pb.IMPORT_RESULT_SUCCESS_WITH_WARNINGS
    else:
        classification = pb.IMPORT_RESULT_SUCCESS

    production_ready = classification in {pb.IMPORT_RESULT_SUCCESS, pb.IMPORT_RESULT_SUCCESS_WITH_WARNINGS}
    primary_code = error_codes[0] if error_codes else (warning_codes[0] if warning_codes else "")

    if classification == pb.IMPORT_RESULT_SUCCESS:
        summary = "DXF import succeeded and is ready for production."
    elif classification == pb.IMPORT_RESULT_SUCCESS_WITH_WARNINGS:
        summary = context.warnings[0].message
    elif classification == pb.IMPORT_RESULT_PREVIEW_ONLY:
        summary = context.errors[0].message if context.errors else "DXF import is preview-only."
    else:
        summary = context.errors[0].message if context.errors else "DXF import failed."

    diagnostics = bundle.import_diagnostics
    diagnostics.classification = classification
    diagnostics.preview_ready = preview_ready
    diagnostics.production_ready = production_ready
    diagnostics.summary = summary
    diagnostics.primary_code = primary_code
    diagnostics.warning_codes.extend(warning_codes)
    diagnostics.error_codes.extend(error_codes)
    diagnostics.warning_messages.extend(issue.message for issue in context.warnings)
    diagnostics.error_messages.extend(issue.message for issue in context.errors)
    diagnostics.ignored_entity_types.extend(sorted(context.ignored_entity_types))
    diagnostics.rejected_entity_types.extend(sorted(context.rejected_entity_types))
    diagnostics.resolved_units = context.resolved_units
    diagnostics.resolved_unit_scale = float(context.resolved_unit_scale)
    return diagnostics


def emit_diagnostics_to_stderr(diagnostics: pb.ImportDiagnostics):
    for code, message in zip(diagnostics.warning_codes, diagnostics.warning_messages):
        print(f"Warning[{code}]: {message}", file=sys.stderr)
    for code, message in zip(diagnostics.error_codes, diagnostics.error_messages):
        print(f"Error[{code}]: {message}", file=sys.stderr)


def parse_args(argv):
    parser = argparse.ArgumentParser(description="DXF -> Protobuf (.pb) export")
    parser.add_argument("--input", required=True, help="Input DXF file path")
    parser.add_argument("--output", required=True, help="Output .pb file path")
    parser.add_argument("--schema-version", type=int, default=1)
    parser.add_argument("--chordal", type=float, default=0.005)
    parser.add_argument("--max-seg", type=float, default=0.0)
    parser.add_argument("--snap", type=float, default=0.0)
    parser.add_argument("--angular", type=float, default=0.0)
    parser.add_argument("--min-seg", type=float, default=0.0)
    parser.add_argument("--normalize-units", action=argparse.BooleanOptionalAction, default=True)
    parser.add_argument("--snap-enabled", action=argparse.BooleanOptionalAction, default=False)
    parser.add_argument("--densify-enabled", action=argparse.BooleanOptionalAction, default=False)
    parser.add_argument("--min-seg-enabled", action=argparse.BooleanOptionalAction, default=False)
    parser.add_argument("--validation-report", help="Optional DXFValidationReport.v1 JSON output path")
    return parser.parse_args(argv)


def write_validation_report(path: str | None, source_path: Path, doc, context: ImportContext, *, dxf_version: str = "") -> None:
    if not path:
        return
    modelspace_count = 0
    layer_count = 0
    if doc is not None:
        try:
            modelspace_count = len(list(doc.modelspace()))
        except Exception:
            modelspace_count = 0
        try:
            layer_count = len(list(doc.layers))
        except Exception:
            layer_count = 0
    report = build_validation_report(
        source_path=source_path,
        dxf_version=dxf_version,
        unit=context.resolved_units,
        entity_count=modelspace_count,
        layer_count=layer_count,
        errors=[(issue.code, issue.message) for issue in context.errors],
        warnings=[(issue.code, issue.message) for issue in context.warnings],
    )
    report_path = Path(path)
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")


def main(argv=None):
    args = parse_args(sys.argv[1:] if argv is None else argv)
    input_path = Path(args.input)
    output_path = Path(args.output)
    if not input_path.exists():
        context = ImportContext()
        context.add_error("DXF_E_FILE_NOT_FOUND", f"DXF not found: {input_path}")
        write_validation_report(args.validation_report, input_path, None, context)
        print(f"Error[DXF_E_FILE_NOT_FOUND]: DXF not found: {input_path}", file=sys.stderr)
        return 1

    try:
        doc = ezdxf.readfile(str(input_path))
    except Exception as exc:
        context = ImportContext()
        context.add_error("DXF_E_FILE_OPEN_FAILED", f"failed to parse DXF: {input_path}: {exc}")
        write_validation_report(args.validation_report, input_path, None, context)
        print(f"Error[DXF_E_FILE_OPEN_FAILED]: failed to parse DXF: {input_path}: {exc}", file=sys.stderr)
        return 2

    context = ImportContext()
    dxf_version = str(getattr(doc, "dxfversion", "")).upper()
    if dxf_version not in READABLE_DXF_VERSIONS:
        context.add_error(
            "DXF_E_UNSUPPORTED_VERSION",
            f"Unsupported DXF version: {dxf_version or 'UNKNOWN'}. Supported versions: {', '.join(sorted(READABLE_DXF_VERSIONS.values()))}.",
        )
    elif is_higher_than_standard(dxf_version):
        context.add_warning(
            "DXF_W_HIGH_VERSION_DOWNGRADED",
            f"Accepted non-baseline DXF version {READABLE_DXF_VERSIONS[dxf_version]} ({dxf_version}); semantics were normalized to the R2000 owner baseline.",
        )

    unit_scale = resolve_unit_scale(doc, args.normalize_units, context)

    tolerances = {
        "chordal": args.chordal,
        "max_seg": args.max_seg,
        "snap": args.snap,
        "angular": args.angular,
        "min_seg": args.min_seg,
    }

    bundle = build_bundle(doc, input_path, args.schema_version, tolerances, context)
    opts = {
        "chordal": args.chordal,
        "max_seg": args.max_seg,
        "snap": args.snap,
        "angular": args.angular,
        "min_seg": args.min_seg,
        "snap_enabled": args.snap_enabled,
        "densify_enabled": args.densify_enabled,
        "min_seg_enabled": args.min_seg_enabled,
    }

    if unit_scale is not None and not any(issue.code in FATAL_IMPORT_ERROR_CODES for issue in context.errors):
        expanded_entities = []
        for entity in doc.modelspace():
            expanded_entities.extend(expand_entities(entity, context))

        entity_id = 0
        for entity in expanded_entities:
            if process_entity(entity, doc, entity_id, bundle, unit_scale, opts, context):
                entity_id += 1

    diagnostics = finalize_import_diagnostics(bundle, context)
    write_validation_report(args.validation_report, input_path, doc, context, dxf_version=dxf_version)
    emit_diagnostics_to_stderr(diagnostics)

    if diagnostics.classification == pb.IMPORT_RESULT_FAILED:
        return 4

    output_path.parent.mkdir(parents=True, exist_ok=True)
    with output_path.open("wb") as f:
        f.write(bundle.SerializeToString())

    classification_name = pb.ImportResultClassification.Name(diagnostics.classification)
    print(f"Exported {len(bundle.primitives)} primitives -> {output_path} [{classification_name}]")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
