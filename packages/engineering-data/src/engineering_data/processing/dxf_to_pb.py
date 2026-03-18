import argparse
from collections import Counter
import math
from pathlib import Path
import sys

import ezdxf

from engineering_data.proto import dxf_primitives_pb2 as pb

MIN_BEZIER_SEGMENTS = 4
R12_DXF_VERSION = "AC1009"
R12_CORE_ENTITY_TYPES = {"LINE", "ARC", "CIRCLE", "POLYLINE", "POINT"}
FALLBACK_ENTITY_TYPES = {"LWPOLYLINE", "SPLINE", "ELLIPSE"}


def insunits_to_scale(insunits: int) -> float:
    return {
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
    }.get(insunits, 1.0)


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


def build_contour(points, bulges, closed):
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


def spline_points_from_geomdl(entity, max_step, samples):
    try:
        from geomdl import BSpline
        from geomdl import NURBS

        ctrlpts = list(entity.control_points)
        if not ctrlpts:
            ctrlpts = list(entity.fit_points)
        if not ctrlpts:
            return []

        degree = int(getattr(entity.dxf, "degree", 3) or 3)
        weights = list(getattr(entity, "weights", []))
        knots = list(getattr(entity, "knots", []))

        if weights:
            curve = NURBS.Curve()
            curve.weights = list(weights)
        else:
            curve = BSpline.Curve()
        curve.degree = degree
        curve.ctrlpts = [(float(p[0]), float(p[1])) for p in ctrlpts]
        if knots:
            curve.knotvector = list(knots)

        if max_step > 0:
            approx_len = 0.0
            for i in range(1, len(curve.ctrlpts)):
                approx_len += distance(curve.ctrlpts[i - 1], curve.ctrlpts[i])
            sample_size = max(2, int(math.ceil(approx_len / max_step)) + 1)
        else:
            sample_size = max(2, int(samples))

        curve.sample_size = sample_size
        return [(float(p[0]), float(p[1])) for p in curve.evalpts]
    except Exception:
        return []


def spline_points_from_ezdxf(entity, chordal, max_seg):
    try:
        from ezdxf.path import make_path
    except Exception:
        return []

    try:
        path = make_path(entity)
        pts = [(float(p.x), float(p.y)) for p in path.flattening(chordal, segments=MIN_BEZIER_SEGMENTS)]
        if max_seg > 0:
            pts = densify(pts, max_seg)
        return pts
    except Exception:
        return []


def approximate_spline_points(entity, chordal, max_seg, max_step, samples):
    pts = spline_points_from_geomdl(entity, max_step, samples)
    if pts:
        return pts
    return spline_points_from_ezdxf(entity, chordal, max_seg)


def process_r12_polyline(entity, entity_id, bundle, scale, snap_eps, layer, color):
    points = []
    bulges = []
    vertices_attr = getattr(entity, "vertices", [])
    vertices = list(vertices_attr() if callable(vertices_attr) else vertices_attr)
    for v in vertices:
        loc = v.dxf.location
        points.append(transform_point((float(loc.x), float(loc.y)), scale, snap_eps))
        bulges.append(float(v.dxf.get("bulge", 0.0)))
    closed = bool(entity.is_closed)
    contour = build_contour(points, bulges, closed)
    add_contour(bundle, contour,
                (entity_id, pb.DXF_ENTITY_LWPOLYLINE, 0, closed, layer, color))
    return True


def process_r12_entity(entity, doc, entity_id, bundle, scale, opts):
    etype = entity.dxftype()
    layer = entity.dxf.get("layer", "0")
    color = resolve_color(entity, doc)
    snap_eps = opts["snap"] if opts["snap_enabled"] else 0.0

    if etype == "LINE":
        start = transform_point((entity.dxf.start.x, entity.dxf.start.y), scale, snap_eps)
        end = transform_point((entity.dxf.end.x, entity.dxf.end.y), scale, snap_eps)
        add_line(bundle, start, end,
                 (entity_id, pb.DXF_ENTITY_LINE, 0, False, layer, color))
        return True

    if etype == "ARC":
        center = transform_point((entity.dxf.center.x, entity.dxf.center.y), scale, snap_eps)
        radius = float(entity.dxf.radius) * scale
        start_angle = float(entity.dxf.start_angle)
        end_angle = float(entity.dxf.end_angle)
        add_arc(bundle, center, radius, start_angle, end_angle, False,
                (entity_id, pb.DXF_ENTITY_ARC, 0, False, layer, color))
        return True

    if etype == "CIRCLE":
        center = transform_point((entity.dxf.center.x, entity.dxf.center.y), scale, snap_eps)
        radius = float(entity.dxf.radius) * scale
        add_circle(bundle, center, radius,
                   (entity_id, pb.DXF_ENTITY_CIRCLE, 0, True, layer, color))
        return True

    if etype == "POLYLINE":
        return process_r12_polyline(entity, entity_id, bundle, scale, snap_eps, layer, color)

    if etype == "POINT":
        pos = transform_point((entity.dxf.location.x, entity.dxf.location.y), scale, snap_eps)
        add_point(bundle, pos,
                  (entity_id, pb.DXF_ENTITY_POINT, 0, False, layer, color))
        return True

    return False


