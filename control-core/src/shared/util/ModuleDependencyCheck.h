// ModuleDependencyCheck.h
// 版本: 1.0.0
// 描述: 编译时模块依赖检查机制 - 验证六边形架构依赖方向
//
// 六边形架构 - 编译时约束检查
// 依赖方向: Infrastructure → Shared ← Application → Domain → Shared
// Task: T051 - Phase 4 建立清晰的模块边界

#pragma once

#include <string>
#include <type_traits>
#include <vector>

// 取消Windows宏定义冲突
#ifdef DOMAIN
#undef DOMAIN
#endif

namespace Siligen {
namespace Shared {
namespace Util {

/// @brief 模块层级枚举
/// @details 定义六边形架构的各个层级
enum class ModuleLayer : int {
    SHARED = 0,          // 共享层(最底层,无依赖)
    DOMAIN = 1,          // 领域层(只依赖Shared)
    APPLICATION = 2,     // 应用层(依赖Domain和Shared)
    INFRASTRUCTURE = 3   // 基础设施层(依赖Shared)
};

/// @brief 模块标签基类
/// @details 所有模块类型都应该继承此类并定义ModuleLayerValue
template <ModuleLayer Layer>
struct ModuleTag {
    static constexpr ModuleLayer ModuleLayerValue = Layer;
};

/// @brief 共享层模块标签
struct SharedModuleTag : ModuleTag<ModuleLayer::SHARED> {};

/// @brief 领域层模块标签
struct DomainModuleTag : ModuleTag<ModuleLayer::DOMAIN> {};

/// @brief 应用层模块标签
struct ApplicationModuleTag : ModuleTag<ModuleLayer::APPLICATION> {};

/// @brief 基础设施层模块标签
struct InfrastructureModuleTag : ModuleTag<ModuleLayer::INFRASTRUCTURE> {};

// ============================================================
// 模块类型萃取
// ============================================================

/// @brief 检查类型T是否定义了ModuleLayerValue
template <typename T>
class HasModuleLayer {
    template <typename U>
    static auto test(int) -> decltype(U::ModuleLayerValue, std::true_type());

    template <typename>
    static std::false_type test(...);

