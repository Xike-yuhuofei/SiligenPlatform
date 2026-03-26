#pragma once

#include "domain/trajectory/ports/IDXFPathSourcePort.h"
#include "AutoPathSourceAdapter.h"
#include <memory>

namespace Siligen::Infrastructure::Adapters::Parsing {

using Siligen::Domain::Trajectory::Ports::IDXFPathSourcePort;

/**
 * @brief DXF适配器工厂
 * 
 * 提供统一的DXF适配器创建接口，支持未来迁移到独立服务。
 * 当前使用本地实现，未来可切换为远程服务调用。
 */
class DXFAdapterFactory {
public:
    enum class AdapterType {
        LOCAL,      // 本地实现（当前）
        REMOTE,     // 远程服务（未来）
        MOCK        // 模拟实现（测试）
    };

    /**
     * @brief 创建DXF路径源适配器
     * 
     * 根据配置创建不同类型的适配器实例。
     * 默认使用LOCAL类型以保持向后兼容。
     */
    static std::shared_ptr<IDXFPathSourcePort> CreateDXFPathSourceAdapter(AdapterType type = AdapterType::LOCAL);

    /**
     * @brief 检查适配器健康状态
     * 
     * 验证适配器是否可用，特别是远程服务适配器。
     */
    static bool CheckAdapterHealth(const std::shared_ptr<IDXFPathSourcePort>& adapter);

    /**
     * @brief 获取当前适配器类型
     * 
     * 返回当前使用的适配器类型，用于监控和诊断。
     */
    static AdapterType GetCurrentAdapterType();

private:
    static AdapterType current_adapter_type_;
    
    // 创建本地适配器实例
    static std::shared_ptr<IDXFPathSourcePort> CreateLocalAdapter();
    
    // 创建远程适配器实例（预留接口）
    static std::shared_ptr<IDXFPathSourcePort> CreateRemoteAdapter();
    
    // 创建模拟适配器实例（测试用）
    static std::shared_ptr<IDXFPathSourcePort> CreateMockAdapter();
};

} // namespace Siligen::Infrastructure::Adapters::Parsing
