try:
    from hmi_client.module_paths import ensure_hmi_application_path
except ImportError:  # pragma: no cover - script-mode fallback
    from module_paths import ensure_hmi_application_path  # type: ignore

ensure_hmi_application_path()

from .tcp_client import TcpClient
from .protocol import CommandProtocol, MachineStatus, AxisStatus
from .auth import AuthManager, User
from .recipe import Recipe, RecipeManager
from .backend_manager import BackendManager
from .gateway_launch import GatewayLaunchSpec, load_gateway_launch_spec
try:
    from hmi_client.features.dispense_preview_gate import (
        DispensePreviewGate,
        PreviewGateDecision,
        PreviewGateState,
        PreviewSnapshotMeta,
        StartBlockReason,
    )
except ImportError:  # pragma: no cover - script-mode fallback
    from features.dispense_preview_gate import (  # type: ignore
        DispensePreviewGate,
        PreviewGateDecision,
        PreviewGateState,
        PreviewSnapshotMeta,
        StartBlockReason,
    )
from .launch_state import (
    LaunchUiState,
    RecoveryActionDecision,
    RecoveryControlsState,
    RuntimeDegradationResult,
    build_launch_ui_state,
    build_online_capability_message,
    build_recovery_action_decision,
    build_recovery_finished_message,
    build_runtime_degradation_result,
    build_startup_error_message,
    detect_runtime_degradation_result,
)
from .supervisor_contract import (
    FailureCode,
    FailureStage,
    RecoveryAction,
    SessionStageEvent,
    SessionSnapshot,
    StageEventType,
    is_online_ready,
    snapshot_timestamp,
)
from .supervisor_session import SupervisorPolicy, SupervisorSession
from .startup_sequence import (
    build_offline_launch_result,
    LaunchResult,
    LaunchMode,
    RecoveryWorker,
    StartupResult,
    StartupWorker,
    launch_result_from_snapshot,
    load_supervisor_policy_from_env,
    normalize_launch_mode,
    run_recovery_action,
    run_launch_sequence,
)
try:
    from hmi_application.preview_session import PreviewSessionOwner, PreviewSnapshotWorker
except ImportError:  # pragma: no cover - script-mode fallback
    from preview_session import PreviewSessionOwner, PreviewSnapshotWorker  # type: ignore

__all__ = [
    "TcpClient",
    "CommandProtocol",
    "MachineStatus",
    "AxisStatus",
    "AuthManager",
    "User",
    "Recipe",
    "RecipeManager",
    "BackendManager",
    "GatewayLaunchSpec",
    "load_gateway_launch_spec",
    "DispensePreviewGate",
    "LaunchUiState",
    "SessionSnapshot",
    "FailureCode",
    "FailureStage",
    "PreviewGateDecision",
    "PreviewGateState",
    "PreviewSnapshotMeta",
    "RecoveryAction",
    "RecoveryActionDecision",
    "RecoveryControlsState",
    "StageEventType",
    "snapshot_timestamp",
    "SessionStageEvent",
    "SupervisorSession",
    "SupervisorPolicy",
    "RuntimeDegradationResult",
    "StartBlockReason",
    "PreviewSessionOwner",
    "PreviewSnapshotWorker",
    "build_launch_ui_state",
    "build_offline_launch_result",
    "build_online_capability_message",
    "build_recovery_action_decision",
    "build_recovery_finished_message",
    "build_runtime_degradation_result",
    "build_startup_error_message",
    "detect_runtime_degradation_result",
    "is_online_ready",
    "LaunchMode",
    "LaunchResult",
    "StartupWorker",
    "RecoveryWorker",
    "StartupResult",
    "normalize_launch_mode",
    "launch_result_from_snapshot",
    "load_supervisor_policy_from_env",
    "run_launch_sequence",
    "run_recovery_action",
]
