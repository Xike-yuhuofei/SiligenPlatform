from __future__ import annotations

from dataclasses import asdict, dataclass, field
from hashlib import sha256
from pathlib import Path
from typing import Any


SCHEMA_VERSION = "DXFValidationReport.v1"
SPEC_VERSION = "DXFInputSpec.v1"
POLICY_VERSION = "DXFValidationPolicy.v1"
TOLERANCE_POLICY_VERSION = "GeometryTolerancePolicy.v1"
LAYER_SEMANTIC_POLICY_VERSION = "LayerSemanticPolicy.v1"

ERROR_CODE_MAP = {
    "DXF_E_FILE_NOT_FOUND": "DXF_FILE_NOT_FOUND",
    "DXF_E_FILE_OPEN_FAILED": "DXF_HEADER_UNREADABLE",
    "DXF_E_INVALID_SIGNATURE": "DXF_HEADER_UNREADABLE",
    "DXF_E_CORRUPTED_FILE": "DXF_SECTION_UNREADABLE",
    "DXF_E_UNSUPPORTED_VERSION": "DXF_VERSION_UNSUPPORTED",
    "DXF_E_UNIT_REQUIRED_FOR_PRODUCTION": "DXF_UNIT_MISSING",
    "DXF_E_UNIT_UNSUPPORTED": "DXF_UNIT_NOT_MM",
    "DXF_E_BLOCK_INSERT_NOT_EXPLODED": "DXF_BLOCK_INSERT_NOT_EXPLODED",
    "DXF_E_BLOCK_DEPTH_EXCEEDED": "DXF_BLOCK_INSERT_NOT_EXPLODED",
    "DXF_E_BLOCK_EXPAND_FAILED": "DXF_BLOCK_INSERT_NOT_EXPLODED",
    "DXF_E_HATCH_NOT_SUPPORTED": "DXF_HATCH_NOT_SUPPORTED",
    "DXF_E_SPLINE_NOT_SUPPORTED": "DXF_SPLINE_NOT_SUPPORTED",
    "DXF_E_ELLIPSE_NOT_SUPPORTED": "DXF_ELLIPSE_NOT_SUPPORTED",
    "DXF_E_3D_ENTITY_FOUND": "DXF_3D_ENTITY_DETECTED",
    "DXF_E_UNSUPPORTED_ENTITY": "DXF_ENTITY_NOT_SUPPORTED",
    "DXF_E_NO_VALID_ENTITY": "PROCESS_NO_GLUE_GEOMETRY",
    "DXF_E_ZERO_LENGTH_SEGMENT": "GEOM_ZERO_LENGTH_ENTITY",
    "DXF_E_INVALID_ARC_PARAM": "GEOM_INVALID_ARC_RADIUS",
    "DXF_W_TEXT_IGNORED": "DXF_NON_EXEC_ENTITY_PRESENT",
    "DXF_W_DIMENSION_IGNORED": "DXF_NON_EXEC_ENTITY_PRESENT",
    "DXF_W_HIGH_VERSION_DOWNGRADED": "DXF_VERSION_NON_BASELINE",
}


@dataclass(frozen=True)
class ValidationDiagnostic:
    error_code: str
    severity: str
    message: str
    operator_message: str
    stage_id: str
    owner_module: str
    layer: str = ""
    entity_id: str = ""
    entity_type: str = ""
    source_handle: str = ""
    location_mm: dict[str, float] | None = None
    related_entities: list[str] = field(default_factory=list)
    suggested_fix: str = ""
    is_blocking: bool = True


def stable_error_code(code: str) -> str:
    if code in ERROR_CODE_MAP:
        return ERROR_CODE_MAP[code]
    if code.startswith("DXF_W_"):
        return "DXF_NON_EXEC_ENTITY_PRESENT"
    if code.startswith("DXF_E_"):
        return "DXF_INPUT_INVALID"
    return code


def diagnostic_stage_owner(code: str) -> tuple[str, str]:
    stable_code = stable_error_code(code)
    if stable_code in {"DXF_FILE_NOT_FOUND", "DXF_FILE_EMPTY", "DXF_HEADER_UNREADABLE", "DXF_SECTION_UNREADABLE"}:
        return "S1", "M1"
    if stable_code.startswith("GEOM_"):
        return "S3", "M3"
    if stable_code.startswith("TOPOLOGY_"):
        return "S3", "M3"
    if stable_code.startswith("FEATURE_"):
        return "S4", "M3"
    if stable_code.startswith("PROCESS_"):
        return "S4", "M3"
    return "S2", "M2"


