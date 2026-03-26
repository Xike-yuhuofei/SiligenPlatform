#pragma once

#include "OptimizedTypes.h"

#include <chrono>
#include <iostream>
#include <random>
#include <vector>

namespace Siligen {
namespace Benchmark {

// 插补性能基准测试
class InterpolationBenchmark {
   public:
    // 生成测试数据
    static std::vector<OptimizedPoint2D> GenerateTestPath(std::size_t num_points) {
        std::vector<OptimizedPoint2D> path;
        path.reserve(num_points);

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(-1000.0f, 1000.0f);

        for (std::size_t i = 0; i < num_points; ++i) {
            path.emplace_back(dist(gen), dist(gen));
        }

        return path;
    }

    // 测试SIMD优化的距离计算
    template <std::size_t Iterations = 1000000>
    static double BenchmarkSIMDDistanceCalculation() {
        auto path = GenerateTestPath(1000);
        volatile float total_distance = 0.0f;

        auto start = std::chrono::high_resolution_clock::now();

        for (std::size_t iter = 0; iter < Iterations; ++iter) {
            float distance = 0.0f;
            for (std::size_t i = 1; i < path.size(); ++i) {
                distance += path[i].DistanceTo(path[i - 1]);
            }
            total_distance = distance;  // 防止优化
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

        return static_cast<double>(duration.count()) / (Iterations * (path.size() - 1));
    }

    // 测试标准距离计算
    template <std::size_t Iterations = 1000000>
    static double BenchmarkStandardDistanceCalculation() {
        auto path = GenerateTestPath(1000);
        volatile float total_distance = 0.0f;

        auto start = std::chrono::high_resolution_clock::now();

        for (std::size_t iter = 0; iter < Iterations; ++iter) {
            float distance = 0.0f;
            for (std::size_t i = 1; i < path.size(); ++i) {
                distance += path[i].DistanceToStandard(path[i - 1]);
            }
            total_distance = distance;  // 防止优化
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

        return static_cast<double>(duration.count()) / (Iterations * (path.size() - 1));
    }

    // 测试内存池性能
    template <std::size_t Objects = 100000>
    static double BenchmarkMemoryPool() {
        MemoryPool<OptimizedTrajectoryPoint, 1024> pool;

        auto start = std::chrono::high_resolution_clock::now();

        // 分配阶段
        std::vector<OptimizedTrajectoryPoint*> objects;
        objects.reserve(Objects);

        for (std::size_t i = 0; i < Objects; ++i) {
            objects.push_back(pool.Allocate());
        }

        // 重置阶段
        pool.Reset();

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

        return static_cast<double>(duration.count()) / Objects;
    }

    // 测试标准new/delete性能
    template <std::size_t Objects = 100000>
    static double BenchmarkStandardAllocation() {
        std::vector<OptimizedTrajectoryPoint*> objects;
        objects.reserve(Objects);

        auto start = std::chrono::high_resolution_clock::now();

        // 分配阶段
        for (std::size_t i = 0; i < Objects; ++i) {
            objects.push_back(new OptimizedTrajectoryPoint());
        }

        // 释放阶段
        for (auto* obj : objects) {
            delete obj;
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

        return static_cast<double>(duration.count()) / Objects;
    }

    // 运行完整基准测试
    static void RunFullBenchmark() {
        std::cout << "=== Siligen 性能优化基准测试 ===" << std::endl << std::endl;

        // 距离计算性能测试
        std::cout << "距离计算性能测试:" << std::endl;
        double simd_time = BenchmarkSIMDDistanceCalculation();
        double standard_time = BenchmarkStandardDistanceCalculation();

        std::cout << "  SIMD优化:   " << simd_time << " ns/op" << std::endl;
        std::cout << "  标准算法:   " << standard_time << " ns/op" << std::endl;
        std::cout << "  性能提升:   " << (standard_time / simd_time) << "x" << std::endl << std::endl;

        // 内存分配性能测试
        std::cout << "内存分配性能测试:" << std::endl;
        double pool_time = BenchmarkMemoryPool();
        double new_time = BenchmarkStandardAllocation();

        std::cout << "  内存池:     " << pool_time << " ns/op" << std::endl;
        std::cout << "  new/delete: " << new_time << " ns/op" << std::endl;
        std::cout << "  性能提升:   " << (new_time / pool_time) << "x" << std::endl << std::endl;

        // 总结
        std::cout << "优化总结:" << std::endl;
        std::cout << "  SIMD距离计算: " << (standard_time / simd_time) << "x 性能提升" << std::endl;
        std::cout << "  内存池分配:   " << (new_time / pool_time) << "x 性能提升" << std::endl;
        std::cout << "  内存对齐:     16字节对齐，提高SIMD效率" << std::endl;
        std::cout << "  零开销DI:     编译时依赖注入，无运行时开销" << std::endl;
    }
};

}  // namespace Benchmark
}  // namespace Siligen