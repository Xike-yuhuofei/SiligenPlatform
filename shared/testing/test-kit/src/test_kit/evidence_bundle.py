from __future__ import annotations

import json
from dataclasses import asdict, dataclass, field
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


EVIDENCE_BUNDLE_SCHEMA_VERSION = "validation-evidence-bundle.v2"
REPORT_MANIFEST_SCHEMA_VERSION = "validation-report-manifest.v1"
REPORT_INDEX_SCHEMA_VERSION = "validation-report-index.v1"
COMPATIBLE_BUNDLE_SCHEMA_VERSIONS = (EVIDENCE_BUNDLE_SCHEMA_VERSION,)
REQUIRED_FAILURE_CLASSIFICATION_FIELDS = ("category", "code", "blocking")
REQUIRED_TRACE_FIELDS = (
    "stage_id",
    "artifact_id",
    "module_id",
    "workflow_state",
    "execution_state",
    "event_name",
    "failure_code",
    "evidence_path",
)


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat()


def _ensure_parent(path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)


def trace_fields(
    *,
    stage_id: str,
    artifact_id: str,
    module_id: str,
    workflow_state: str,
    execution_state: str,
    event_name: str,
    evidence_path: str,
    failure_code: str = "",
) -> dict[str, str]:
    payload = {
        "stage_id": stage_id,
        "artifact_id": artifact_id,
        "module_id": module_id,
        "workflow_state": workflow_state,
        "execution_state": execution_state,
        "event_name": event_name,
        "failure_code": failure_code,
        "evidence_path": evidence_path,
    }
    missing = [key for key in REQUIRED_TRACE_FIELDS if key not in payload]
    if missing:
        raise ValueError(f"missing trace fields: {missing}")
    return payload


def default_deterministic_replay(*, passed: bool = False, seed: int = 0, clock_profile: str = "", repeat_count: int = 0) -> dict[str, object]:
    return {
        "passed": passed,
        "seed": seed,
        "clock_profile": clock_profile,
        "repeat_count": repeat_count,
    }


def default_failure_classification(
    *,
    status: str,
    failure_code: str = "",
    note: str = "",
) -> dict[str, object]:
    normalized_status = status.strip() or "incomplete"
    fallback_message = note.strip() or failure_code.strip() or f"validation status={normalized_status}"
    mapping = {
        "passed": {"category": "validation", "code": "passed", "blocking": False},
        "failed": {"category": "validation", "code": "assertion_failed", "blocking": True},
        "known_failure": {"category": "runtime", "code": "known_failure", "blocking": True},
        "blocked": {"category": "validation", "code": "blocked", "blocking": True},
        "skipped": {"category": "validation", "code": "skipped", "blocking": False},
        "deferred": {"category": "validation", "code": "deferred", "blocking": False},
        "incomplete": {"category": "validation", "code": "incomplete", "blocking": True},
    }
    resolved = dict(mapping.get(normalized_status, mapping["incomplete"]))
    resolved["message"] = fallback_message
    return resolved


def _normalize_failure_classification(
    *,
    status: str,
    trace_failure_code: str,
    note: str,
    classification: dict[str, object] | None,
) -> dict[str, object]:
    resolved = dict(default_failure_classification(status=status, failure_code=trace_failure_code, note=note))
    if classification:
        resolved.update(classification)
    missing = [field_name for field_name in REQUIRED_FAILURE_CLASSIFICATION_FIELDS if field_name not in resolved]
    if missing:
        raise ValueError(f"failure classification missing fields: {missing}")
    resolved["category"] = str(resolved["category"])
    resolved["code"] = str(resolved["code"])
    resolved["blocking"] = bool(resolved["blocking"])
    if "message" in resolved:
        resolved["message"] = str(resolved["message"])
    else:
        resolved["message"] = default_failure_classification(
            status=status,
            failure_code=trace_failure_code,
            note=note,
        )["message"]
    return resolved


