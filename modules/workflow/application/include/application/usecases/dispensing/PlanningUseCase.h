#pragma once

#include "workflow/contracts/WorkflowContracts.h"
#include "application/services/dispensing/WorkflowPlanningAssemblyTypes.h"
#include "application/services/dispensing/WorkflowPlanningAssemblyOperations.h"
#include "process_planning/contracts/configuration/IConfigurationPort.h"
#include "domain/diagnostics/ports/IDiagnosticsPort.h"
#include "domain/dispensing/value-objects/AuthorityTriggerLayout.h"
#include "domain/dispensing/value-objects/DispenseCompensationProfile.h"
#include "domain/motion/domain-services/interpolation/TrajectoryInterpolatorBase.h"
#include "domain/system/ports/IEventPublisherPort.h"
#include "motion_planning/contracts/MotionPlan.h"
#include "motion_planning/contracts/MotionPlanningReport.h"
#include "motion_planning/contracts/TimePlanningConfig.h"
#include "process_path/contracts/IPathSourcePort.h"
#include "process_path/contracts/PathGenerationRequest.h"
#include "process_path/contracts/PathGenerationResult.h"
#include "domain/dispensing/contracts/ExecutionPackage.h"
#include "process_path/contracts/ProcessPath.h"
#include "shared/types/Point.h"
#include "shared/types/Result.h"
#include "shared/types/TrajectoryTypes.h"
#include "runtime_execution/application/services/dispensing/PlanningArtifactExportPort.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Siligen::Application::Services::DXF {
class DxfPbPreparationService;
}

namespace Siligen::Application::Services::Dispensing {
class IWorkflowPlanningAssemblyOperations;
}

namespace Siligen::Application::Services::ProcessPath {
class ProcessPathFacade;
}

namespace Siligen::Application::Services::MotionPlanning {
class MotionPlanningFacade;
}

