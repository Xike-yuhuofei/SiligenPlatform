/**
 * @file SplineGeometryMath.cpp
 * @brief 样条数学计算工具实现
 *
 * @author Claude Code
 * @date 2025-11-24
 */

#include "SplineGeometryMath.h"

#include <algorithm>
#include <cmath>

namespace Siligen::Domain::Motion {

std::vector<float32> SplineGeometryMath::ThomasAlgorithm(const std::vector<float32>& a,
                                                      const std::vector<float32>& b,
                                                      const std::vector<float32>& c,
                                                      const std::vector<float32>& d) const {
    size_t n = d.size();
    if (n == 0) {
        return {};
    }

    std::vector<float32> c_prime(n), d_prime(n), solution(n);

    // 前向消元
    c_prime[0] = c[0] / b[0];
    d_prime[0] = d[0] / b[0];

    for (size_t i = 1; i < n - 1; ++i) {
        float32 denominator = b[i] - a[i] * c_prime[i - 1];
        c_prime[i] = c[i] / denominator;
        d_prime[i] = (d[i] - a[i] * d_prime[i - 1]) / denominator;
    }

    d_prime[n - 1] = (d[n - 1] - a[n - 1] * d_prime[n - 2]) / (b[n - 1] - a[n - 1] * c_prime[n - 2]);

    // 回代
    solution[n - 1] = d_prime[n - 1];
    for (int32 i = static_cast<int32>(n) - 2; i >= 0; --i) {
        solution[i] = d_prime[i] - c_prime[i] * solution[i + 1];
    }

    return solution;
}

float32 SplineGeometryMath::CalculateSplineDerivative(const std::vector<SplineInterpolator::SplineSegment>& segments,
                                                   float32 x) const {
    for (const auto& segment : segments) {
        if (x >= segment.x_start && x <= segment.x_end) {
            float32 local_x = x - segment.x_start;
            return segment.b + 2.0f * segment.c * local_x + 3.0f * segment.d * local_x * local_x;
        }
    }

    return 0.0f;
}

float32 SplineGeometryMath::CalculateSplineCurvature(const std::vector<SplineInterpolator::SplineSegment>& segments,
                                                  float32 x) const {
    float32 first_derivative = CalculateSplineDerivative(segments, x);

    for (const auto& segment : segments) {
        if (x >= segment.x_start && x <= segment.x_end) {
            float32 local_x = x - segment.x_start;
            float32 second_derivative = 2.0f * segment.c + 6.0f * segment.d * local_x;
            return std::abs(second_derivative) / std::pow(1.0f + first_derivative * first_derivative, 1.5f);
        }
    }

    return 0.0f;
}

float32 SplineGeometryMath::CalculateSplineLength(const std::vector<SplineInterpolator::SplineSegment>& segments,
                                               int32 num_samples) const {
    float32 total_length = 0.0f;

    for (const auto& segment : segments) {
        float32 segment_length = 0.0f;
        Point2D prev_point;

        for (int32 i = 0; i <= num_samples; ++i) {
            float32 t = static_cast<float32>(i) / static_cast<float32>(num_samples);
            float32 x = segment.x_start + t * (segment.x_end - segment.x_start);
            float32 local_x = x - segment.x_start;
            float32 y = segment.a + segment.b * local_x + segment.c * local_x * local_x +
                        segment.d * local_x * local_x * local_x;

            Point2D current_point(x, y);

            if (i > 0) {
                segment_length += current_point.DistanceTo(prev_point);
            }

            prev_point = current_point;
        }

        total_length += segment_length;
    }

    return total_length;
}

std::vector<float32> SplineGeometryMath::AdaptiveStepControl(
    const std::vector<SplineInterpolator::SplineSegment>& segments, float32 max_error, float32 initial_step) const {
    std::vector<float32> steps;
    float32 current_x = segments[0].x_start;
    float32 end_x = segments.back().x_end;

    while (current_x < end_x) {
        float32 step = initial_step;

        // 自适应调整步长
        float32 error_estimate = 0.0f;
        int32 iteration = 0;
        const int32 max_iterations = 10;

        while (iteration < max_iterations) {
            // 计算当前步长的误差估计
            float32 mid_x = current_x + step * 0.5f;
            float32 end_x_step = current_x + step;

            float32 y_start = CalculateSplineDerivative(segments, current_x);
            float32 y_mid = CalculateSplineDerivative(segments, mid_x);
            float32 y_end = CalculateSplineDerivative(segments, end_x_step);

            // 使用Richardson外推法估计误差
            error_estimate = std::abs(y_end - 2.0f * y_mid + y_start);

            if (error_estimate < max_error) {
                break;
            }

            step *= 0.5f;
            iteration++;
        }

        steps.push_back(step);
        current_x += step;
    }

    return steps;
}

}  // namespace Siligen::Domain::Motion
