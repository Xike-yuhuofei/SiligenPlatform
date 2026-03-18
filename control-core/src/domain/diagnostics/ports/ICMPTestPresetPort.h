// ICMPTestPresetPort.h
// 版本: 1.0.0
// 描述: CMP测试预设管理接口 - 领域层端口
// 任务: CMP测试默认数据功能

#pragma once

#include "shared/types/Result.h"
#include "shared/types/CMPTestPresetTypes.h"

#include <string>
#include <vector>

namespace Siligen {
namespace Domain {
namespace Diagnostics {
namespace Ports {

using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::CMPTestPreset;
using Siligen::Shared::Types::PresetCategory;

/**
 * @brief CMP测试预设管理接口
 * @details 定义预设的加载、查询和管理操作
 *
 * 职责:
 * - 从配置文件加载预设数据
 * - 提供预设查询功能(按名称/分类)
 * - 验证预设有效性
 *
 * 实现者:
 * - CMPTestPresetService - JSON文件实现（后续建议收口到 modules/device-hal/src/adapters/diagnostics/health/）
 * - MockPresetService (tests/mocks/) - 内存Mock实现
 *
 * 消费者:
 * - CMPTestUseCase (application/usecases/) - 测试执行
 */
class ICMPTestPresetPort {
   public:
    virtual ~ICMPTestPresetPort() = default;

    /**
     * @brief 加载所有预设
     * @return 预设列表或错误
     */
    virtual Result<std::vector<CMPTestPreset>> loadAllPresets() = 0;

    /**
     * @brief 按名称获取预设
     * @param name 预设名称
     * @return 预设对象或错误
     */
    virtual Result<CMPTestPreset> getPresetByName(const std::string& name) = 0;

    /**
     * @brief 按分类列出预设
     * @param category 分类标签
     * @return 预设列表
     */
    virtual std::vector<CMPTestPreset> listPresetsByCategory(PresetCategory category) = 0;

    /**
     * @brief 验证预设有效性
     * @param preset 预设对象
     * @return 验证结果
     */
    virtual Result<void> validatePreset(const CMPTestPreset& preset) = 0;

    /**
     * @brief 获取预设数量
     * @return 预设总数
     */
    virtual size_t getPresetCount() const = 0;
};

}  // namespace Ports
}  // namespace Diagnostics
}  // namespace Domain
}  // namespace Siligen



