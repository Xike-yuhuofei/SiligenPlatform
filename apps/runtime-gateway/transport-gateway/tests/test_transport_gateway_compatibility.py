import json
import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[4]
CONTRACTS = ROOT / "shared" / "contracts" / "application"
DXF_COMMAND_SET = CONTRACTS / "commands" / "dxf.command-set.json"
DXF_PREVIEW_SUCCESS_FIXTURE = CONTRACTS / "fixtures" / "responses" / "dxf.preview.snapshot.success.json"
DXF_JOB_CONTINUE_REQUEST_FIXTURE = CONTRACTS / "fixtures" / "requests" / "dxf.job.continue.request.json"
DXF_JOB_CONTINUE_SUCCESS_FIXTURE = CONTRACTS / "fixtures" / "responses" / "dxf.job.continue.success.json"
PROTOCOL_MAPPING = CONTRACTS / "mappings" / "protocol-mapping.md"
TCP_DISPATCHER = ROOT / "apps" / "runtime-gateway" / "transport-gateway" / "src" / "tcp" / "TcpCommandDispatcher.cpp"
TCP_DISPATCHER_HEADER = ROOT / "apps" / "runtime-gateway" / "transport-gateway" / "src" / "tcp" / "TcpCommandDispatcher.h"
RUNTIME_STATUS_EXPORT_PORT = ROOT / "apps" / "runtime-service" / "runtime" / "status" / "RuntimeStatusExportPort.cpp"
RUNTIME_SUPERVISION_BACKEND = ROOT / "apps" / "runtime-service" / "runtime" / "supervision" / "RuntimeExecutionSupervisionBackend.cpp"
TCP_DISPENSING_FACADE_HEADER = ROOT / "apps" / "runtime-gateway" / "transport-gateway" / "src" / "facades" / "tcp" / "TcpDispensingFacade.h"
TCP_DISPENSING_FACADE_CPP = ROOT / "apps" / "runtime-gateway" / "transport-gateway" / "src" / "facades" / "tcp" / "TcpDispensingFacade.cpp"
TCP_MOTION_FACADE_HEADER = ROOT / "apps" / "runtime-gateway" / "transport-gateway" / "src" / "facades" / "tcp" / "TcpMotionFacade.h"
TCP_MOTION_FACADE_CPP = ROOT / "apps" / "runtime-gateway" / "transport-gateway" / "src" / "facades" / "tcp" / "TcpMotionFacade.cpp"
TCP_FACADE_BUILDER = ROOT / "apps" / "runtime-gateway" / "transport-gateway" / "include" / "siligen" / "gateway" / "tcp" / "tcp_facade_builder.h"
APPLICATION_CONTAINER_DISPENSING = ROOT / "apps" / "runtime-service" / "container" / "ApplicationContainer.Dispensing.cpp"
RUNTIME_EXECUTION_UC_HEADER = ROOT / "modules" / "runtime-execution" / "application" / "include" / "runtime_execution" / "application" / "usecases" / "dispensing" / "DispensingExecutionUseCase.h"
LEGACY_EXECUTION_WORKFLOW_HEADER = ROOT / "modules" / "workflow" / "application" / "include" / "application" / "usecases" / "dispensing" / "DispensingExecutionWorkflowUseCase.h"
LEGACY_EXECUTION_WORKFLOW_CPP = ROOT / "modules" / "workflow" / "application" / "usecases" / "dispensing" / "DispensingExecutionWorkflowUseCase.cpp"
LEGACY_COMPATIBILITY_SERVICE_HEADER = ROOT / "modules" / "workflow" / "application" / "services" / "dispensing" / "DispensingExecutionCompatibilityService.h"
LEGACY_COMPATIBILITY_SERVICE_CPP = ROOT / "modules" / "workflow" / "application" / "services" / "dispensing" / "DispensingExecutionCompatibilityService.cpp"
MOCK_IO_CONTROL_SERVICE = ROOT / "apps" / "runtime-gateway" / "transport-gateway" / "src" / "tcp" / "MockIoControlService.cpp"
TCP_SYSTEM_FACADE_HEADER = ROOT / "apps" / "runtime-gateway" / "transport-gateway" / "src" / "facades" / "tcp" / "TcpSystemFacade.h"
TCP_SYSTEM_FACADE_CPP = ROOT / "apps" / "runtime-gateway" / "transport-gateway" / "src" / "facades" / "tcp" / "TcpSystemFacade.cpp"
TCP_SERVER_HOST_HEADER = ROOT / "apps" / "runtime-gateway" / "transport-gateway" / "include" / "siligen" / "gateway" / "tcp" / "tcp_server_host.h"
APP_MAIN = ROOT / "apps" / "runtime-gateway" / "main.cpp"
TARGET_APP_CMAKE = ROOT / "apps" / "runtime-gateway" / "CMakeLists.txt"
TRANSPORT_GATEWAY_CMAKE = ROOT / "apps" / "runtime-gateway" / "transport-gateway" / "CMakeLists.txt"
RUNTIME_SERVICE_CMAKE = ROOT / "apps" / "runtime-service" / "CMakeLists.txt"
ROOT_CMAKE = ROOT / "CMakeLists.txt"
WORKSPACE_LAYOUT = ROOT / "cmake" / "workspace-layout.env"


def load_json(path: Path):
    return json.loads(path.read_text(encoding="utf-8"))


def load_workspace_layout():
    entries = {}
    for raw_line in WORKSPACE_LAYOUT.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        key, _, value = line.partition("=")
        if key and _:
            entries[key] = value
    return entries


def load_operations():
    operations = {}
    for folder in ("commands", "queries"):
        for path in sorted((CONTRACTS / folder).glob("*.json")):
            payload = load_json(path)
            for op in payload["operations"]:
                method = op["method"]
                if method in operations:
                    raise AssertionError(f"duplicate contract method: {method}")
                operations[method] = op
    return operations


