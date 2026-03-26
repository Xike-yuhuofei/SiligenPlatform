// SerializationTypes.h - 序列化错误类型定义
#pragma once

#include "Error.h"
#include <string>

namespace Siligen {
namespace Shared {
namespace Types {

/**
 * @brief 创建序列化错误
 */
inline Error CreateSerializationError(ErrorCode code, const std::string& message) {
    return Error(
        code,
        message,
        "Serialization"
    );
}

/**
 * @brief 便捷错误创建函数
 */
inline Error CreateJsonParseError(const std::string& message) {
    return CreateSerializationError(ErrorCode::JSON_PARSE_ERROR, message);
}

inline Error CreateMissingFieldError(const std::string& fieldName) {
    return CreateSerializationError(
        ErrorCode::JSON_MISSING_REQUIRED_FIELD,
        "Missing required field: " + fieldName
    );
}

inline Error CreateEnumConversionError(const std::string& enumName, const std::string& value) {
    return CreateSerializationError(
        ErrorCode::ENUM_CONVERSION_FAILED,
        "Invalid enum value for " + enumName + ": " + value
    );
}

}  // namespace Types
}  // namespace Shared
}  // namespace Siligen