def process_fallback_entity(entity, doc, entity_id, bundle, scale, opts):
    etype = entity.dxftype()
    layer = entity.dxf.get("layer", "0")
    color = resolve_color(entity, doc)
    snap_eps = opts["snap"] if opts["snap_enabled"] else 0.0

    if etype == "LWPOLYLINE":
        points = []
        bulges = []
        closed = bool(getattr(entity, "closed", False))
        for x, y, b in entity.get_points("xyb"):
            points.append(transform_point((float(x), float(y)), scale, snap_eps))
            bulges.append(float(b))
        contour = build_contour(points, bulges, closed)
        add_contour(bundle, contour,
                    (entity_id, pb.DXF_ENTITY_LWPOLYLINE, 0, closed, layer, color))
        return True

    if etype == "SPLINE":
        closed = bool(getattr(entity, "closed", False))
        if opts["approx_splines"]:
            max_seg = opts["max_seg"] if opts["densify_enabled"] else 0.0
            min_seg = opts["min_seg"] if opts["min_seg_enabled"] else 0.0
            points = approximate_spline_points(entity,
                                                opts["chordal"],
                                                max_seg,
                                                opts["spline_max_step"],
                                                opts["spline_samples"])
            points = [transform_point(p, scale, snap_eps) for p in points]
            if max_seg > 0:
                points = densify(points, max_seg)
            if min_seg > 0:
                points = collapse_points(points, min_seg)
            if points:
                contour = build_polyline_contour(points, closed)
                add_contour(bundle, contour,
                            (entity_id, pb.DXF_ENTITY_SPLINE, 0, closed, layer, color))
                return True
        control_points = []
        for p in entity.control_points:
            control_points.append(transform_point((float(p[0]), float(p[1])), scale, snap_eps))
        if not control_points:
            for p in entity.fit_points:
                control_points.append(transform_point((float(p[0]), float(p[1])), scale, snap_eps))
        if control_points:
            add_spline(bundle, control_points, closed,
                       (entity_id, pb.DXF_ENTITY_SPLINE, 0, closed, layer, color))
            return True
        return False

    if etype == "ELLIPSE":
        center = transform_point((entity.dxf.center.x, entity.dxf.center.y), scale, snap_eps)
        major_axis = transform_point((entity.dxf.major_axis.x, entity.dxf.major_axis.y), scale, snap_eps)
        ratio = float(entity.dxf.ratio)
        start_param = float(entity.dxf.start_param)
        end_param = float(entity.dxf.end_param)
        add_ellipse(bundle, center, major_axis, ratio, start_param, end_param,
                    (entity_id, pb.DXF_ENTITY_ELLIPSE, 0, False, layer, color))
        return True

    return False


def process_entity(entity, doc, entity_id, bundle, scale, opts):
    if process_r12_entity(entity, doc, entity_id, bundle, scale, opts):
        return True
    return process_fallback_entity(entity, doc, entity_id, bundle, scale, opts)


def classify_entity_type(entity) -> str:
    etype = entity.dxftype()
    if etype in R12_CORE_ENTITY_TYPES:
        return "core"
    if etype in FALLBACK_ENTITY_TYPES:
        return "fallback"
    return "unsupported"

def format_entity_counts(counts):
    if not counts:
        return "none"
    return ", ".join(f"{etype}={counts[etype]}" for etype in sorted(counts))


def is_r12_document(doc) -> bool:
    return str(getattr(doc, "dxfversion", "")).upper() == R12_DXF_VERSION