def extract_tcp_methods():
    source = TCP_DISPATCHER.read_text(encoding="utf-8")
    return set(re.findall(r'RegisterCommand\("([^"]+)"', source))


def test_dispatcher_matches_contracts():
    operations = load_operations()
    tcp_methods = extract_tcp_methods()
    missing = sorted(tcp_methods - operations.keys())
    extra = sorted(operations.keys() - tcp_methods)
    assert not missing, f"transport-gateway methods missing from contracts: {missing}"
    assert not extra, f"contract methods not registered by transport-gateway: {extra}"


def test_app_entry_is_thin():
    source = APP_MAIN.read_text(encoding="utf-8")
    host_header = TCP_SERVER_HOST_HEADER.read_text(encoding="utf-8")
    assert '#include "siligen/gateway/tcp/tcp_facade_builder.h"' in source
    assert '#include "siligen/gateway/tcp/tcp_server_host.h"' in source
    assert '#include "runtime_execution/contracts/system/IRuntimeStatusExportPort.h"' in source
    assert 'BuildTcpFacadeBundle(*container)' in source
    assert 'TcpServerHost server_host(' in source
    assert "TcpServerHostOptions{port}" in source
    assert "ResolvePort<Siligen::RuntimeExecution::Contracts::System::IRuntimeStatusExportPort>()" in source
    assert "std::shared_ptr<RuntimeExecution::Contracts::System::IRuntimeStatusExportPort> runtime_status_export_port" in host_header
    assert 'TcpCommandDispatcher' not in source
    assert 'modules/control-gateway/src/' not in source


def test_gateway_public_host_contract_does_not_expose_config_path():
    host_header = TCP_SERVER_HOST_HEADER.read_text(encoding="utf-8")
    dispatcher_header = TCP_DISPATCHER_HEADER.read_text(encoding="utf-8")
    dispatcher_impl = TCP_DISPATCHER.read_text(encoding="utf-8")

    assert "std::string config_path" not in host_header
    assert "std::string config_path" not in dispatcher_header
    assert "config_path_" not in dispatcher_header
    assert "config_path_" not in dispatcher_impl
    assert "CanonicalMachineConfigRelativePath" not in dispatcher_impl


def test_canonical_targets_are_exported_without_legacy_aliases():
    target_app_cmake = TARGET_APP_CMAKE.read_text(encoding="utf-8")
    transport_gateway_cmake = TRANSPORT_GATEWAY_CMAKE.read_text(encoding="utf-8")
    runtime_service_cmake = RUNTIME_SERVICE_CMAKE.read_text(encoding="utf-8")
    root_cmake = ROOT_CMAKE.read_text(encoding="utf-8")
    workspace_layout = load_workspace_layout()

    assert "siligen_app_runtime_gateway" in target_app_cmake
    assert "siligen_transport_gateway" in target_app_cmake
    assert "siligen_runtime_process_bootstrap_public" in target_app_cmake
    assert "siligen_runtime_host" not in target_app_cmake
    assert "siligen_runtime_host_infrastructure" not in target_app_cmake
    assert "siligen_runtime_host_storage" not in target_app_cmake
    assert "siligen_transport_gateway" in transport_gateway_cmake
    assert "siligen_transport_gateway_protocol" in transport_gateway_cmake
    assert "siligen_runtime_process_bootstrap_public" in transport_gateway_cmake
    assert "siligen_runtime_host_infrastructure" not in transport_gateway_cmake
    assert "siligen_runtime_host_storage" not in transport_gateway_cmake
    assert "siligen_control_gateway_tcp_adapter" not in transport_gateway_cmake
    assert "siligen_tcp_adapter" not in transport_gateway_cmake
    assert "siligen_control_gateway" not in transport_gateway_cmake
    assert "siligen_runtime_process_bootstrap_public" in runtime_service_cmake
    assert 'add_subdirectory("${SILIGEN_TRANSPORT_GATEWAY_DIR}"' not in root_cmake
    assert workspace_layout.get("SILIGEN_TRANSPORT_GATEWAY_DIR") is None
    assert 'add_subdirectory("${SILIGEN_APPS_ROOT}"' in root_cmake
    assert workspace_layout.get("SILIGEN_APPS_ROOT") == "apps"


