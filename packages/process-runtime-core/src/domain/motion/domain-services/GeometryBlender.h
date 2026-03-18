#pragma once

#include "shared/types/Point.h"

#include <vector>

namespace Siligen::Domain::Motion {

/**
 * @brief 几何平滑服务
 * @details 基于圆弧过渡对线段连接进行平滑处理
 */
class GeometryBlender {
   public:
    GeometryBlender() = default;
    ~GeometryBlender() = default;

    /**
     * @brief 对线段序列进行圆弧过渡平滑
     * @param segments 输入段序列
     * @param config 插补配置
     * @return 平滑后的段序列
     */
    std::vector<MotionSegment> BlendSegments(const std::vector<MotionSegment>& segments,
                                             const InterpolationConfig& config) const;

   private:
    bool TryBlendCorner(const MotionSegment& prev,
                        const MotionSegment& next,
                        const InterpolationConfig& config,
                        MotionSegment& blended_prev,
                        MotionSegment& blend_arc,
                        MotionSegment& blended_next) const;
};

}  // namespace Siligen::Domain::Motion