def validate_bundle_compatibility(
    payload: dict[str, Any],
    *,
    expected_schema_version: str = EVIDENCE_BUNDLE_SCHEMA_VERSION,
) -> list[str]:
    errors: list[str] = []
    schema_version = str(payload.get("schema_version", ""))
    if schema_version != expected_schema_version:
        errors.append(f"bundle schema_version mismatch: expected={expected_schema_version} actual={schema_version or 'missing'}")

    manifest_file = str(payload.get("report_manifest_file", "")).strip()
    if not manifest_file:
        errors.append("bundle report_manifest_file missing")
    else:
        manifest_path = Path(manifest_file)
        if not manifest_path.exists():
            errors.append(f"bundle report_manifest_file missing on disk: {manifest_path}")
        else:
            manifest_payload = json.loads(manifest_path.read_text(encoding="utf-8"))
            manifest_schema_version = str(manifest_payload.get("schema_version", ""))
            if manifest_schema_version != REPORT_MANIFEST_SCHEMA_VERSION:
                errors.append(
                    "report manifest schema_version mismatch: "
                    f"expected={REPORT_MANIFEST_SCHEMA_VERSION} actual={manifest_schema_version or 'missing'}"
                )
            if str(manifest_payload.get("bundle_schema_version", "")) != schema_version:
                errors.append("report manifest bundle_schema_version does not match bundle schema_version")

    index_file = str(payload.get("report_index_file", "")).strip()
    if not index_file:
        errors.append("bundle report_index_file missing")
    else:
        index_path = Path(index_file)
        if not index_path.exists():
            errors.append(f"bundle report_index_file missing on disk: {index_path}")
        else:
            index_payload = json.loads(index_path.read_text(encoding="utf-8"))
            index_schema_version = str(index_payload.get("schema_version", ""))
            if index_schema_version != REPORT_INDEX_SCHEMA_VERSION:
                errors.append(
                    "report index schema_version mismatch: "
                    f"expected={REPORT_INDEX_SCHEMA_VERSION} actual={index_schema_version or 'missing'}"
                )
            if str(index_payload.get("bundle_schema_version", "")) != schema_version:
                errors.append("report index bundle_schema_version does not match bundle schema_version")
    return errors


def bundle_is_compatible(
    payload: dict[str, Any],
    *,
    expected_schema_version: str = EVIDENCE_BUNDLE_SCHEMA_VERSION,
) -> bool:
    return not validate_bundle_compatibility(payload, expected_schema_version=expected_schema_version)


@dataclass(frozen=True)
class EvidenceLink:
    label: str
    path: str
    role: str

    def to_dict(self) -> dict[str, str]:
        return asdict(self)


@dataclass(frozen=True)
class EvidenceCaseRecord:
    case_id: str
    name: str
    suite_ref: str
    owner_scope: str
    primary_layer: str
    producer_lane_ref: str
    status: str
    evidence_profile: str
    stability_state: str
    trace_fields: dict[str, str]
    size_label: str = ""
    label_refs: tuple[str, ...] = ()
    risk_tags: tuple[str, ...] = ()
    required_assets: tuple[str, ...] = ()
    required_fixtures: tuple[str, ...] = ()
    fault_ids: tuple[str, ...] = ()
    deterministic_replay: dict[str, object] = field(default_factory=default_deterministic_replay)
    failure_classification: dict[str, object] = field(default_factory=dict)
    note: str = ""

    def to_dict(self) -> dict[str, object]:
        record = asdict(self)
        for field_name in REQUIRED_TRACE_FIELDS:
            if field_name not in self.trace_fields:
                raise ValueError(f"case record {self.case_id} missing trace field: {field_name}")
        if "passed" not in self.deterministic_replay:
            raise ValueError(f"case record {self.case_id} missing deterministic_replay.passed")
        record["failure_classification"] = _normalize_failure_classification(
            status=self.status,
            trace_failure_code=str(self.trace_fields.get("failure_code", "")),
            note=self.note,
            classification=self.failure_classification,
        )
        return record