def test_dxf_preview_gate_contract_is_wired():
    source = TCP_DISPATCHER.read_text(encoding="utf-8")
    assert 'RegisterCommand("dxf.load"' not in source
    assert 'RegisterCommand("dxf.preview.snapshot"' in source
    assert 'RegisterCommand("dxf.preview.confirm"' in source
    assert 'RegisterCommand("dxf.artifact.create"' in source
    assert 'RegisterCommand("dxf.plan.prepare"' in source
    assert 'RegisterCommand("dxf.job.start"' in source
    assert 'RegisterCommand("dxf.job.status"' in source
    assert 'RegisterCommand("dxf.job.continue"' in source
    assert 'std::string TcpCommandDispatcher::HandleDxfPreviewSnapshot' in source
    assert 'std::string TcpCommandDispatcher::HandleDxfPreviewConfirm' in source
    assert 'std::string TcpCommandDispatcher::HandleDxfJobContinue' in source
    assert "GetDxfPreviewSnapshot(" in source
    assert "ConfirmDxfPreview(" in source
    assert '{"preview_source", snapshot.preview_source}' in source
    assert '{"preview_kind", snapshot.preview_kind}' in source
    assert '{"glue_points", glue_points}' in source
    assert '{"glue_reveal_lengths_mm", glue_reveal_lengths_mm}' in source
    assert '{"glue_point_count", snapshot.glue_point_count}' in source
    assert '{"motion_preview", motion_preview}' in source
    assert '{"source", snapshot.motion_preview_source}' in source
    assert '{"point_count", snapshot.motion_preview_point_count}' in source
    assert 'for (const auto& point : snapshot.motion_preview_polyline)' in source
    assert '{"execution_polyline"' not in source
    assert "dxf_cache_.preview_state = snapshot.preview_state;" in source
    assert 'request.snapshot_hash = snapshot_hash;' in source
    assert 'GatewayJsonProtocol::MakeErrorResponse(id, 3011, "Missing plan_id")' in source
    assert "PrepareDeprecatedPreviewSnapshotPlan(" not in source
    assert "deprecated_path_used=true" not in source
    assert "removal_target=" not in source
    assert "dxf.preview.snapshot max_polyline_points 超过上限，已夹断" in source
    assert "std::min<std::size_t>(" in source
    assert "kPreviewPolylineMaxPoints" in source
    assert 'GatewayJsonProtocol::MakeErrorResponse(id, 3012, snapshot_result.GetError().GetMessage())' in source
    assert 'GatewayJsonProtocol::MakeErrorResponse(id, 3012, "Preview plan_id mismatch")' in source
    assert 'GatewayJsonProtocol::MakeErrorResponse(id, 3014, "Preview snapshot hash is missing")' in source
    assert 'GatewayJsonProtocol::MakeErrorResponse(id, 3014, "Preview source is missing")' in source
    assert 'GatewayJsonProtocol::MakeErrorResponse(id, 3014, "Preview source must be planned_glue_snapshot")' in source
    assert 'GatewayJsonProtocol::MakeErrorResponse(id, 3014, "Preview kind must be glue_points")' in source
    assert 'GatewayJsonProtocol::MakeErrorResponse(id, 3014, "Preview glue points are empty")' in source
    assert "Preview motion preview source must be execution_trajectory_snapshot" in source
    assert "Preview motion preview kind must be polyline" in source
    assert "Preview motion preview polyline is empty" in source
    assert 'LogPreviewGateFailure("dxf.preview.snapshot"' in source
    assert 'LogPreviewGateFailure("dxf.preview.confirm"' in source
    assert 'LogPreviewGateFailure("dxf.job.start"' in source
    assert 'ReadJsonBool(params, "optimize_path", true)' in source
    assert 'ReadJsonBool(params, "use_interpolation_planner", true)' in source
    assert 'ReadJsonBool(params, "auto_continue", true)' in source
    assert 'ReadJsonInt(params, "requested_execution_strategy", -1)' in source
    assert "BuildPreparePlanRequest(" in source
    assert "BuildPreparePlanRuntimeOverrides(" in source
    assert "Missing 'snapshot_hash'" in source
    assert 'GatewayJsonProtocol::MakeErrorResponse(id, 3018, confirm_result.GetError().GetMessage())' in source
    assert "HandleDxfPreviewSnapshot" in source


def test_dxf_preview_contract_docs_freeze_shared_authority_semantics():
    command_set = load_json(DXF_COMMAND_SET)
    preview_operation = next(op for op in command_set["operations"] if op["method"] == "dxf.preview.snapshot")
    notes = "\n".join(preview_operation.get("compatibility", {}).get("notes", []))
    fixture = load_json(DXF_PREVIEW_SUCCESS_FIXTURE)
    mapping = PROTOCOL_MAPPING.read_text(encoding="utf-8")

    assert "shared authority" in notes.lower()
    assert "显式 plan_id" in notes
    assert "deprecated" not in notes.lower()
    assert "2026-04" not in notes
    assert fixture["result"]["preview_source"] == "planned_glue_snapshot"
    assert fixture["result"]["preview_kind"] == "glue_points"
    assert fixture["result"]["glue_point_count"] > 0
    assert len(fixture["result"]["glue_points"]) == fixture["result"]["glue_point_count"]
    assert len(fixture["result"]["glue_reveal_lengths_mm"]) == len(fixture["result"]["glue_points"])
    assert fixture["result"]["motion_preview"]["source"] == "execution_trajectory_snapshot"
    assert fixture["result"]["motion_preview"]["sampling_strategy"] == "execution_trajectory_geometry_preserving_clamp"
    assert len(fixture["result"]["motion_preview"]["polyline"]) == fixture["result"]["motion_preview"]["point_count"]
    assert "shared authority" in mapping.lower()


