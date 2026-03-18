// CMPTestPresetService.h
// 版本: 1.0.0
// 描述: CMP测试预设服务 - 硬编码版本 (MVP)
// 任务: CMP测试默认数据功能
// 说明: 当前使用硬编码预设，后续可升级为JSON文件加载

#pragma once

#include "domain/diagnostics/ports/ICMPTestPresetPort.h"
#include "shared/types/CMPTestPresetTypes.h"

#include <map>

namespace Siligen {
namespace Infrastructure {
namespace Services {

using Siligen::Domain::Diagnostics::Ports::ICMPTestPresetPort;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::CMPTestPreset;
using Siligen::Shared::Types::PresetCategory;

/**
 * @brief CMP测试预设服务实现
 * @details 提供预设数据管理功能
 *
 * 当前实现: 硬编码3个基础预设
 * 后续升级: 从JSON文件加载(resources/config/cmp_test_presets.json)
 */
class CMPTestPresetService : public ICMPTestPresetPort {
   public:
    /**
     * @brief 构造函数
     */
    CMPTestPresetService();

    /**
     * @brief 析构函数
     */
    ~CMPTestPresetService() override = default;

    // ========== ICMPTestPresetPort实现 ==========

    Result<std::vector<CMPTestPreset>> loadAllPresets() override;

    Result<CMPTestPreset> getPresetByName(const std::string& name) override;

    std::vector<CMPTestPreset> listPresetsByCategory(PresetCategory category) override;

    Result<void> validatePreset(const CMPTestPreset& preset) override;

    size_t getPresetCount() const override;

   private:
    /**
     * @brief 初始化硬编码预设
     */
    void initializePresets();

    /**
     * @brief 创建"简单直线点胶"预设
     */
    CMPTestPreset createSimpleLinearPreset();

    /**
     * @brief 创建"圆弧轨迹点胶"预设
     */
    CMPTestPreset createCircularArcPreset();

    /**
     * @brief 创建"高速精度测试"预设
     */
    CMPTestPreset createHighSpeedPreset();

    // ========== 内部数据 ==========

    std::map<std::string, CMPTestPreset> presets_;  // 预设存储 (按名称索引)
    bool initialized_;                              // 初始化标志
};

}  // namespace Services
}  // namespace Infrastructure
}  // namespace Siligen



