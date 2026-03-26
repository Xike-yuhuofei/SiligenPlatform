#pragma once

#include "shared/types/Types.h"

#include <mutex>
#include <string>
#include <unordered_set>

namespace Siligen {

// IP白名单管理器
class NetworkWhitelist {
   public:
    NetworkWhitelist() = default;

    // 从配置加载白名单
    void LoadFromConfig(const std::vector<std::string>& ip_list);

    // 添加/移除IP
    bool AddIP(const std::string& ip);
    bool RemoveIP(const std::string& ip);

    // 检查IP是否在白名单中
    bool IsAllowed(const std::string& ip) const;

    // 获取所有白名单IP
    std::vector<std::string> GetAllIPs() const;

    // 清空白名单
    void Clear();

    // 获取白名单大小
    size_t Size() const {
        return whitelist_.size();
    }

   private:
    mutable std::mutex mutex_;
    std::unordered_set<std::string> whitelist_;

    // 验证IP地址格式
    static bool ValidateIPFormat(const std::string& ip);
};

}  // namespace Siligen