def test_dxf_plan_prepare_contract_exposes_requested_execution_strategy():
    command_set = load_json(DXF_COMMAND_SET)
    prepare_operation = next(op for op in command_set["operations"] if op["method"] == "dxf.plan.prepare")
    mapping = PROTOCOL_MAPPING.read_text(encoding="utf-8")
    dispatcher_source = TCP_DISPATCHER.read_text(encoding="utf-8")

    assert {"artifact_id", "dispensing_speed_mm_s"}.issubset(set(prepare_operation["paramsSchema"]["required"]))
    assert "recipe_id" not in prepare_operation["paramsSchema"]["properties"]
    assert "version_id" not in prepare_operation["paramsSchema"]["properties"]
    assert {"import_result_classification", "import_production_ready", "formal_compare_gate", "prepared_filepath"}.issubset(
        set(prepare_operation["resultSchema"]["required"])
    )
    assert {"execution_nominal_time_s", "execution_plan_summary"}.issubset(
        set(prepare_operation["resultSchema"]["required"])
    )
    assert "estimated_time_s" not in prepare_operation["resultSchema"]["required"]
    assert "speed_mm_s" not in prepare_operation["paramsSchema"]["properties"]
    assert "use_hardware_trigger" not in prepare_operation["paramsSchema"]["properties"]
    assert not prepare_operation.get("compatibility", {}).get("requestAliases")
    assert "requested_execution_strategy" in prepare_operation["paramsSchema"]["properties"]
    notes = "\n".join(prepare_operation.get("compatibility", {}).get("notes", []))
    assert "current production baseline" in notes
    assert "point_flying_carrier_policy" in notes
    assert "approach_direction = normalize(point - planning_start_position)" in notes
    assert "artifact_id 允许省略" not in notes
    assert "requested_execution_strategy" in mapping
    assert "current production baseline" in mapping
    assert "published" in mapping
    assert "recipe version" in mapping
    assert "activeVersionId" in mapping
    assert "允许省略 `artifact_id`" not in mapping
    job_start_operation = next(op for op in command_set["operations"] if op["method"] == "dxf.job.start")
    job_start_notes = "\n".join(job_start_operation.get("compatibility", {}).get("notes", []))
    assert "回退最近一次 prepared plan" not in job_start_notes
    assert {"import_result_classification", "import_production_ready", "formal_compare_gate", "prepared_filepath"}.issubset(
        set(job_start_operation["resultSchema"]["required"])
    )
    assert {"execution_budget_s", "execution_budget_breakdown"}.issubset(
        set(job_start_operation["resultSchema"]["required"])
    )
    assert 'GatewayJsonProtocol::MakeErrorResponse(id, 2895, "Missing artifact_id")' in dispatcher_source
    assert '{"execution_nominal_time_s", plan.execution_nominal_time_s}' in dispatcher_source
    assert '{"execution_plan_summary", BuildExecutionPlanSummaryJson(plan.execution_plan_summary)}' in dispatcher_source
    assert '{"execution_budget_s", start_response.execution_budget_s}' in dispatcher_source
    assert 'ReadJsonStringAlias(params, "recipeId", "recipe_id")' not in dispatcher_source
    assert 'ReadJsonStringAlias(params, "versionId", "version_id")' not in dispatcher_source
    assert 'ReadJsonDouble(params, "dispensing_speed_mm_s", ReadJsonDouble(params, "speed_mm_s", 0.0))' not in dispatcher_source
    assert 'ReadJsonBool(params, "use_hardware_trigger", true)' not in dispatcher_source


def test_dxf_job_continue_contract_freezes_wait_for_continue_semantics():
    command_set = load_json(DXF_COMMAND_SET)
    mapping = PROTOCOL_MAPPING.read_text(encoding="utf-8")
    dispatcher_source = TCP_DISPATCHER.read_text(encoding="utf-8")
    states = load_json(CONTRACTS / "models" / "states.json")
    continue_request = load_json(DXF_JOB_CONTINUE_REQUEST_FIXTURE)
    continue_success = load_json(DXF_JOB_CONTINUE_SUCCESS_FIXTURE)

    job_start_operation = next(op for op in command_set["operations"] if op["method"] == "dxf.job.start")
    job_continue_operation = next(op for op in command_set["operations"] if op["method"] == "dxf.job.continue")
    job_start_notes = "\n".join(job_start_operation.get("compatibility", {}).get("notes", []))

    assert "auto_continue" in job_start_operation["paramsSchema"]["properties"]
    assert "auto_continue 省略时默认为 true" in job_start_notes
    assert "awaiting_continue" in states["definitions"]["dxfJobStatus"]["properties"]["state"]["enum"]
    assert "dxf.job.continue" in mapping
    assert "awaiting_continue" in mapping
    assert "dispensingFacade_->ContinueDxfJob(job_id)" in dispatcher_source
    assert 'GatewayJsonProtocol::MakeErrorResponse(id, 2917, "TcpDispensingFacade not available")' in dispatcher_source
    assert 'GatewayJsonProtocol::MakeErrorResponse(id, 2918, "DXF job not waiting for continue")' in dispatcher_source
    assert 'GatewayJsonProtocol::MakeErrorResponse(id, 2919, continue_result.GetError().GetMessage())' in dispatcher_source
    assert continue_request["method"] == "dxf.job.continue"
    assert continue_request["params"]["job_id"]
    assert continue_success["result"]["continued"] is True
    assert continue_success["result"]["job_id"]


