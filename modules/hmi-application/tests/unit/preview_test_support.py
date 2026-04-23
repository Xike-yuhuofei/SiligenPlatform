from __future__ import annotations

import types
from dataclasses import dataclass

from hmi_application.domain.preview_session_types import PreviewSessionState
from hmi_application.preview_gate import DispensePreviewGate
from hmi_application.services.preview_payload_authority import PreviewPayloadAuthorityService
from hmi_application.services.preview_playback import PreviewPlaybackController
from hmi_application.services.preview_preflight import PreviewPreflightService


@dataclass
class FakeStatus:
    connected: bool = True
    estop_known: bool = True
    door_known: bool = True
    estop_active: bool = False
    door_active: bool = False

    def gate_estop_known(self) -> bool:
        return self.estop_known

    def gate_door_known(self) -> bool:
        return self.door_known

    def gate_estop_active(self) -> bool:
        return self.estop_active

    def gate_door_active(self) -> bool:
        return self.door_active


def valid_payload(
    *,
    preview_source: str = "planned_glue_snapshot",
    preview_kind: str = "glue_points",
    dry_run: bool = False,
    glue_point_count: int = 3,
    motion_source_point_count: int = 10,
    include_motion_preview: bool = True,
) -> dict:
    glue_points = (
        [{"x": float(index) * 3.0, "y": 0.0} for index in range(glue_point_count)]
        if glue_point_count > 0
        else []
    )
    total_length_mm = float(glue_points[-1]["x"]) if glue_points else 0.0
    payload = {
        "snapshot_id": "snapshot-1",
        "snapshot_hash": "hash-1",
        "plan_id": "plan-1",
        "preview_source": preview_source,
        "preview_kind": preview_kind,
        "segment_count": 2,
        "glue_point_count": glue_point_count,
        "glue_points": glue_points,
        "glue_reveal_lengths_mm": [float(index) * 3.0 for index in range(glue_point_count)],
        "total_length_mm": total_length_mm,
        "estimated_time_s": 1.0,
        "generated_at": "2026-03-28T00:00:00Z",
        "dry_run": dry_run,
        "preview_diagnostic_code": "",
    }
    if include_motion_preview:
        payload["motion_preview"] = {
            "source": "execution_trajectory_snapshot",
            "kind": "polyline",
            "source_point_count": motion_source_point_count,
            "point_count": 2,
            "is_sampled": motion_source_point_count > 2,
            "sampling_strategy": "execution_trajectory_geometry_preserving_clamp",
            "polyline": [
                {"x": 0.0, "y": 0.0},
                {"x": total_length_mm, "y": 0.0},
            ],
        }
    return payload


def build_preview_services(
    gate: DispensePreviewGate | None = None,
) -> tuple[PreviewSessionState, PreviewPlaybackController, PreviewPreflightService, PreviewPayloadAuthorityService]:
    state = PreviewSessionState(gate=gate or DispensePreviewGate())
    playback = PreviewPlaybackController(state)
    preflight = PreviewPreflightService(state)
    authority = PreviewPayloadAuthorityService(state, playback, preflight)
    return state, playback, preflight, authority


class WorkerFakeClient:
    def __init__(self, host: str = "127.0.0.1", port: int = 9527) -> None:
        self.host = host
        self.port = port
        self.connected = False

    def connect(self) -> bool:
        self.connected = True
        return True

    def disconnect(self) -> None:
        self.connected = False


class WorkerFakeProtocol:
    calls: list[tuple] = []

    def __init__(self, client) -> None:
        self.client = client

    def dxf_prepare_plan(
        self,
        artifact_id: str,
        speed_mm_s: float,
        *,
        dry_run: bool = False,
        dry_run_speed_mm_s: float = 0.0,
        timeout: float = 15.0,
    ) -> tuple:
        type(self).calls.append(
            (
                "dxf.plan.prepare",
                artifact_id,
                speed_mm_s,
                dry_run,
                dry_run_speed_mm_s,
                timeout,
            )
        )
        return True, {
            "plan_id": "plan-1",
            "plan_fingerprint": "fp-1",
            "production_baseline": {
                "baseline_id": "baseline-production-default",
                "baseline_fingerprint": "baseline-fp-20260421",
            },
            "performance_profile": {
                "authority_cache_hit": False,
                "authority_joined_inflight": True,
                "prepare_total_ms": 123,
            },
        }, ""

    def dxf_preview_snapshot(
        self,
        plan_id: str,
        max_polyline_points: int = 4000,
        max_glue_points: int = 5000,
        timeout: float = 15.0,
    ) -> tuple:
        type(self).calls.append(("dxf.preview.snapshot", plan_id, max_polyline_points, max_glue_points, timeout))
        return True, {"snapshot_id": "snapshot-1", "preview_source": "planned_glue_snapshot", "preview_kind": "glue_points"}, ""


def worker_import_modules() -> dict[str, object]:
    hmi_client_package = types.ModuleType("hmi_client")
    hmi_client_package.__path__ = []  # type: ignore[attr-defined]
    hmi_client_client_package = types.ModuleType("hmi_client.client")
    hmi_client_client_package.__path__ = []  # type: ignore[attr-defined]
    client_package = types.ModuleType("client")
    client_package.__path__ = []  # type: ignore[attr-defined]
    tcp_module = types.ModuleType("client.tcp_client")
    setattr(tcp_module, "TcpClient", WorkerFakeClient)
    protocol_module = types.ModuleType("client.protocol")
    setattr(protocol_module, "CommandProtocol", WorkerFakeProtocol)
    return {
        "hmi_client": hmi_client_package,
        "hmi_client.client": hmi_client_client_package,
        "hmi_client.client.tcp_client": tcp_module,
        "hmi_client.client.protocol": protocol_module,
        "client": client_package,
        "client.tcp_client": tcp_module,
        "client.protocol": protocol_module,
    }