def _file_hash(path: Path) -> str:
    if not path.exists() or not path.is_file():
        return ""
    digest = sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _operator_message(code: str, message: str) -> str:
    stable_code = stable_error_code(code)
    if stable_code == "DXF_UNIT_MISSING":
        return "禁止执行：DXF 未声明单位。请在 CAD 中明确设置单位为 mm 后重新导入。"
    if stable_code == "DXF_UNIT_NOT_MM":
        return "禁止执行：DXF 单位不是 mm 或单位不受支持。请转换为 mm 后重新导入。"
    if stable_code == "DXF_BLOCK_INSERT_NOT_EXPLODED":
        return "禁止执行：DXF 包含未展开的 BLOCK/INSERT。请在 CAD 或受控预处理工具中展开后重新导入。"
    if stable_code == "DXF_SPLINE_NOT_SUPPORTED":
        return "禁止执行：DXF 包含 SPLINE。请先转换为受控 LINE/ARC/POLYLINE 几何并重新验证。"
    if stable_code == "DXF_ELLIPSE_NOT_SUPPORTED":
        return "禁止执行：DXF 包含 ELLIPSE。请先转换为受控 LINE/ARC/POLYLINE 几何并重新验证。"
    if stable_code == "DXF_HATCH_NOT_SUPPORTED":
        return "禁止执行：DXF 包含 HATCH。请移除或转换为受控轮廓后重新导入。"
    if stable_code == "DXF_3D_ENTITY_DETECTED":
        return "禁止执行：DXF 包含 3D entity 或非 XY 平面几何。请转换为 2D mm 图形后重新导入。"
    if stable_code.startswith("GEOM_"):
        return f"禁止执行：DXF 几何质量不满足输入规范。{message}"
    if stable_code.startswith("PROCESS_"):
        return f"禁止执行：DXF 未形成可点胶输入。{message}"
    if stable_code == "DXF_NON_EXEC_ENTITY_PRESENT":
        return f"注意：DXF 包含非执行对象。{message}"
    return f"禁止执行：DXF 输入不满足规范。{message}"


def build_diagnostic(code: str, message: str, *, warning: bool = False) -> ValidationDiagnostic:
    stable_code = stable_error_code(code)
    stage_id, owner_module = diagnostic_stage_owner(code)
    return ValidationDiagnostic(
        error_code=stable_code,
        severity="WARNING" if warning else "ERROR",
        message=message,
        operator_message=_operator_message(code, message),
        stage_id=stage_id,
        owner_module=owner_module,
        suggested_fix="返回 CAD 修复源文件后重新导入，或走受控修复提案并重新验证。",
        is_blocking=not warning,
    )


def build_validation_report(
    *,
    source_path: Path,
    dxf_version: str,
    unit: str,
    entity_count: int,
    layer_count: int,
    errors: list[tuple[str, str]],
    warnings: list[tuple[str, str]],
) -> dict[str, Any]:
    error_items = [build_diagnostic(code, message, warning=False) for code, message in errors]
    warning_items = [build_diagnostic(code, message, warning=True) for code, message in warnings]
    gate_result = "FAIL" if error_items else ("PASS_WITH_WARNINGS" if warning_items else "PASS")
    stage_id = "S1" if any(item.stage_id == "S1" for item in error_items) else "S2"
    owner_module = "M1" if stage_id == "S1" else "M2"
    report = {
        "schema_version": SCHEMA_VERSION,
        "file": {
            "file_name": source_path.name,
            "file_hash": _file_hash(source_path),
            "file_size_bytes": source_path.stat().st_size if source_path.exists() and source_path.is_file() else 0,
            "dxf_version": dxf_version,
            "unit": unit,
            "source_drawing_ref": f"sha256:{_file_hash(source_path)}" if source_path.exists() and source_path.is_file() else None,
        },
        "policy": {
            "spec_version": SPEC_VERSION,
            "policy_version": POLICY_VERSION,
            "tolerance_policy_version": TOLERANCE_POLICY_VERSION,
            "layer_semantic_policy_version": LAYER_SEMANTIC_POLICY_VERSION,
        },
        "stage": {
            "stage_id": stage_id,
            "owner_module": owner_module,
        },
        "summary": {
            "gate_result": gate_result,
            "error_count": len(error_items),
            "warning_count": len(warning_items),
            "entity_count": entity_count,
            "layer_count": layer_count,
            "glue_contour_count": 0,
            "bounds_mm": {"min_x": 0.0, "min_y": 0.0, "max_x": 0.0, "max_y": 0.0},
        },
        "entity_summary": [],
        "layer_summary": [],
        "geometry_summary": {},
        "topology_summary": {},
        "errors": [asdict(item) for item in error_items],
        "warnings": [asdict(item) for item in warning_items],
        "recommended_actions": [
            "修复 DXF 源文件后重新导入。",
            "任何修复必须重新运行 DXF 输入治理验证。",
        ] if gate_result == "FAIL" else [],
    }
    return report