def test_legacy_execute_and_task_surface_are_removed():
    dispatcher_source = TCP_DISPATCHER.read_text(encoding="utf-8")
    dispatcher_header = TCP_DISPATCHER_HEADER.read_text(encoding="utf-8")
    facade_header = TCP_DISPENSING_FACADE_HEADER.read_text(encoding="utf-8")
    facade_impl = TCP_DISPENSING_FACADE_CPP.read_text(encoding="utf-8")
    builder = TCP_FACADE_BUILDER.read_text(encoding="utf-8")
    container = APPLICATION_CONTAINER_DISPENSING.read_text(encoding="utf-8")
    runtime_execution_header = RUNTIME_EXECUTION_UC_HEADER.read_text(encoding="utf-8")
    command_set = load_json(DXF_COMMAND_SET)
    states = load_json(CONTRACTS / "models" / "states.json")

    assert "std::string active_dxf_job_id_" in dispatcher_header
    assert "dxf_task_id_" not in dispatcher_header
    assert "dxf_task_id_" not in dispatcher_source
    assert '{"task_id", job_id}' not in dispatcher_source
    assert '{"active_task_id", status.active_task_id}' not in dispatcher_source
    assert "CancelDxfTask(" not in dispatcher_source
    assert "StopDxfExecution(" not in dispatcher_source

    assert "ExecuteDxfAsync(" not in facade_header
    assert "GetDxfTaskStatus(" not in facade_header
    assert "PauseDxfTask(" not in facade_header
    assert "ResumeDxfTask(" not in facade_header
    assert "CancelDxfTask(" not in facade_header
    assert "StopDxfExecution(" not in facade_header
    assert "DispensingExecutionWorkflowUseCase" not in facade_header
    assert "DispensingExecutionWorkflowUseCase" not in facade_impl
    assert "active_task_id" not in facade_impl

    assert "DispensingExecutionWorkflowUseCase" not in builder
    assert "DispensingExecutionCompatibilityService" not in container
    assert "DispensingExecutionWorkflowUseCase" not in container
    assert "SetLegacyExecutionForwarders(" not in container

    assert "SetLegacyExecutionForwarders(" not in runtime_execution_header
    assert "LegacyExecuteFn" not in runtime_execution_header
    assert "LegacyExecuteAsyncFn" not in runtime_execution_header
    assert "DispensingMVPRequest" not in runtime_execution_header
    assert "DispensingMVPResult" not in runtime_execution_header
    assert "TaskID" not in runtime_execution_header
    assert "TaskState" not in runtime_execution_header
    assert "TaskExecutionContext" not in runtime_execution_header
    assert "TaskStatusResponse" not in runtime_execution_header
    assert "Execute(const DispensingMVPRequest& request)" not in runtime_execution_header
    assert "ExecuteAsync(const DispensingMVPRequest& request)" not in runtime_execution_header
    assert "ExecuteAsync(" not in runtime_execution_header
    assert "GetTaskStatus(" not in runtime_execution_header
    assert "PauseTask(" not in runtime_execution_header
    assert "ResumeTask(" not in runtime_execution_header
    assert "CancelTask(" not in runtime_execution_header
    assert "CleanupExpiredTasks(" not in runtime_execution_header
    assert "StopExecution(" not in runtime_execution_header
    assert "DispensingExecutionResult" in runtime_execution_header
    runtime_job_status_match = re.search(
        r"struct RuntimeJobStatusResponse \{(?P<body>.*?)\n\};",
        runtime_execution_header,
        re.S,
    )
    assert runtime_job_status_match, "cannot locate RuntimeJobStatusResponse"
    assert "active_task_id" not in runtime_job_status_match.group("body")
    assert "execution_budget_s" in runtime_job_status_match.group("body")
    assert "execution_budget_breakdown" in runtime_job_status_match.group("body")

    assert not LEGACY_EXECUTION_WORKFLOW_HEADER.exists()
    assert not LEGACY_EXECUTION_WORKFLOW_CPP.exists()
    assert not LEGACY_COMPATIBILITY_SERVICE_HEADER.exists()
    assert not LEGACY_COMPATIBILITY_SERVICE_CPP.exists()

    job_start = next(op for op in command_set["operations"] if op["method"] == "dxf.job.start")
    assert "task_id" not in job_start["resultSchema"]["required"]
    assert "task_id" not in job_start["resultSchema"]["properties"]
    assert "active_task_id" not in states["definitions"]["dxfJobStatus"]["required"]
    assert "active_task_id" not in states["definitions"]["dxfJobStatus"]["properties"]


def test_status_reads_backend_interlock_signals():
    source = RUNTIME_SUPERVISION_BACKEND.read_text(encoding="utf-8")
    assert "ReadSignals()" in source
    assert "IsInEmergencyStop()" in source
    assert "if (estop_state_result.IsSuccess() && inputs.connected)" in source
    assert "inputs.io.estop_known = inputs.connected;" in source
    assert "inputs.io.estop = inputs.estop_active || signals.emergency_stop_triggered;" in source
    assert "inputs.io.estop = inputs.estop_active;" in source
    assert "GetActiveJobId()" in source
    assert "dispensing_execution_use_case_->GetJobStatus(inputs.active_job_id)" in source
    assert "motion_control_use_case->ReadLimitStatus(axis, positive)" in source
    assert "ReadLimitIfAvailable(motion_control_use_case_, LogicalAxisId::X, true, inputs.io.limit_x_pos);" in source
    assert "ReadLimitIfAvailable(motion_control_use_case_, LogicalAxisId::X, false, inputs.io.limit_x_neg);" in source
    assert "ReadLimitIfAvailable(motion_control_use_case_, LogicalAxisId::Y, true, inputs.io.limit_y_pos);" in source
    assert "ReadLimitIfAvailable(motion_control_use_case_, LogicalAxisId::Y, false, inputs.io.limit_y_neg);" in source


def test_status_supervision_contract_is_derived_by_runtime_supervision_adapter():
    source = (ROOT / "modules" / "runtime-execution" / "runtime" / "host" / "runtime" / "supervision" / "RuntimeSupervisionPortAdapter.cpp").read_text(encoding="utf-8")
    assert "const bool heartbeat_degraded_while_connected =" in source
    assert "inputs.connection_info.state == Siligen::Device::Contracts::State::DeviceConnectionState::Connected &&" in source
    assert 'connection_state = "degraded";' in source
    assert 'state_reason = "heartbeat_degraded";' in source
    assert 'snapshot.sources["estop"] =' in source
    assert 'snapshot.sources["door_open"] =' in source
    assert 'snapshot.requested_state = "Idle";' in source
    assert 'snapshot.requested_state = "Estop";' in source
    assert 'snapshot.requested_state = "Fault";' in source
    assert 'snapshot.failure_code = "heartbeat_degraded";' in source
    assert 'snapshot.failure_stage = snapshot.failure_code.empty() ? "" : "runtime_status";' in source


def test_status_dispatcher_only_serializes_authority_status_fields():
    source = TCP_DISPATCHER.read_text(encoding="utf-8")
    status_source = RUNTIME_STATUS_EXPORT_PORT.read_text(encoding="utf-8")
    assert "runtimeStatusExportPort_->ReadSnapshot()" in source
    assert "BuildRawIoJson(status_snapshot)" in source
    assert "BuildEffectiveInterlocksJson(status_snapshot)" in source
    assert "BuildSupervisionJson(status_snapshot)" in source
    assert "BuildJobExecutionJson(status_snapshot)" in source
    assert "BuildCompatMachineState(" not in source
    assert '{"machine_state", status_snapshot.machine_state}' in source
    assert '{"machine_state_reason", status_snapshot.machine_state_reason}' in source
    assert "snapshot.machine_state = supervision.supervision.current_state;" in status_source
    assert "snapshot.machine_state_reason = supervision.supervision.state_reason;" in status_source
    assert "snapshot.dispenser.completedCount = dispenser.completedCount;" in status_source
    assert "snapshot.dispenser.totalCount = dispenser.totalCount;" in status_source
    assert "snapshot.dispenser.progress = dispenser.progress;" in status_source
    assert '{"supervision", supervisionJson}' in source
    assert '{"effective_interlocks", effectiveInterlocksJson}' in source
    assert '{"job_execution", jobExecutionJson}' in source
    assert '{"execution_budget_s", snapshot.job_execution.execution_budget_s}' in source
    assert '{"active_job_id"' not in source
    assert '{"active_job_state"' not in source
    assert '{"completedCount", snapshot.dispenser.completedCount}' in source
    assert '{"totalCount", snapshot.dispenser.totalCount}' in source
    assert '{"progress", snapshot.dispenser.progress}' in source