   public:
    static constexpr bool value = decltype(test<T>(0))::value;
};

/// @brief 获取类型T的模块层级
/// @note 如果T未定义ModuleLayerValue,默认为SHARED层
template <typename T, bool HasLayer = HasModuleLayer<T>::value>
struct GetModuleLayer {
    static constexpr ModuleLayer value = ModuleLayer::SHARED;
};

template <typename T>
struct GetModuleLayer<T, true> {
    static constexpr ModuleLayer value = T::ModuleLayerValue;
};

template <typename T>
inline constexpr ModuleLayer GetModuleLayerV = GetModuleLayer<T>::value;

// ============================================================
// 依赖关系验证
// ============================================================

/// @brief 检查依赖方向是否合法
/// @details 六边形架构依赖规则:
///   - Shared层: 无依赖(最底层)
///   - Domain层: 只能依赖Shared层
///   - Application层: 可以依赖Domain层和Shared层
///   - Infrastructure层: 只能依赖Shared层(实现接口)
///
/// @param dependent_layer 依赖者的层级
/// @param dependency_layer 被依赖者的层级
/// @return 是否合法
constexpr bool IsValidDependency(ModuleLayer dependent_layer, ModuleLayer dependency_layer) {
    // Shared层无依赖,任何层都可以依赖Shared层
    if (dependency_layer == ModuleLayer::SHARED) {
        return true;
    }

    // Shared层不能依赖其他层
    if (dependent_layer == ModuleLayer::SHARED) {
        return false;
    }

    // Domain层只能依赖Shared层
    if (dependent_layer == ModuleLayer::DOMAIN) {
        return dependency_layer == ModuleLayer::SHARED;
    }

    // Application层可以依赖Domain层和Shared层
    if (dependent_layer == ModuleLayer::APPLICATION) {
        return dependency_layer == ModuleLayer::SHARED || dependency_layer == ModuleLayer::DOMAIN;
    }

    // Infrastructure层只能依赖Shared层
    if (dependent_layer == ModuleLayer::INFRASTRUCTURE) {
        return dependency_layer == ModuleLayer::SHARED;
    }

    return false;
}

/// @brief 编译时依赖方向检查
/// @tparam Dependent 依赖者类型
/// @tparam Dependency 被依赖者类型
/// @details 使用static_assert在编译时验证依赖方向
///
/// 使用示例:
/// @code
/// // 在类定义中添加依赖检查
/// class MotionService : public DomainModuleTag {
/// private:
///     std::shared_ptr<IHardwareController> hardware_;
///
///     // 编译时检查: Domain层依赖IHardwareController(Shared层) - 合法
///     static constexpr bool dependency_check_ =
///         CheckDependency<MotionService, IHardwareController>();
/// };
/// @endcode
template <typename Dependent, typename Dependency>
constexpr bool CheckDependency() {
    constexpr ModuleLayer dependent_layer = GetModuleLayer<Dependent>::value;
    constexpr ModuleLayer dependency_layer = GetModuleLayer<Dependency>::value;

    static_assert(IsValidDependency(dependent_layer, dependency_layer),
                  "Invalid module dependency: violates hexagonal architecture rules. "
                  "Check that dependencies flow: Infrastructure->Shared, "
                  "Application->Domain->Shared.");

    return true;
}

/// @brief 验证多个依赖
/// @tparam Dependent 依赖者类型
/// @tparam Dependencies 被依赖者类型列表
template <typename Dependent, typename... Dependencies>
constexpr bool CheckDependencies() {
    return (CheckDependency<Dependent, Dependencies>() && ...);
}

// ============================================================
// 依赖检查宏
// ============================================================

/// @brief 声明模块层级
/// @param ClassName 类名
/// @param Layer 模块层级(SHARED/DOMAIN/APPLICATION/INFRASTRUCTURE)
#define DECLARE_MODULE_LAYER(ClassName, Layer) \
    static constexpr ::Siligen::Shared::Util::ModuleLayer ModuleLayerValue = ::Siligen::Shared::Util::ModuleLayer::Layer

/// @brief 检查单个依赖
/// @param Dependent 依赖者类型
/// @param Dependency 被依赖者类型
#define CHECK_DEPENDENCY(Dependent, Dependency)                                      \
    static_assert(::Siligen::Shared::Util::CheckDependency<Dependent, Dependency>(), \
                  "Invalid dependency from " #Dependent " to " #Dependency)

/// @brief 检查多个依赖
/// @param Dependent 依赖者类型
/// @param ... 被依赖者类型列表
#define CHECK_DEPENDENCIES(Dependent, ...)                                              \
    static_assert(::Siligen::Shared::Util::CheckDependencies<Dependent, __VA_ARGS__>(), \
                  "Invalid dependencies from " #Dependent)

// ============================================================
// 辅助工具
// ============================================================

/// @brief 获取模块层级名称
/// @param layer 模块层级
/// @return 层级名称字符串
constexpr const char* GetModuleLayerName(ModuleLayer layer) {
    switch (layer) {
        case ModuleLayer::SHARED:
            return "Shared";
        case ModuleLayer::DOMAIN:
            return "Domain";
        case ModuleLayer::APPLICATION:
            return "Application";
        case ModuleLayer::INFRASTRUCTURE:
            return "Infrastructure";
        default:
            return "Unknown";
    }
}

/// @brief 模块依赖检查器(运行时版本)
/// @details 提供运行时依赖关系查询能力
class ModuleDependencyChecker {
   public:
    /// @brief 检查依赖是否合法(运行时版本)
    /// @param dependent_layer 依赖者层级
    /// @param dependency_layer 被依赖者层级
    /// @return 是否合法
    static bool IsValidDependencyRuntime(ModuleLayer dependent_layer, ModuleLayer dependency_layer) {
        return IsValidDependency(dependent_layer, dependency_layer);
    }

    /// @brief 获取允许的依赖层级列表
    /// @param layer 当前层级
    /// @return 允许依赖的层级列表
    static std::vector<ModuleLayer> GetAllowedDependencies(ModuleLayer layer) {
        std::vector<ModuleLayer> allowed;

        // 所有层都可以依赖Shared层
        allowed.push_back(ModuleLayer::SHARED);

        // Application层可以依赖Domain层
        if (layer == ModuleLayer::APPLICATION) {
            allowed.push_back(ModuleLayer::DOMAIN);
        }

        return allowed;
    }

    /// @brief 打印依赖规则
    /// @return 依赖规则字符串
    static std::string GetDependencyRules() {
        return "Hexagonal Architecture Dependency Rules:\n"
               "  Shared Layer:         No dependencies (base layer)\n"
               "  Domain Layer:         Shared only\n"
               "  Application Layer:    Domain + Shared\n"
               "  Infrastructure Layer: Shared only (implements interfaces)\n"
               "\n"
               "Dependency Flow: Infrastructure -> Shared <- Application -> Domain -> Shared";
    }
};

// ============================================================
// 编译时测试用例
// ============================================================

namespace CompileTimeTests {

// 模拟类型定义
struct MockSharedType : SharedModuleTag {};
struct MockDomainType : DomainModuleTag {};
struct MockApplicationType : ApplicationModuleTag {};
struct MockInfrastructureType : InfrastructureModuleTag {};

// 合法依赖测试(这些应该编译通过)
namespace ValidDependencies {
// Domain -> Shared: 合法
constexpr bool test1 = CheckDependency<MockDomainType, MockSharedType>();

// Application -> Domain: 合法
constexpr bool test2 = CheckDependency<MockApplicationType, MockDomainType>();

// Application -> Shared: 合法
constexpr bool test3 = CheckDependency<MockApplicationType, MockSharedType>();

// Infrastructure -> Shared: 合法
constexpr bool test4 = CheckDependency<MockInfrastructureType, MockSharedType>();
}  // namespace ValidDependencies

// 非法依赖测试(取消注释会导致编译错误)
namespace InvalidDependencies {
// Domain -> Application: 非法 (依赖方向错误)
// constexpr bool test1 = CheckDependency<MockDomainType, MockApplicationType>();

// Infrastructure -> Domain: 非法 (基础设施层不能依赖领域层)
// constexpr bool test2 = CheckDependency<MockInfrastructureType, MockDomainType>();

// Shared -> Domain: 非法 (共享层不能依赖其他层)
// constexpr bool test4 = CheckDependency<MockSharedType, MockDomainType>();
}  // namespace InvalidDependencies

}  // namespace CompileTimeTests

}  // namespace Util
}  // namespace Shared
}  // namespace Siligen
