#pragma once

#include <memory>
#include <string>

namespace Siligen::Domain::Configuration::Ports {
class IConfigurationPort;
}

namespace Siligen::Shared::Types {
template <typename T>
class Result;
}

namespace Siligen::Application::Services::DXF {

using Siligen::Shared::Types::Result;

/**
 * @brief 确保 DXF 对应的运行时 PB 中间文件已就绪
 *
 * 该服务统一封装：
 * - 环境变量覆盖
 * - DXF 预处理配置读取
 * - 外部 DXF 算法仓库发现（优先使用独立 DXF 项目）
 * - Python 脚本路径解析
 * - DXF/PB 时间戳比较
 * - 预处理输出基础校验
 */
class DxfPbPreparationService {
   public:
    explicit DxfPbPreparationService(
        std::shared_ptr<Siligen::Domain::Configuration::Ports::IConfigurationPort> config_port = nullptr);

    Result<std::string> EnsurePbReady(const std::string& filepath) const;

   private:
    std::shared_ptr<Siligen::Domain::Configuration::Ports::IConfigurationPort> config_port_;
};

}  // namespace Siligen::Application::Services::DXF