namespace Siligen::Application::UseCases::Dispensing {

// 引入 Result 类型（位于 Siligen::Shared::Types）
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::TrajectoryConfig;
using Siligen::Shared::Types::TrajectoryResult;
using Siligen::TrajectoryPoint;
using Siligen::MotionPlanning::Contracts::MotionPlan;
using Siligen::MotionPlanning::Contracts::MotionPlanningReport;
using Siligen::MotionPlanning::Contracts::TimePlanningConfig;
using Siligen::ProcessPath::Contracts::PathGenerationRequest;
using Siligen::ProcessPath::Contracts::PathGenerationResult;
using Siligen::Domain::Dispensing::Contracts::ExecutionPackageValidated;

struct AuthorityProfile {
    std::uint32_t pb_prepare_ms = 0;
    std::uint32_t path_load_ms = 0;
    std::uint32_t process_path_ms = 0;
    std::uint32_t authority_build_ms = 0;
    std::uint32_t total_ms = 0;
};

struct ExecutionProfile {
    std::uint32_t motion_plan_ms = 0;
    std::uint32_t assembly_ms = 0;
    std::uint32_t export_ms = 0;
    std::uint32_t total_ms = 0;
};

struct PreparedAuthorityPreview {
    bool success = false;
    std::string source_path;
    std::string prepared_pb_path;
    Siligen::ProcessPath::Contracts::ProcessPath process_path;
    Siligen::ProcessPath::Contracts::ProcessPath authority_process_path;
    int discontinuity_count = 0;
    std::string preview_diagnostic_code;
    Siligen::Application::Services::Dispensing::WorkflowAuthorityPreviewArtifacts artifacts;
    AuthorityProfile authority_profile;
};

struct ExecutionAssemblyResponse {
    bool success = false;
    std::vector<TrajectoryPoint> execution_trajectory_points;
    std::vector<TrajectoryPoint> motion_trajectory_points;
    MotionPlanningReport planning_report;
    bool preview_authority_shared_with_execution = false;
    bool execution_binding_ready = false;
    std::string execution_failure_reason;
    std::shared_ptr<ExecutionPackageValidated> execution_package;
    Siligen::Domain::Dispensing::ValueObjects::AuthorityTriggerLayout authority_trigger_layout;
    Siligen::Application::Services::Dispensing::PlanningArtifactExportRequest export_request;
    ExecutionProfile execution_profile;
    int diagnostic_error_code = 0;
    std::string diagnostic_message;
    std::int64_t observed_at_ms = 0;
    bool export_attempted = false;
    bool export_succeeded = false;
};

struct PlanningRuntimeParams {
    float32 dispensing_velocity = 0.0f;
    float32 acceleration = 0.0f;
    float32 pulse_per_mm = 200.0f;
    uint32 dispenser_interval_ms = 0;
    uint32 dispenser_duration_ms = 0;
    float32 trigger_spatial_interval_mm = 0.0f;
    float32 valve_response_ms = 0.0f;
    float32 safety_margin_ms = 0.0f;
    float32 min_interval_ms = 0.0f;
    float32 sample_dt = 0.01f;
    float32 sample_ds = 0.0f;
    Siligen::Domain::Dispensing::ValueObjects::DispenseCompensationProfile compensation_profile{};
};

struct ExecutionAssemblyArtifacts {
    bool success = false;
    MotionPlanningReport planning_report;
    std::shared_ptr<ExecutionPackageValidated> execution_package;
    Siligen::Application::Services::Dispensing::WorkflowExecutionAssemblyResult artifacts;
};

/**
 * @brief DXF 路径规划请求参数
 */
/**
 * @brief DXF 路径规划请求参数
 */
struct PlanningRequest {
    std::string dxf_filepath;              ///< DXF 文件路径
    TrajectoryConfig trajectory_config;    ///< 轨迹配置参数
    bool optimize_path = true;             ///< 是否启用路径优化
    float32 start_x = 0.0f;                ///< 路径优化起点 X
    float32 start_y = 0.0f;                ///< 路径优化起点 Y
    bool approximate_splines = false;      ///< 是否将SPLINE近似为LINE
    int two_opt_iterations = 0;            ///< 2-Opt迭代次数 (0=禁用)
    float32 spline_max_step_mm = 0.0f;     ///< SPLINE近似步长(mm, 0=自动)
    float32 spline_max_error_mm = 0.0f;    ///< SPLINE近似最大误差(mm, 0=关闭)
    float32 continuity_tolerance_mm = 0.0f;  ///< 归一化连续性容差(mm, 0=默认)
    float32 curve_chain_angle_deg = 0.0f;    ///< 小角度曲线链角度阈值(度, 0=默认)
    float32 curve_chain_max_segment_mm = 0.0f;  ///< 曲线链最大线段长度(mm, 0=默认)
    bool use_hardware_trigger = true;      ///< 兼容字段：DXF 规划/执行固定使用规划位置触发
    bool use_interpolation_planner = false;  ///< 是否使用Domain插补算法
    MotionPlanning::Contracts::InterpolationAlgorithm interpolation_algorithm =
        MotionPlanning::Contracts::InterpolationAlgorithm::LINEAR;
    float32 spacing_tol_ratio = 0.0f;      ///< 点距容差比例(0=默认)
    float32 spacing_min_mm = 0.0f;         ///< 点距下限(0=默认)
    float32 spacing_max_mm = 0.0f;         ///< 点距上限(0=默认)