def test_manual_dispenser_pause_resume_do_not_stop_dxf_job_implicitly():
    source = TCP_DISPATCHER.read_text(encoding="utf-8")

    pause_match = re.search(
        r"std::string TcpCommandDispatcher::HandleDispenserPause.*?return GatewayJsonProtocol::MakeSuccessResponse",
        source,
        re.S,
    )
    resume_match = re.search(
        r"std::string TcpCommandDispatcher::HandleDispenserResume.*?return GatewayJsonProtocol::MakeSuccessResponse",
        source,
        re.S,
    )
    assert pause_match, "cannot locate HandleDispenserPause body"
    assert resume_match, "cannot locate HandleDispenserResume body"

    pause_body = pause_match.group(0)
    resume_body = resume_match.group(0)

    assert "StopDxfJob(" not in pause_body
    assert "GetDxfJobStatus(current_dxf_job_id)" in pause_body
    assert "manual dispenser pause is forbidden while DXF job is active" in pause_body
    assert "GetDxfJobStatus(current_dxf_job_id)" in resume_body
    assert "manual dispenser resume is forbidden while DXF job is active" in resume_body


def test_motion_coord_status_exposes_feedback_diagnostics():
    source = TCP_DISPATCHER.read_text(encoding="utf-8")
    assert 'RegisterCommand("motion.coord.status"' in source
    assert '{"home_failed", x_status_result.Value().home_failed}' in source
    assert '{"selected_feedback_source", x_status_result.Value().selected_feedback_source}' in source
    assert '{"prf_position_mm", x_status_result.Value().profile_position_mm}' in source
    assert '{"enc_position_mm", x_status_result.Value().encoder_position_mm}' in source
    assert '{"prf_velocity_mm_s", x_status_result.Value().profile_velocity_mm_s}' in source
    assert '{"enc_velocity_mm_s", x_status_result.Value().encoder_velocity_mm_s}' in source
    assert '{"prf_position_ret", x_status_result.Value().profile_position_ret}' in source
    assert '{"enc_position_ret", x_status_result.Value().encoder_position_ret}' in source
    assert '{"prf_velocity_ret", x_status_result.Value().profile_velocity_ret}' in source
    assert '{"enc_velocity_ret", x_status_result.Value().encoder_velocity_ret}' in source
    assert '{"home_failed", y_status_result.Value().home_failed}' in source
    assert '{"selected_feedback_source", y_status_result.Value().selected_feedback_source}' in source
    assert '{"prf_position_mm", y_status_result.Value().profile_position_mm}' in source
    assert '{"enc_position_mm", y_status_result.Value().encoder_position_mm}' in source
    assert '{"prf_velocity_mm_s", y_status_result.Value().profile_velocity_mm_s}' in source
    assert '{"enc_velocity_mm_s", y_status_result.Value().encoder_velocity_mm_s}' in source
    assert '{"prf_position_ret", y_status_result.Value().profile_position_ret}' in source
    assert '{"enc_position_ret", y_status_result.Value().encoder_position_ret}' in source
    assert '{"prf_velocity_ret", y_status_result.Value().profile_velocity_ret}' in source
    assert '{"enc_velocity_ret", y_status_result.Value().encoder_velocity_ret}' in source


def test_estop_reset_and_disconnect_semantics_are_wired():
    source = TCP_DISPATCHER.read_text(encoding="utf-8")
    facade_header = TCP_SYSTEM_FACADE_HEADER.read_text(encoding="utf-8")
    facade_impl = TCP_SYSTEM_FACADE_CPP.read_text(encoding="utf-8")

    assert 'RegisterCommand("estop.reset"' in source
    assert "std::string TcpCommandDispatcher::HandleEstopReset" in source
    assert '{"reset", true}' in source
    assert '{"disconnected", true}' in source
    assert "Shared::Types::Result<void> RecoverFromEmergencyStop();" in facade_header
    assert "emergency_stop_use_case_->RecoverFromEmergencyStop()" in facade_impl


def test_stop_command_uses_unified_non_estop_motion_stop_path():
    source = TCP_DISPATCHER.read_text(encoding="utf-8")
    facade_header = (ROOT / "apps" / "runtime-gateway" / "transport-gateway" / "src" / "facades" / "tcp" / "TcpMotionFacade.h").read_text(encoding="utf-8")
    facade_impl = (ROOT / "apps" / "runtime-gateway" / "transport-gateway" / "src" / "facades" / "tcp" / "TcpMotionFacade.cpp").read_text(encoding="utf-8")
    builder = TCP_FACADE_BUILDER.read_text(encoding="utf-8")

    stop_match = re.search(
        r"std::string TcpCommandDispatcher::HandleStop.*?return GatewayJsonProtocol::MakeSuccessResponse",
        source,
        re.S,
    )
    assert stop_match, "cannot locate HandleStop body"
    stop_body = stop_match.group(0)

    assert "Shared::Types::Result<void> StopAllAxes(bool immediate = false);" in facade_header
    assert "motion_safety_use_case_" in facade_header
    assert "motion_safety_use_case_->StopAllAxes(immediate)" in facade_impl
    assert "Resolve<Application::UseCases::Motion::Safety::MotionSafetyUseCase>()" in builder
    assert "motionFacade_->StopAllAxes(false)" in stop_body
    assert "StopJog(axis_id)" not in stop_body
    assert "axis_index < 4" not in stop_body


