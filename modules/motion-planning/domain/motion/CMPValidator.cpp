/**
 * @file CMPValidator.cpp
 * @brief CMP配置验证器实现
 *
 * @author Claude Code
 * @date 2025-11-24
 */

#define MODULE_NAME "CMPValidator"
#include "CMPValidator.h"

#include "shared/interfaces/ILoggingService.h"

namespace Siligen::Domain::Motion {

bool CMPValidator::ValidateCMPConfiguration(const CMPConfiguration& cmp_config) const {
    // 使用CMPConfiguration::Validate()进行统一验证
    std::string error_msg;
    if (!cmp_config.Validate(&error_msg)) {
        SILIGEN_LOG_ERROR(error_msg);
        return false;
    }

    return true;
}

}  // namespace Siligen::Domain::Motion
