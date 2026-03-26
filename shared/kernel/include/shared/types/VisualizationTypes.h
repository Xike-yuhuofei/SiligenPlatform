// VisualizationTypes.h - 可视化相关类型 (Visualization related types)
// 职责: 定义可视化生成所需的数据传输对象(DTO)
// 架构合规性: 共享层零依赖，纯数据结构
#pragma once

#include "Point.h"  // 包含 MotionSegment 定义
#include "Point2D.h"
#include "Types.h"

#include <string>
#include <vector>

namespace Siligen {
namespace Shared {
namespace Types {

// DXF段类型枚举 (DXF segment type enumeration)
enum class DXFSegmentType { LINE, ARC, SPLINE };

// DXF实体类型枚举 (DXF entity type enumeration)
enum class DXFEntityType { Unknown, Line, Arc, Circle, LwPolyline, Spline, Ellipse, Point };

// DXF段结构 (DXF segment structure)
struct DXFSegment {
    DXFSegmentType type = DXFSegmentType::LINE;
    Point2D start_point;
    Point2D end_point;
    Point2D center_point;                 // 圆弧圆心
    float32 radius = 0;                   // 圆弧半径
    float32 start_angle = 0;              // 圆弧起始角度(度)
    float32 end_angle = 0;                // 圆弧终止角度(度)
    bool clockwise = false;               // 圆弧方向
    Point2D major_axis;                   // 椭圆长轴向量
    float32 ellipse_ratio = 0.0f;         // 椭圆短轴/长轴比例
    float32 ellipse_start = 0.0f;         // 椭圆起始参数(弧度)
    float32 ellipse_end = 0.0f;           // 椭圆结束参数(弧度)
    std::vector<Point2D> control_points;  // 样条控制点
    float32 length = 0;                   // 路径长度
    int32 entity_id = -1;                 // 原始DXF实体编号
    DXFEntityType entity_type = DXFEntityType::Unknown;  // 原始DXF实体类型
    int32 entity_segment_index = 0;       // 实体内段序号
    bool entity_closed = false;           // 实体是否闭合

    // 转换为运动段 (Convert to motion segment)
    // Note: MotionSegment is defined in Point.h
    MotionSegment ToMotionSegment() const;
};

// 可视化配置 (Visualization configuration)
struct VisualizationConfig {
    int width = 800;                    // 宽度 (像素)
    int height = 600;                   // 高度 (像素)
    float32 padding = 40.0f;            // 边距 (像素)
    std::string title = "DXF Preview";  // 标题
    bool show_direction = true;         // 显示方向箭头
    bool show_markers = true;           // 显示标记点
};

// 轨迹预览触发点 (Visualization trigger point)
struct VisualizationTriggerPoint {
    Point2D position;
    float32 time_s = 0.0f;

    VisualizationTriggerPoint() = default;
    VisualizationTriggerPoint(const Point2D& pos, float32 time_val) : position(pos), time_s(time_val) {}
};

}  // namespace Types
}  // namespace Shared
}  // namespace Siligen