    /**
     * @brief 验证请求参数
     * @return true 如果参数有效
     */
    bool Validate() const {
        // 检查文件路径非空
        if (dxf_filepath.empty()) {
            return false;
        }

        // 检查轨迹配置参数范围
        if (trajectory_config.max_velocity <= 0.0f ||
            trajectory_config.max_velocity > 1000.0f) {
            return false;
        }

        if (trajectory_config.max_acceleration <= 0.0f ||
            trajectory_config.max_acceleration > 10000.0f) {
            return false;
        }

        if (trajectory_config.time_step <= 0.0f ||
            trajectory_config.time_step > 0.1f) {
            return false;
        }
        if (trajectory_config.arc_tolerance < 0.0f) {
            return false;
        }
        if (use_interpolation_planner &&
            (interpolation_algorithm == MotionPlanning::Contracts::InterpolationAlgorithm::LINEAR ||
             interpolation_algorithm == MotionPlanning::Contracts::InterpolationAlgorithm::CMP_COORDINATED) &&
            trajectory_config.max_jerk <= 0.0f) {
            return false;
        }
        if (spacing_tol_ratio < 0.0f || spacing_tol_ratio > 1.0f) {
            return false;
        }
        if (spacing_min_mm < 0.0f || spacing_max_mm < 0.0f) {
            return false;
        }
        if (spacing_min_mm > 0.0f && spacing_max_mm > 0.0f && spacing_min_mm > spacing_max_mm) {
            return false;
        }
        if (continuity_tolerance_mm < 0.0f) {
            return false;
        }
        if (curve_chain_angle_deg < 0.0f || curve_chain_angle_deg > 180.0f) {
            return false;
        }
        if (curve_chain_max_segment_mm < 0.0f) {
            return false;
        }

        return true;
    }
};

/**
 * @brief DXF 路径规划响应结果
 */
struct PlanningResponse {
    bool success = false;                      ///< 是否成功
    std::string error_message;                 ///< 错误信息

    // 解析统计
    int segment_count = 0;                     ///< 轨迹段数量
    float32 total_length = 0.0f;               ///< 总路径长度 (mm)
    float32 estimated_time = 0.0f;             ///< 预计执行时间 (s)

    // 预览数据
    std::vector<TrajectoryPoint> execution_trajectory_points;   ///< 执行轨迹点序列
    std::vector<Siligen::Shared::Types::Point2D> glue_points;   ///< 胶点触发点序列
    std::vector<int32> process_tags;                            ///< 执行轨迹点工艺标签(与执行轨迹点索引对齐)

    // CMP 触发统计
    int trigger_count = 0;                     ///< CMP 触发点数量

    // 元数据
    std::string dxf_filename;                  ///< 原始 DXF 文件名
    int32 timestamp = 0;                       ///< 生成时间戳

    // 规划报告
    MotionPlanningReport planning_report;      ///< 规划摘要指标

    // authority / preview gate 元数据
    bool preview_authority_ready = false;
    bool preview_authority_shared_with_execution = false;
    bool preview_binding_ready = false;
    bool preview_spacing_valid = false;
    bool preview_has_short_segment_exceptions = false;
    std::string preview_validation_classification;
    std::string preview_exception_reason;
    std::string preview_failure_reason;
    std::string preview_diagnostic_code;
    Siligen::Domain::Dispensing::ValueObjects::AuthorityTriggerLayout authority_trigger_layout;
    std::vector<Siligen::Application::Services::Dispensing::WorkflowAuthorityTriggerPoint> authority_trigger_points;
    std::vector<Siligen::Application::Services::Dispensing::WorkflowSpacingValidationGroup> spacing_validation_groups;
    AuthorityProfile authority_profile;
    ExecutionProfile execution_profile;