@dataclass
class EvidenceBundle:
    bundle_id: str
    request_ref: str
    producer_lane_ref: str
    report_root: str
    summary_file: str
    machine_file: str
    verdict: str
    case_records: list[EvidenceCaseRecord] = field(default_factory=list)
    linked_asset_refs: tuple[str, ...] = ()
    generated_at: str = field(default_factory=utc_now)
    schema_version: str = EVIDENCE_BUNDLE_SCHEMA_VERSION
    required_trace_fields: tuple[str, ...] = REQUIRED_TRACE_FIELDS
    skipped_layer_refs: tuple[str, ...] = ()
    skip_justification: str = ""
    offline_prerequisites: tuple[str, ...] = ()
    abort_metadata: dict[str, object] = field(default_factory=dict)
    metadata: dict[str, object] = field(default_factory=dict)

    def to_dict(self) -> dict[str, object]:
        return {
            "schema_version": self.schema_version,
            "bundle_id": self.bundle_id,
            "request_ref": self.request_ref,
            "producer_lane_ref": self.producer_lane_ref,
            "report_root": self.report_root,
            "summary_file": self.summary_file,
            "machine_file": self.machine_file,
            "verdict": self.verdict,
            "generated_at": self.generated_at,
            "required_trace_fields": list(self.required_trace_fields),
            "linked_asset_refs": list(self.linked_asset_refs),
            "skipped_layer_refs": list(self.skipped_layer_refs),
            "skip_justification": self.skip_justification,
            "offline_prerequisites": list(self.offline_prerequisites),
            "abort_metadata": self.abort_metadata,
            "metadata": self.metadata,
            "case_refs": [record.case_id for record in self.case_records],
            "case_records": [record.to_dict() for record in self.case_records],
        }


def infer_verdict(statuses: list[str]) -> str:
    if not statuses:
        return "incomplete"
    if "failed" in statuses:
        return "failed"
    if "blocked" in statuses:
        return "blocked"
    if "known_failure" in statuses:
        return "known-failure"
    if "incomplete" in statuses:
        return "incomplete"
    if "deferred" in statuses:
        return "deferred"
    if statuses and all(status == "skipped" for status in statuses):
        return "skipped"
    if "skipped" in statuses:
        return "deferred"
    if all(status == "passed" for status in statuses):
        return "passed"
    return "incomplete"