def validate_input_contract(doc, entities, strict_r12):
    warnings = []
    errors = []
    dxf_version = str(getattr(doc, "dxfversion", "")).upper()
    fallback_counts = Counter()
    unsupported_counts = Counter()

    if not is_r12_document(doc):
        message = f"Expected DXF R12 ({R12_DXF_VERSION}), got {dxf_version or 'UNKNOWN'}."
        if strict_r12:
            errors.append(message)
        else:
            warnings.append(f"{message} Proceeding because --no-strict-r12 was set.")

    for entity in entities:
        etype = entity.dxftype()
        category = classify_entity_type(entity)
        if category == "fallback":
            fallback_counts[etype] += 1
        elif category == "unsupported":
            unsupported_counts[etype] += 1

    if fallback_counts:
        message = (
            "Non-R12 compatibility handlers will be used for: "
            f"{format_entity_counts(fallback_counts)}."
        )
        if strict_r12:
            errors.append(message)
        else:
            warnings.append(message)

    if unsupported_counts:
        warnings.append(
            "Unsupported DXF entities will be skipped: "
            f"{format_entity_counts(unsupported_counts)}."
        )

    return errors, warnings


def build_bundle(doc, source_path, schema_version, tolerances, normalize_units):
    bundle = pb.PathBundle()
    header = bundle.header
    header.schema_version = int(schema_version)
    header.source_path = str(source_path)
    insunits = int(doc.header.get("$INSUNITS", 0) or 0)
    unit_scale = float(insunits_to_scale(insunits))
    header.unit_scale = unit_scale
    header.units = "mm"
    header.tolerance.chordal = float(tolerances["chordal"])
    header.tolerance.max_seg = float(tolerances["max_seg"])
    header.tolerance.snap = float(tolerances["snap"])
    header.tolerance.angular = float(tolerances["angular"])
    header.tolerance.min_seg = float(tolerances["min_seg"])
    return bundle, (unit_scale if normalize_units else 1.0)


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
    parser.add_argument("--approx-splines", action=argparse.BooleanOptionalAction, default=False)
    parser.add_argument("--snap-enabled", action=argparse.BooleanOptionalAction, default=False)
    parser.add_argument("--densify-enabled", action=argparse.BooleanOptionalAction, default=False)
    parser.add_argument("--min-seg-enabled", action=argparse.BooleanOptionalAction, default=False)
    parser.add_argument("--spline-samples", type=int, default=64)
    parser.add_argument("--spline-max-step", type=float, default=0.0)
    parser.add_argument("--strict-r12", action=argparse.BooleanOptionalAction, default=True,
                        help="Reject non-R12 DXF versions and compatibility-only entities")
    return parser.parse_args(argv)


def main(argv=None):
    args = parse_args(sys.argv[1:] if argv is None else argv)
    input_path = Path(args.input)
    output_path = Path(args.output)
    if not input_path.exists():
        print(f"Error: DXF not found: {input_path}", file=sys.stderr)
        return 1

    try:
        doc = ezdxf.readfile(str(input_path))
    except Exception as exc:
        print(f"Error: failed to parse DXF: {input_path}: {exc}", file=sys.stderr)
        return 2

    entities = list(doc.modelspace())
    errors, warnings = validate_input_contract(doc, entities, args.strict_r12)
    if errors:
        for message in errors:
            print(f"Error: {message}", file=sys.stderr)
        return 4
    for message in warnings:
        print(f"Warning: {message}", file=sys.stderr)

    tolerances = {
        "chordal": args.chordal,
        "max_seg": args.max_seg,
        "snap": args.snap,
        "angular": args.angular,
        "min_seg": args.min_seg,
    }

    opts = {
        "chordal": args.chordal,
        "max_seg": args.max_seg,
        "snap": args.snap,
        "angular": args.angular,
        "min_seg": args.min_seg,
        "approx_splines": args.approx_splines,
        "snap_enabled": args.snap_enabled,
        "densify_enabled": args.densify_enabled,
        "min_seg_enabled": args.min_seg_enabled,
        "spline_samples": args.spline_samples,
        "spline_max_step": args.spline_max_step,
    }

    bundle, scale = build_bundle(doc, input_path, args.schema_version, tolerances, args.normalize_units)

    entity_id = 0
    for entity in entities:
        if process_entity(entity, doc, entity_id, bundle, scale, opts):
            entity_id += 1

    if not bundle.primitives:
        print(f"Error: no supported primitives exported from DXF: {input_path}", file=sys.stderr)
        return 3

    output_path.parent.mkdir(parents=True, exist_ok=True)
    with output_path.open("wb") as f:
        f.write(bundle.SerializeToString())

    print(f"Exported {len(bundle.primitives)} primitives -> {output_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
