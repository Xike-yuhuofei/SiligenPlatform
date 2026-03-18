#include "PasswordHasher.h"

#include <windows.h>
#include <bcrypt.h>
#include <cctype>
#include <iomanip>
#include <random>
#include <sstream>

#pragma comment(lib, "bcrypt.lib")

namespace Siligen {

std::optional<std::string> PasswordHasher::HashPassword(const std::string& password) {
    constexpr uint32 kIterations = 100000;

    auto salt = GenerateSalt();
    if (!salt.has_value()) return std::nullopt;

    auto hash = ComputePBKDF2Hash(*salt, password, kIterations);
    if (!hash.has_value()) return std::nullopt;

    std::string salt_hex = ToHex(salt->data(), salt->size());
    std::string hash_hex = ToHex(hash->data(), hash->size());

    return "pbkdf2$" + std::to_string(kIterations) + "$" + salt_hex + "$" + hash_hex;
}

bool PasswordHasher::VerifyPassword(const std::string& password, const std::string& stored_hash) {
    if (stored_hash.empty()) {
        return false;
    }

    std::vector<std::string> parts;
    size_t start = 0;
    while (true) {
        size_t pos = stored_hash.find('$', start);
        if (pos == std::string::npos) {
            parts.push_back(stored_hash.substr(start));
            break;
        }
        parts.push_back(stored_hash.substr(start, pos - start));
        start = pos + 1;
    }

    if (parts.size() == 4 && parts[0] == "pbkdf2") {
        uint32 iterations = 0;
        try {
            iterations = static_cast<uint32>(std::stoul(parts[1]));
        } catch (...) {
            return false;
        }
        auto salt_bytes = FromHex(parts[2]);
        if (!salt_bytes.has_value()) return false;

        auto hash = ComputePBKDF2Hash(*salt_bytes, password, iterations);
        if (!hash.has_value()) return false;

        std::string hash_hex = ToHex(hash->data(), hash->size());
        return hash_hex == parts[3];
    }

    // Legacy format: salt$hash
    if (parts.size() == 2) {
        const std::string& salt = parts[0];
        const std::string& stored = parts[1];
        auto hash = ComputeHash(salt, password);
        if (!hash.has_value()) return false;
        return *hash == stored;
    }

    return false;
}

bool PasswordHasher::ValidatePasswordStrength(const std::string& password,
                                              int32 min_length,
                                              bool require_mixed_case,
                                              bool require_numbers,
                                              bool require_special_chars) {
    if (static_cast<int32>(password.length()) < min_length) return false;

    bool has_upper = false, has_lower = false, has_digit = false, has_special = false;

    // 使用 unsigned char 避免 UTF-8 字符导致的负值传入字符分类函数
    for (unsigned char c : password) {
        if (std::isupper(c))
            has_upper = true;
        else if (std::islower(c))
            has_lower = true;
        else if (std::isdigit(c))
            has_digit = true;
        else
            has_special = true;
    }

    if (require_mixed_case && !(has_upper && has_lower)) return false;
    if (require_numbers && !has_digit) return false;
    if (require_special_chars && !has_special) return false;

    return true;
}

std::optional<std::vector<uint8>> PasswordHasher::GenerateSalt() {
    BCRYPT_ALG_HANDLE hAlg = NULL;
    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_RNG_ALGORITHM, NULL, 0) != 0) {
        return std::nullopt;
    }

    constexpr size_t SALT_LENGTH = 16;
    std::vector<uint8> salt_bytes(SALT_LENGTH);

    NTSTATUS status = BCryptGenRandom(hAlg, salt_bytes.data(), static_cast<ULONG>(salt_bytes.size()), 0);
    BCryptCloseAlgorithmProvider(hAlg, 0);

    if (status != 0) return std::nullopt;

    return salt_bytes;
}

std::optional<std::string> PasswordHasher::ComputeHash(const std::string& salt, const std::string& password) {
    BCRYPT_ALG_HANDLE hAlg = NULL;
    BCRYPT_HASH_HANDLE hHash = NULL;

    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, NULL, 0) != 0) {
        return std::nullopt;
    }

    DWORD hash_obj_size = 0, result_size = 0;
    BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PBYTE)&hash_obj_size, sizeof(DWORD), &result_size, 0);

    std::vector<uint8> hash_obj(hash_obj_size);

    if (BCryptCreateHash(hAlg, &hHash, hash_obj.data(), hash_obj_size, NULL, 0, 0) != 0) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return std::nullopt;
    }

    std::string input = salt + password;
    BCryptHashData(hHash, (PBYTE)input.data(), (ULONG)input.size(), 0);

    constexpr size_t HASH_LENGTH = 32;  // SHA256 = 256 bits = 32 bytes
    uint8 hash_bytes[HASH_LENGTH];

    NTSTATUS status = BCryptFinishHash(hHash, hash_bytes, HASH_LENGTH, 0);

    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);

    if (status != 0) return std::nullopt;

    return ToHex(hash_bytes, HASH_LENGTH);
}

std::optional<std::vector<uint8>> PasswordHasher::ComputePBKDF2Hash(
    const std::vector<uint8>& salt,
    const std::string& password,
    uint32 iterations) {
    BCRYPT_ALG_HANDLE hAlg = NULL;
    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, NULL, 0) != 0) {
        return std::nullopt;
    }

    constexpr size_t HASH_LENGTH = 32;  // SHA256 = 256 bits = 32 bytes
    std::vector<uint8> hash_bytes(HASH_LENGTH);

    NTSTATUS status = BCryptDeriveKeyPBKDF2(
        hAlg,
        reinterpret_cast<PUCHAR>(const_cast<char*>(password.data())),
        static_cast<ULONG>(password.size()),
        const_cast<PUCHAR>(salt.data()),
        static_cast<ULONG>(salt.size()),
        iterations,
        hash_bytes.data(),
        static_cast<ULONG>(hash_bytes.size()),
        0);

    BCryptCloseAlgorithmProvider(hAlg, 0);

    if (status != 0) return std::nullopt;

    return hash_bytes;
}

std::string PasswordHasher::ToHex(const uint8* data, size_t length) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < length; ++i) {
        oss << std::setw(2) << static_cast<int32>(data[i]);
    }
    return oss.str();
}

std::optional<std::vector<uint8>> PasswordHasher::FromHex(const std::string& hex) {
    if (hex.length() % 2 != 0) return std::nullopt;

    std::vector<uint8> bytes;
    bytes.reserve(hex.length() / 2);

    for (size_t i = 0; i < hex.length(); i += 2) {
        std::string byte_str = hex.substr(i, 2);
        uint8 byte = static_cast<uint8>(std::stoi(byte_str, nullptr, 16));
        bytes.push_back(byte);
    }

    return bytes;
}

}  // namespace Siligen