def test_mock_io_set_is_registered_and_wired():
    source = TCP_DISPATCHER.read_text(encoding="utf-8")
    mock_io_service = MOCK_IO_CONTROL_SERVICE.read_text(encoding="utf-8")
    host_header = TCP_SERVER_HOST_HEADER.read_text(encoding="utf-8")
    app_main = APP_MAIN.read_text(encoding="utf-8")

    assert 'RegisterCommand("mock.io.set"' in source
    assert "std::string TcpCommandDispatcher::HandleMockIoSet" in source
    assert '{"estop", state.estop}' in source
    assert '{"door", state.door}' in source
    assert '{"limit_x_neg", state.limit_x_neg}' in source
    assert '{"limit_y_neg", state.limit_y_neg}' in source
    assert "mock.io.set is only available when Hardware.mode=Mock" in mock_io_service
    assert "mock.io.set cannot drive door because [Interlock].safety_door_input is disabled" in mock_io_service
    assert "std::shared_ptr<Adapters::Tcp::MockIoControlService> mock_io_control" in host_header
    assert "MockIoControlService" in app_main


def test_homed_semantics_follow_homing_state_only():
    source = TCP_DISPATCHER.read_text(encoding="utf-8")

    status_match = re.search(
        r"std::string TcpCommandDispatcher::HandleStatus.*?return GatewayJsonProtocol::MakeSuccessResponse",
        source,
        re.S,
    )
    assert status_match, "cannot locate HandleStatus body"
    status_body = status_match.group(0)
    assert "BuildAxesJson(status_snapshot)" in status_body
    assert "MotionState::HOMED" not in status_body

    coord_match = re.search(
        r"std::string TcpCommandDispatcher::HandleMotionCoordStatus.*?return GatewayJsonProtocol::MakeSuccessResponse",
        source,
        re.S,
    )
    assert coord_match, "cannot locate HandleMotionCoordStatus body"
    coord_body = coord_match.group(0)
    assert '{"homed", x_status_result.Value().homing_state == "homed"}' in coord_body
    assert '{"homed", y_status_result.Value().homing_state == "homed"}' in coord_body
    assert "MotionState::HOMED" not in coord_body


def test_manual_commands_are_guarded_by_interlocks():
    source = TCP_DISPATCHER.read_text(encoding="utf-8")
    assert "MakeManualInterlockErrorResponse(" in source
    assert "ReadManualInterlockSnapshot(systemFacade_, dispensingFacade_)" in source
    assert "2404" in source
    assert "2305" in source
    assert "2417" in source
    assert "2703" in source
    assert "2842" in source


def test_move_prechecks_directional_limits_before_dispatch():
    source = TCP_DISPATCHER.read_text(encoding="utf-8")

    assert "std::optional<std::string> CheckMoveAxisLimitBeforeDispatch(" in source
    assert "ReadLimitStatus(axis_id, positive)" in source

    move_match = re.search(
        r"std::string TcpCommandDispatcher::HandleMove.*?return GatewayJsonProtocol::MakeSuccessResponse",
        source,
        re.S,
    )
    assert move_match, "cannot locate HandleMove body"
    move_body = move_match.group(0)
    assert "GetCurrentPosition()" in move_body
    assert "CheckMoveAxisLimitBeforeDispatch(motionFacade_, LogicalAxisId::X, current_position.x, x)" in move_body
    assert "CheckMoveAxisLimitBeforeDispatch(motionFacade_, LogicalAxisId::Y, current_position.y, y)" in move_body


def test_home_command_exposes_axis_level_results():
    source = TCP_DISPATCHER.read_text(encoding="utf-8")
    assert 'RegisterCommand("home"' in source
    assert '{"succeeded_axes", axes_to_json(response.successfully_homed_axes)}' in source
    assert '{"failed_axes", axes_to_json(response.failed_axes)}' in source
    assert '{"error_messages", response.error_messages}' in source
    assert '{"axis_results", axis_results_to_json(response.axis_results)}' in source
    assert '{"total_time_ms", response.total_time_ms}' in source
    assert "Hardware connection degraded, reconnect before homing" in source


def test_home_auto_command_is_registered_and_uses_supervisor_chain():
    source = TCP_DISPATCHER.read_text(encoding="utf-8")
    facade_header = (ROOT / "apps" / "runtime-gateway" / "transport-gateway" / "src" / "facades" / "tcp" / "TcpMotionFacade.h").read_text(encoding="utf-8")
    facade_impl = (ROOT / "apps" / "runtime-gateway" / "transport-gateway" / "src" / "facades" / "tcp" / "TcpMotionFacade.cpp").read_text(encoding="utf-8")

    assert 'RegisterCommand("home.auto"' in source
    assert "std::string TcpCommandDispatcher::HandleHomeAuto" in source
    assert 'motionFacade_->EnsureAxesReadyZero(request)' in source
    assert '{"accepted", response.accepted}' in source
    assert '{"summary_state", response.summary_state}' in source
    assert '{"axis_results", axisResultsJson}' in source
    assert '{"total_time_ms", response.total_time_ms}' in source
    assert "HandleHomeGo" in source
    assert "HandleHome(" in source
    assert "EnsureAxesReadyZero(" in facade_header
    assert "motion_control_use_case_" in facade_header
    assert "motion_control_use_case_->EnsureAxesReadyZero(request)" in facade_impl


