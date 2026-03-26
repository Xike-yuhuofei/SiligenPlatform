/**
 * @file CMPValidator.h
 * @brief CMP配置验证器
 * @details 负责验证CMP触发配置的有效性
 *
 * @author Claude Code
 * @date 2025-11-24
 */

#pragma once

#include "shared/types/CMPTypes.h"
#include "shared/types/Types.h"

namespace Siligen::Domain::Motion {

/**
 * @brief CMP配置验证器
 * @details 验证CMP配置参数的合法性和有效性
 */
class CMPValidator {
   public:
    /**
     * @brief 构造函数
     */
    CMPValidator() = default;

    /**
     * @brief 析构函数
     */
    ~CMPValidator() = default;

    /**
     * @brief 验证CMP配置
     * @param cmp_config CMP配置
     * @return 验证结果
     */
    bool ValidateCMPConfiguration(const CMPConfiguration& cmp_config) const;

   private:
};

}  // namespace Siligen::Domain::Motion

