#pragma once

#include "shared/types/Types.h"

#include <optional>
#include <string>
#include <vector>

namespace Siligen {

// 密码哈希工具类 (使用Windows BCrypt API)
class PasswordHasher {
   public:
    PasswordHasher() = default;

    // 生成密码哈希 (返回格式: salt$hash)
    std::optional<std::string> HashPassword(const std::string& password);

    // 验证密码
    bool VerifyPassword(const std::string& password, const std::string& stored_hash);

    // 验证密码强度
    bool ValidatePasswordStrength(const std::string& password,
                                  int32 min_length = 8,
                                  bool require_mixed_case = true,
                                  bool require_numbers = true,
                                  bool require_special_chars = false);

   private:
    // 生成随机盐值 (16字节)
    std::optional<std::vector<uint8>> GenerateSalt();

    // 计算SHA256哈希 (legacy: salt + password)
    std::optional<std::string> ComputeHash(const std::string& salt, const std::string& password);

    // 计算PBKDF2哈希 (salt + password)
    std::optional<std::vector<uint8>> ComputePBKDF2Hash(const std::vector<uint8>& salt,
                                                        const std::string& password,
                                                        uint32 iterations);

    // 辅助函数
    static std::string ToHex(const uint8* data, size_t length);
    static std::optional<std::vector<uint8>> FromHex(const std::string& hex);
};

}  // namespace Siligen

