#include "shared/hashing/Sha256.h"

#include <stdexcept>

#ifdef _WIN32
#include <Windows.h>
#include <bcrypt.h>
#endif

namespace Siligen::Shared::Hashing {

namespace {

constexpr char kHexDigits[] = "0123456789abcdef";

std::string HexEncode(const std::uint8_t* data, const std::size_t size) {
    std::string encoded;
    encoded.resize(size * 2U);
    for (std::size_t index = 0; index < size; ++index) {
        encoded[index * 2U] = kHexDigits[(data[index] >> 4U) & 0x0F];
        encoded[index * 2U + 1U] = kHexDigits[data[index] & 0x0F];
    }
    return encoded;
}

}  // namespace

std::string ComputeSha256Hex(const std::vector<std::uint8_t>& payload) {
#ifdef _WIN32
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    DWORD object_size = 0;
    DWORD object_size_result = 0;
    DWORD digest_size = 0;
    DWORD digest_size_result = 0;

    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) {
        throw std::runtime_error("failed to open SHA-256 algorithm provider");
    }

    const auto close_algorithm = [&]() {
        if (algorithm != nullptr) {
            BCryptCloseAlgorithmProvider(algorithm, 0);
            algorithm = nullptr;
        }
    };

    if (BCryptGetProperty(
            algorithm,
            BCRYPT_OBJECT_LENGTH,
            reinterpret_cast<PUCHAR>(&object_size),
            sizeof(object_size),
            &object_size_result,
            0) != 0 ||
        object_size_result != sizeof(object_size)) {
        close_algorithm();
        throw std::runtime_error("failed to query SHA-256 object length");
    }

    if (BCryptGetProperty(
            algorithm,
            BCRYPT_HASH_LENGTH,
            reinterpret_cast<PUCHAR>(&digest_size),
            sizeof(digest_size),
            &digest_size_result,
            0) != 0 ||
        digest_size_result != sizeof(digest_size)) {
        close_algorithm();
        throw std::runtime_error("failed to query SHA-256 digest length");
    }

    std::vector<std::uint8_t> hash_object(object_size, 0);
    std::vector<std::uint8_t> digest(digest_size, 0);

    if (BCryptCreateHash(
            algorithm,
            &hash,
            hash_object.data(),
            static_cast<ULONG>(hash_object.size()),
            nullptr,
            0,
            0) != 0) {
        close_algorithm();
        throw std::runtime_error("failed to create SHA-256 hash");
    }

    const auto destroy_hash = [&]() {
        if (hash != nullptr) {
            BCryptDestroyHash(hash);
            hash = nullptr;
        }
    };

    if (!payload.empty() &&
        BCryptHashData(
            hash,
            const_cast<PUCHAR>(reinterpret_cast<const UCHAR*>(payload.data())),
            static_cast<ULONG>(payload.size()),
            0) != 0) {
        destroy_hash();
        close_algorithm();
        throw std::runtime_error("failed to hash payload");
    }

    if (BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0) != 0) {
        destroy_hash();
        close_algorithm();
        throw std::runtime_error("failed to finish SHA-256 hash");
    }

    destroy_hash();
    close_algorithm();
    return HexEncode(digest.data(), digest.size());
#else
    (void)payload;
    throw std::runtime_error("SHA-256 hashing is only implemented on Windows");
#endif
}

}  // namespace Siligen::Shared::Hashing
