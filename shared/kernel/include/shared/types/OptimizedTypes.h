#pragma once

#include <array>
#include <boost/geometry/algorithms/distance.hpp>
#include <boost/geometry/geometries/register/point.hpp>
#include <cstdint>
#include <cstring>
#include <immintrin.h>  // SSE/AVX指令集
#include <memory>

namespace Siligen {
namespace Optimized {

// 对齐内存分配器 - 16字节对齐以提高SIMD性能
template <typename T, std::size_t Alignment = 16>
class AlignedAllocator {
   public:
    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using size_type = std::size_t;

    template <typename U>
    struct rebind {
        using other = AlignedAllocator<U, Alignment>;
    };

    pointer allocate(size_type n) {
        if (n == 0) return nullptr;

        void* ptr = _aligned_malloc(n * sizeof(T), Alignment);
        if (!ptr) throw std::bad_alloc();

        return static_cast<pointer>(ptr);
    }

    void deallocate(pointer p, size_type) noexcept {
        if (p) {
            _aligned_free(p);
        }
    }

    template <typename U, typename... Args>
    void construct(U* p, Args&&... args) {
        ::new (static_cast<void*>(p)) U(std::forward<Args>(args)...);
    }

    template <typename U>
    void destroy(U* p) noexcept {
        p->~U();
    }
};

// 高性能点运算结构 (16字节对齐)
struct alignas(16) OptimizedPoint2D {
    float x, y;    // 8字节
    float pad[2];  // 填充到16字节对齐

    // 默认构造函数
    constexpr OptimizedPoint2D() noexcept : x(0.0f), y(0.0f), pad{0, 0} {}

    // 构造函数
    constexpr OptimizedPoint2D(float x_val, float y_val) noexcept : x(x_val), y(y_val), pad{0, 0} {}

    // SIMD优化的距离计算
    float DistanceTo(const OptimizedPoint2D& other) const noexcept {
        return boost::geometry::distance(*this, other);
    }

    // 标准距离计算（用于兼容性）
    float DistanceToStandard(const OptimizedPoint2D& other) const noexcept {
        return boost::geometry::distance(*this, other);
    }

    // 向量加法
    OptimizedPoint2D operator+(const OptimizedPoint2D& other) const noexcept {
        __m128 a = _mm_load_ps(&x);
        __m128 b = _mm_load_ps(&other.x);
        __m128 result = _mm_add_ps(a, b);

        OptimizedPoint2D output;
        _mm_store_ps(&output.x, result);
        return output;
    }

    // 向量减法
    OptimizedPoint2D operator-(const OptimizedPoint2D& other) const noexcept {
        __m128 a = _mm_load_ps(&x);
        __m128 b = _mm_load_ps(&other.x);
        __m128 result = _mm_sub_ps(a, b);

        OptimizedPoint2D output;
        _mm_store_ps(&output.x, result);
        return output;
    }

    // 标量乘法
    OptimizedPoint2D operator*(float scalar) const noexcept {
        __m128 a = _mm_load_ps(&x);
        __m128 s = _mm_set1_ps(scalar);
        __m128 result = _mm_mul_ps(a, s);

        OptimizedPoint2D output;
        _mm_store_ps(&output.x, result);
        return output;
    }

    // 内积运算
    float Dot(const OptimizedPoint2D& other) const noexcept {
        __m128 a = _mm_load_ps(&x);
        __m128 b = _mm_load_ps(&other.x);
        __m128 mul = _mm_mul_ps(a, b);

        __m128 sum = _mm_hadd_ps(mul, mul);
        sum = _mm_hadd_ps(sum, sum);

        float result;
        _mm_store_ss(&result, sum);
        return result;
    }

    // 长度计算
    float Length() const noexcept {
        __m128 a = _mm_load_ps(&x);
        __m128 sq = _mm_mul_ps(a, a);

        __m128 sum = _mm_hadd_ps(sq, sq);
        sum = _mm_hadd_ps(sum, sum);

        float result;
        _mm_store_ss(&result, _mm_sqrt_ss(sum));
        return result;
    }

    // 归一化
    OptimizedPoint2D Normalize() const noexcept {
        float length = Length();
        if (length > 1e-6f) {
            return *this * (1.0f / length);
        }
        return OptimizedPoint2D();
    }
};

// 高性能轨迹点
struct alignas(16) OptimizedTrajectoryPoint {
    OptimizedPoint2D position;
    float velocity;
    float acceleration;
    float timestamp;
    float pad[3];  // 对齐到16字节边界

    OptimizedTrajectoryPoint() noexcept : velocity(0.0f), acceleration(0.0f), timestamp(0.0f), pad{0, 0, 0} {}

    OptimizedTrajectoryPoint(const OptimizedPoint2D& pos, float vel, float accel, float time) noexcept
        : position(pos), velocity(vel), acceleration(accel), timestamp(time), pad{0, 0, 0} {}
};

// 内存池 - 用于减少动态分配
template <typename T, std::size_t BlockSize = 1024>
class MemoryPool {
   public:
    MemoryPool() : current_block_(nullptr), current_index_(0), block_count_(0) {}

    ~MemoryPool() {
        for (auto* block : blocks_) {
            _aligned_free(block);
        }
    }

    // 分配内存
    T* Allocate() {
        if (!current_block_ || current_index_ >= BlockSize) {
            AllocateNewBlock();
        }

        T* result = &current_block_[current_index_++];
        new (result) T();  // 调用构造函数
        return result;
    }

    // 重置池（调用析构函数）
    void Reset() {
        for (auto* block : blocks_) {
            for (std::size_t i = 0; i < BlockSize; ++i) {
                block[i].~T();
            }
        }

        current_block_ = (blocks_.empty()) ? nullptr : blocks_[0];
        current_index_ = 0;
    }

    // 获取已分配对象数量
    std::size_t AllocatedCount() const noexcept {
        return block_count_ * BlockSize + current_index_;
    }

   private:
    T* current_block_;
    std::size_t current_index_;
    std::size_t block_count_;
    std::vector<T*> blocks_;

    void AllocateNewBlock() {
        T* new_block = static_cast<T*>(_aligned_malloc(BlockSize * sizeof(T), 16));
        if (!new_block) throw std::bad_alloc();

        blocks_.push_back(new_block);
        current_block_ = new_block;
        current_index_ = 0;
        ++block_count_;
    }
};

// 类型别名
using AlignedVector2D = std::vector<OptimizedPoint2D, AlignedAllocator<OptimizedPoint2D>>;
using AlignedTrajectory = std::vector<OptimizedTrajectoryPoint, AlignedAllocator<OptimizedTrajectoryPoint>>;

}  // namespace Optimized
}  // namespace Siligen

BOOST_GEOMETRY_REGISTER_POINT_2D(Siligen::Optimized::OptimizedPoint2D,
                                 float,
                                 boost::geometry::cs::cartesian,
                                 x,
                                 y)