    // 过渡期执行载体：供 runtime-execution canonical API 直接消费
    std::shared_ptr<ExecutionPackageValidated> execution_package;
    int diagnostic_error_code = 0;
    std::string diagnostic_message;
    std::int64_t observed_at_ms = 0;
    bool export_attempted = false;
    bool export_succeeded = false;
};

class PlanningUseCase {
public:
    /**
     * @brief 构造函数
     * @param parser DXF 解析器
     * @param generator 轨迹生成器
     */
    PlanningUseCase(
        std::shared_ptr<Siligen::Domain::Trajectory::Ports::IPathSourcePort> path_source,
        std::shared_ptr<Siligen::Application::Services::ProcessPath::ProcessPathFacade> process_path_facade,
        std::shared_ptr<Siligen::Application::Services::MotionPlanning::MotionPlanningFacade> motion_planning_facade,
        std::shared_ptr<Siligen::Application::Services::Dispensing::IWorkflowPlanningAssemblyOperations> planning_operations,
        std::shared_ptr<Siligen::Domain::Configuration::Ports::IConfigurationPort> config_port = nullptr,
        std::shared_ptr<Siligen::Application::Services::DXF::DxfPbPreparationService>
            pb_preparation_service = nullptr,
        std::shared_ptr<Siligen::Application::Services::Dispensing::IPlanningArtifactExportPort>
            artifact_export_port = nullptr,
        std::shared_ptr<Siligen::Domain::Diagnostics::Ports::IDiagnosticsPort> diagnostics_port = nullptr,
        std::shared_ptr<Siligen::Domain::System::Ports::IEventPublisherPort> event_publisher_port = nullptr);

    ~PlanningUseCase() = default;

    // 禁止拷贝和移动
    PlanningUseCase(const PlanningUseCase&) = delete;
    PlanningUseCase& operator=(const PlanningUseCase&) = delete;
    PlanningUseCase(PlanningUseCase&&) = delete;
    PlanningUseCase& operator=(PlanningUseCase&&) = delete;

    /**
     * @brief 执行 DXF 路径规划
     * @param request 规划请求参数
     * @return Result<PlanningResponse> 规划结果
     */
    Result<PlanningResponse> Execute(const PlanningRequest& request);
    std::string BuildAuthorityCacheKey(const PlanningRequest& request) const;
    Result<PreparedAuthorityPreview> PrepareAuthorityPreview(const PlanningRequest& request);
    Result<ExecutionAssemblyResponse> AssembleExecutionFromAuthority(
        const PlanningRequest& request,
        const PreparedAuthorityPreview& authority_preview);

private:
    std::shared_ptr<Siligen::Domain::Trajectory::Ports::IPathSourcePort> path_source_;
    std::shared_ptr<Siligen::Application::Services::ProcessPath::ProcessPathFacade> process_path_facade_;
    std::shared_ptr<Siligen::Application::Services::MotionPlanning::MotionPlanningFacade> motion_planning_facade_;
    std::shared_ptr<Siligen::Application::Services::Dispensing::IWorkflowPlanningAssemblyOperations> planning_operations_;
    std::shared_ptr<Siligen::Domain::Configuration::Ports::IConfigurationPort> config_port_;
    std::shared_ptr<Siligen::Application::Services::DXF::DxfPbPreparationService> pb_preparation_service_;
    std::shared_ptr<Siligen::Application::Services::Dispensing::IPlanningArtifactExportPort> artifact_export_port_;
    std::shared_ptr<Siligen::Domain::Diagnostics::Ports::IDiagnosticsPort> diagnostics_port_;
    std::shared_ptr<Siligen::Domain::System::Ports::IEventPublisherPort> event_publisher_port_;

    /**
     * @brief 验证文件存在性
     * @param filepath 文件路径
     * @return Result<void> 验证结果
     */
    Result<void> ValidateFileExists(const std::string& filepath);

    /**
     * @brief 提取文件名
     * @param filepath 完整路径
     * @return 文件名（不含路径）
     */
    std::string ExtractFilename(const std::string& filepath);
    void RecordPlanningDiagnostic(
        Siligen::Workflow::Contracts::WorkflowFailureCategory failure_category,
        int error_code,
        const std::string& message,
        const std::string& workflow_run_id,
        const std::string& source_path,
        bool export_attempted,
        bool export_succeeded,
        std::uint32_t stage_duration_ms) const;
};

}  // namespace Siligen::Application::UseCases::Dispensing






