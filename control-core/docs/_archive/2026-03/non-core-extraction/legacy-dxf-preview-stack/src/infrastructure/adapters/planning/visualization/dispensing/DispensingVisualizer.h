#pragma once

#include "shared/types/Point.h"
#include "shared/types/VisualizationTypes.h"

#include <string>
#include <vector>

namespace Siligen {
using Siligen::TrajectoryPoint;

struct DispensingVisualizationConfig {
    int canvas_width = 1400;
    int canvas_height = 1000;
    bool show_velocity_heatmap = false;
    bool show_trigger_points = true;
    std::string title = "Dispensing Trajectory Preview";
    float dot_diameter = 1.0f;        // 点直径(mm)
    bool show_overlap_effect = true;  // 显示重叠效果
};

class DispensingVisualizer {
   public:
    DispensingVisualizer() = default;

    // 生成HTML可视化文件
    bool GenerateHTML(const std::vector<TrajectoryPoint>& trajectory_points,
                      const std::string& output_path,
                      const DispensingVisualizationConfig& config,
                      const std::vector<Shared::Types::VisualizationTriggerPoint>* planned_triggers = nullptr);

   private:
    // JSON序列化
    std::string SerializeToJSON(
        const std::vector<TrajectoryPoint>& trajectory_points,
        const std::vector<Shared::Types::VisualizationTriggerPoint>* planned_triggers);

    // 生成HTML模板
    std::string GenerateHTMLTemplate(const std::string& json_data, const DispensingVisualizationConfig& config);

    // 转义JSON字符串
    std::string EscapeJSON(const std::string& str);
};

}  // namespace Siligen
