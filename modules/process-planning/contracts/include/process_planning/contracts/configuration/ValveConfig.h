#pragma once

namespace Siligen {
namespace Domain {
namespace Configuration {
namespace Ports {

/**
 * @brief 供胶阀配置
 *
 * 配置供胶阀使用的数字输出引脚
 */
struct ValveSupplyConfig {
    int do_card_index;  ///< 卡索引（0=主卡，1=扩展卡1）
    int do_bit_index;   ///< DO位索引（0=Y0, 1=Y1, 2=Y2, 3=Y3）

    /**
     * @brief 验证配置有效性
     * @return true 配置有效
     * @return false 配置无效
     */
    bool IsValid() const {
        return do_card_index >= 0 && do_card_index < 4 &&
               do_bit_index >= 0 && do_bit_index < 16;
    }
};

}  // namespace Ports
}  // namespace Configuration
}  // namespace Domain
}  // namespace Siligen
