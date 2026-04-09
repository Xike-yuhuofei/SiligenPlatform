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

class ICMPTestPresetPort {
   public:
    virtual ~ICMPTestPresetPort() = default;

    virtual Result<std::vector<CMPTestPreset>> loadAllPresets() = 0;
    virtual Result<CMPTestPreset> getPresetByName(const std::string& name) = 0;
    virtual std::vector<CMPTestPreset> listPresetsByCategory(PresetCategory category) = 0;
    virtual Result<void> validatePreset(const CMPTestPreset& preset) = 0;
    virtual size_t getPresetCount() const = 0;
};

}  // namespace Ports
}  // namespace Diagnostics
}  // namespace Domain
}  // namespace Siligen