def test_manual_readiness_gate_uses_typed_job_transition_owner_state():
    dispatcher_source = TCP_DISPATCHER.read_text(encoding="utf-8")
    dispatcher_header = TCP_DISPATCHER_HEADER.read_text(encoding="utf-8")
    facade_header = TCP_DISPENSING_FACADE_HEADER.read_text(encoding="utf-8")
    facade_impl = TCP_DISPENSING_FACADE_CPP.read_text(encoding="utf-8")

    assert "GetDxfJobTransitionState(" in facade_header
    assert "Shared::Types::Result<UseCases::Dispensing::ExecutionTransitionState>" in facade_header
    assert "dxf_execute_use_case_->GetJobStatus(job_id)" in facade_impl
    assert "runtime_result.Value().transition_state" in facade_impl
    assert "ResolveActiveDxfJobTransitionState() const;" in dispatcher_header
    assert "BuildReadinessQuery(ResolveActiveDxfJobTransitionState())" in dispatcher_source

    resolve_match = re.search(
        r"TcpCommandDispatcher::ResolveActiveDxfJobTransitionState\(\) const \{(?P<body>.*?)\n\}",
        dispatcher_source,
        re.S,
    )
    assert resolve_match, "cannot locate ResolveActiveDxfJobTransitionState body"
    resolve_body = resolve_match.group("body")
    assert "GetDxfJobTransitionState(job_id)" in resolve_body
    assert "GetDxfJobStatus(job_id)" not in resolve_body
    assert ".state" not in resolve_body


def test_home_go_uses_ready_zero_configuration_and_rejects_speed_override():
    source = TCP_DISPATCHER.read_text(encoding="utf-8")

    home_go_match = re.search(
        r"std::string TcpCommandDispatcher::HandleHomeGo.*?return GatewayJsonProtocol::MakeSuccessResponse",
        source,
        re.S,
    )
    assert home_go_match, "cannot locate HandleHomeGo body"
    home_go_body = home_go_match.group(0)

    assert "ResolveReadyZeroSpeed(" in home_go_body
    assert "ready_zero_speed_mm_s" in home_go_body
    assert "home.go speed override is not supported; configure ready_zero_speed_mm_s" in home_go_body
    assert "machine_result.Value().max_speed" not in home_go_body


def test_jog_allows_positive_escape_when_home_is_active_but_axis_not_homed():
    source = TCP_DISPATCHER.read_text(encoding="utf-8")

    jog_match = re.search(
        r"std::string TcpCommandDispatcher::HandleJog.*?return GatewayJsonProtocol::MakeSuccessResponse",
        source,
        re.S,
    )
    assert jog_match, "cannot locate HandleJog body"
    jog_body = jog_match.group(0)
    assert "const bool positive_escape_from_home = axis_status.home_signal && direction > 0;" in jog_body
    assert 'if (!is_homed && !positive_escape_from_home)' in jog_body
    assert '"Axis not homed; HOME is active, only positive escape jog is allowed"' in jog_body
    assert '"Axis not homed, run homing first"' in jog_body


def test_move_command_uses_synchronized_xy_position_control():
    dispatcher_source = TCP_DISPATCHER.read_text(encoding="utf-8")
    motion_facade_header = TCP_MOTION_FACADE_HEADER.read_text(encoding="utf-8")
    motion_facade_impl = TCP_MOTION_FACADE_CPP.read_text(encoding="utf-8")
    builder = TCP_FACADE_BUILDER.read_text(encoding="utf-8")

    assert "Shared::Types::Result<void> MoveToPosition(const Point2D& position, float32 velocity);" in motion_facade_header
    assert "position_control_port_->MoveToPosition(position, velocity);" in motion_facade_impl
    assert "ResolvePort<Siligen::Domain::Motion::Ports::IPositionControlPort>()" in builder
    assert "motionFacade_->MoveToPosition(target_position, speed);" in dispatcher_source
    assert "auto resultX = motionFacade_->ExecutePointToPointMotion(cmdX);" not in dispatcher_source
    assert "auto resultY = motionFacade_->ExecutePointToPointMotion(cmdY);" not in dispatcher_source


def main():
    tests = [
        test_dispatcher_matches_contracts,
        test_app_entry_is_thin,
        test_gateway_public_host_contract_does_not_expose_config_path,
        test_canonical_targets_are_exported_without_legacy_aliases,
        test_dxf_preview_gate_contract_is_wired,
        test_dxf_preview_contract_docs_freeze_shared_authority_semantics,
        test_dxf_plan_prepare_contract_exposes_requested_execution_strategy,
        test_dxf_job_continue_contract_freezes_wait_for_continue_semantics,
        test_legacy_execute_and_task_surface_are_removed,
        test_status_reads_backend_interlock_signals,
        test_status_supervision_contract_is_derived_by_runtime_supervision_adapter,
        test_status_dispatcher_only_serializes_authority_status_fields,
        test_manual_dispenser_pause_resume_do_not_stop_dxf_job_implicitly,
        test_motion_coord_status_exposes_feedback_diagnostics,
        test_estop_reset_and_disconnect_semantics_are_wired,
        test_mock_io_set_is_registered_and_wired,
        test_homed_semantics_follow_homing_state_only,
        test_manual_commands_are_guarded_by_interlocks,
        test_move_prechecks_directional_limits_before_dispatch,
        test_home_command_exposes_axis_level_results,
        test_home_auto_command_is_registered_and_uses_supervisor_chain,
        test_manual_readiness_gate_uses_typed_job_transition_owner_state,
        test_home_go_uses_ready_zero_configuration_and_rejects_speed_override,
        test_jog_allows_positive_escape_when_home_is_active_but_axis_not_homed,
        test_move_command_uses_synchronized_xy_position_control,
    ]
    for test in tests:
        test()
    print("transport-gateway compatibility checks passed")


if __name__ == "__main__":
    try:
        main()
    except AssertionError as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        sys.exit(1)
