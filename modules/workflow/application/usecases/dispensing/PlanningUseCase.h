#pragma once

#include "domain/configuration/ports/IConfigurationPort.h"
#include "shared/types/Point.h"
#include "shared/types/Result.h"
#include "shared/types/TrajectoryTypes.h"
#include "domain/trajectory/value-objects/PlanningReport.h"
#include "domain/motion/domain-services/interpolation/TrajectoryInterpolatorBase.h"
#include "application/services/dispensing/DispensePlanningFacade.h"
#include <memory>
#include <string>
#include <vector>

namespace Siligen::Application::Services::DXF {
class DxfPbPreparationService;
}

namespace Siligen::Application::UseCases::Dispensing {

// 引入 Result 类型（位于 Siligen::Shared::Types）
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::TrajectoryConfig;
using Siligen::Shared::Types::TrajectoryResult;
using Siligen::TrajectoryPoint;
using Siligen::Domain::Trajectory::ValueObjects::PlanningReport;
using Siligen::Domain::Dispensing::DomainServices::DispensingPlan;
using Siligen::Domain::Dispensing::DomainServices::DispensingPlanRequest;
using DispensingPlanner = Siligen::Application::Services::Dispensing::DispensePlanningFacade;

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
    Domain::Motion::InterpolationAlgorithm interpolation_algorithm =
        Domain::Motion::InterpolationAlgorithm::LINEAR;
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
            (interpolation_algorithm == Domain::Motion::InterpolationAlgorithm::LINEAR ||
             interpolation_algorithm == Domain::Motion::InterpolationAlgorithm::CMP_COORDINATED) &&
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

    // 轨迹数据
    std::vector<TrajectoryPoint> trajectory_points;  ///< 轨迹点序列
    std::vector<int32> process_tags;                 ///< 轨迹点工艺标签(与轨迹点索引对齐)

    // CMP 触发统计
    int trigger_count = 0;                     ///< CMP 触发点数量

    // 元数据
    std::string dxf_filename;                  ///< 原始 DXF 文件名
    int32 timestamp;                           ///< 生成时间戳

    // 规划报告
    PlanningReport planning_report;            ///< 规划摘要指标
};

/**
 * @brief DXF Web 路径规划用例
 *
 * 业务流程:
 * 1. 验证请求参数
 * 2. 调用统一规划器解析 DXF 并生成点胶规划
 * 3. 可选：应用路径优化
 * 4. 生成执行轨迹与触发统计
 * 5. 返回完整的规划结果
 *
 * 架构合规性:
 * - 组合现有组件（DispensingPlanner）
 * - 单一职责：仅编排 DXF 路径规划流程
 */
class PlanningUseCase {
public:
    /**
     * @brief 构造函数
     * @param parser DXF 解析器
     * @param generator 轨迹生成器
     */
    PlanningUseCase(
        std::shared_ptr<DispensingPlanner> planner,
        std::shared_ptr<Siligen::Domain::Configuration::Ports::IConfigurationPort> config_port = nullptr,
        std::shared_ptr<Siligen::Application::Services::DXF::DxfPbPreparationService>
            pb_preparation_service = nullptr);

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

private:
    std::shared_ptr<DispensingPlanner> planner_;
    std::shared_ptr<Siligen::Domain::Configuration::Ports::IConfigurationPort> config_port_;
    std::shared_ptr<Siligen::Application::Services::DXF::DxfPbPreparationService> pb_preparation_service_;

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
};

}  // namespace Siligen::Application::UseCases::Dispensing