def write_bundle_artifacts(
    *,
    bundle: EvidenceBundle,
    report_root: Path,
    summary_json_path: Path,
    summary_md_path: Path,
    evidence_links: list[EvidenceLink],
) -> dict[str, str]:
    resolved_root = report_root.resolve()
    resolved_root.mkdir(parents=True, exist_ok=True)
    case_index_path = resolved_root / "case-index.json"
    links_path = resolved_root / "evidence-links.md"
    bundle_path = resolved_root / "validation-evidence-bundle.json"
    failure_details_path = resolved_root / "failure-details.json"
    report_manifest_path = resolved_root / "report-manifest.json"
    report_index_path = resolved_root / "report-index.json"

    case_records = [record.to_dict() for record in bundle.case_records]
    case_index_payload = {
        "generated_at": bundle.generated_at,
        "schema_version": bundle.schema_version,
        "bundle_id": bundle.bundle_id,
        "request_ref": bundle.request_ref,
        "producer_lane_ref": bundle.producer_lane_ref,
        "verdict": bundle.verdict,
        "cases": case_records,
    }
    _ensure_parent(case_index_path)
    case_index_path.write_text(json.dumps(case_index_payload, ensure_ascii=True, indent=2), encoding="utf-8")

    failure_records = [
        record
        for record in case_records
        if record["status"] in {"failed", "blocked", "known_failure", "incomplete", "deferred", "skipped"}
    ]
    if failure_records:
        _ensure_parent(failure_details_path)
        failure_details_path.write_text(
            json.dumps(
                {
                    "generated_at": bundle.generated_at,
                    "schema_version": bundle.schema_version,
                    "bundle_id": bundle.bundle_id,
                    "verdict": bundle.verdict,
                    "failures": failure_records,
                },
                ensure_ascii=True,
                indent=2,
            ),
            encoding="utf-8",
        )
    elif failure_details_path.exists():
        failure_details_path.unlink()

    default_links = [
        EvidenceLink(label="summary.json", path=str(summary_json_path.resolve()), role="machine-summary"),
        EvidenceLink(label="summary.md", path=str(summary_md_path.resolve()), role="human-summary"),
        EvidenceLink(label="case-index.json", path=str(case_index_path.resolve()), role="case-index"),
    ]
    if failure_records:
        default_links.append(
            EvidenceLink(label="failure-details.json", path=str(failure_details_path.resolve()), role="failure-details")
        )
    all_links = [*default_links, *evidence_links]

    manifest_files = [
        {"role": link.role, "label": link.label, "path": link.path, "exists": Path(link.path).exists()}
        for link in all_links
    ]
    manifest_files.extend(
        [
            {"role": "bundle", "label": "validation-evidence-bundle.json", "path": str(bundle_path.resolve()), "exists": True},
            {"role": "report-manifest", "label": "report-manifest.json", "path": str(report_manifest_path.resolve()), "exists": True},
            {"role": "report-index", "label": "report-index.json", "path": str(report_index_path.resolve()), "exists": True},
        ]
    )
    report_manifest_payload = {
        "schema_version": REPORT_MANIFEST_SCHEMA_VERSION,
        "bundle_schema_version": bundle.schema_version,
        "compatible_bundle_schema_versions": list(COMPATIBLE_BUNDLE_SCHEMA_VERSIONS),
        "generated_at": bundle.generated_at,
        "bundle_id": bundle.bundle_id,
        "request_ref": bundle.request_ref,
        "report_root": str(resolved_root),
        "files": manifest_files,
    }
    _ensure_parent(report_manifest_path)
    report_manifest_path.write_text(
        json.dumps(report_manifest_payload, ensure_ascii=True, indent=2),
        encoding="utf-8",
    )

    report_index_payload = {
        "schema_version": REPORT_INDEX_SCHEMA_VERSION,
        "bundle_schema_version": bundle.schema_version,
        "generated_at": bundle.generated_at,
        "bundle_id": bundle.bundle_id,
        "request_ref": bundle.request_ref,
        "producer_lane_ref": bundle.producer_lane_ref,
        "verdict": bundle.verdict,
        "report_root": str(resolved_root),
        "summary_file": str(summary_md_path.resolve()),
        "machine_file": str(summary_json_path.resolve()),
        "case_index_file": str(case_index_path.resolve()),
        "evidence_links_file": str(links_path.resolve()),
        "failure_details_file": str(failure_details_path.resolve()) if failure_records else "",
        "report_manifest_file": str(report_manifest_path.resolve()),
        "case_count": len(case_records),
        "cases": [
            {
                "case_id": record["case_id"],
                "status": record["status"],
                "suite_ref": record["suite_ref"],
                "primary_layer": record["primary_layer"],
            }
            for record in case_records
        ],
    }
    _ensure_parent(report_index_path)
    report_index_path.write_text(
        json.dumps(report_index_payload, ensure_ascii=True, indent=2),
        encoding="utf-8",
    )

    markdown_lines = [
        "# Evidence Links",
        "",
        f"- schema_version: `{bundle.schema_version}`",
        f"- bundle_id: `{bundle.bundle_id}`",
        f"- request_ref: `{bundle.request_ref}`",
        f"- producer_lane_ref: `{bundle.producer_lane_ref}`",
        f"- verdict: `{bundle.verdict}`",
        "",
        "## Links",
        "",
        f"- `report-manifest` `report-manifest.json`: `{report_manifest_path.resolve()}`",
        f"- `report-index` `report-index.json`: `{report_index_path.resolve()}`",
    ]
    for link in all_links:
        markdown_lines.append(f"- `{link.role}` `{link.label}`: `{link.path}`")
    markdown_lines.append("")
    _ensure_parent(links_path)
    links_path.write_text("\n".join(markdown_lines), encoding="utf-8")

    serialized_bundle = bundle.to_dict()
    serialized_bundle["summary_file"] = str(summary_md_path.resolve())
    serialized_bundle["machine_file"] = str(summary_json_path.resolve())
    serialized_bundle["case_index_file"] = str(case_index_path.resolve())
    serialized_bundle["evidence_links_file"] = str(links_path.resolve())
    serialized_bundle["failure_details_file"] = str(failure_details_path.resolve()) if failure_records else ""
    serialized_bundle["report_manifest_file"] = str(report_manifest_path.resolve())
    serialized_bundle["report_index_file"] = str(report_index_path.resolve())
    serialized_bundle["links"] = [link.to_dict() for link in all_links]
    _ensure_parent(bundle_path)
    bundle_path.write_text(json.dumps(serialized_bundle, ensure_ascii=True, indent=2), encoding="utf-8")

    return {
        "case_index_json": str(case_index_path.resolve()),
        "evidence_links_md": str(links_path.resolve()),
        "failure_details_json": str(failure_details_path.resolve()) if failure_records else "",
        "report_manifest_json": str(report_manifest_path.resolve()),
        "report_index_json": str(report_index_path.resolve()),
        "bundle_json": str(bundle_path.resolve()),
    }
