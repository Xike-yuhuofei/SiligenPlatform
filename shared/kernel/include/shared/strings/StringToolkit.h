#pragma once

#include "shared/types/Types.h"
#include "shared/Encoding/EncodingCodec.h"
#include "shared/Hashing/HashCalculator.h"
#include "shared/Paths/PathOperations.h"
#include "shared/Strings/StringFormatter.h"
#include "siligen/shared/strings/string_manipulator.h"
#include "shared/Strings/StringValidator.h"
#include "shared/Conversion/TypeConverter.h"

#include <string>
#include <vector>

namespace Siligen {

class StringToolkit {
   public:
    // 类型转换
    static int32 StringToInt(const std::string& str, int32 default_value = 0);
    static float32 StringToFloat(const std::string& str, float32 default_value = 0.0f);
    static bool StringToBool(const std::string& str, bool default_value = false);
    static std::string IntToString(int32 value);
    static std::string FloatToString(float32 value, int32 precision = 6);
    static std::string BoolToString(bool value);

    // 字符串处理
    static std::string Trim(const std::string& str);
    static std::string TrimLeft(const std::string& str);
    static std::string TrimRight(const std::string& str);
    static std::string ToLower(const std::string& str);
    static std::string ToUpper(const std::string& str);
    static std::string Replace(const std::string& str, const std::string& from, const std::string& to);
    static std::string ReplaceAll(const std::string& str, const std::string& from, const std::string& to);

    // 分割和合并
    static std::vector<std::string> Split(const std::string& str, const std::string& delimiter, int32 max_split = 0);
    static std::vector<std::string> SplitLines(const std::string& str);
    static std::string Join(const std::vector<std::string>& strings, const std::string& separator);
    static std::string PadLeft(const std::string& str, int32 width, char fill_char = ' ');
    static std::string PadRight(const std::string& str, int32 width, char fill_char = ' ');

    // 字符串检查
    static bool IsEmpty(const std::string& str);
    static bool IsWhitespace(const std::string& str);
    static bool StartsWith(const std::string& str, const std::string& prefix, bool ignore_case = false);
    static bool EndsWith(const std::string& str, const std::string& suffix, bool ignore_case = false);
    static bool Contains(const std::string& str, const std::string& substring, bool ignore_case = false);
    static int32 Find(const std::string& str,
                      const std::string& substring,
                      int32 start_pos = 0,
                      bool ignore_case = false);

    // 格式化
    template <typename... Args>
    static std::string Format(const std::string& format, Args... args);
    static std::string FormatNumber(float32 value, int32 decimal_places = 2);
    static std::string FormatScientific(float32 value, int32 precision = 6);
    static std::string FormatFileSize(int64 bytes);
    static std::string FormatDuration(float64 seconds);

    // 编码转换
    static std::string UrlEncode(const std::string& str);
    static std::string UrlDecode(const std::string& str);
    static std::string Base64Encode(const uint8* data, int32 length);
    static std::vector<uint8> Base64Decode(const std::string& str);

    // 哈希和校验
    static uint32 GetHashCode(const std::string& str);
    static uint32 CalculateCRC32(const uint8* data, int32 length);
    static uint32 CalculateCRC32(const std::string& str);

    // 路径处理
    static std::string NormalizePath(const std::string& path);
    static std::string GetFileExtension(const std::string& filename);
    static std::string GetFileName(const std::string& filepath);
    static std::string GetDirectoryPath(const std::string& filepath);
    static std::string CombinePath(const std::string& path1, const std::string& path2);
};

template <typename... Args>
std::string StringToolkit::Format(const std::string& format, Args... args) {
    return StringFormatter::Format(format, args...);
}

#define STRING_FORMAT(...) Siligen::StringToolkit::Format(__VA_ARGS__)
#define STRING_TO_INT(str) Siligen::StringToolkit::StringToInt(str)
#define STRING_TO_FLOAT(str) Siligen::StringToolkit::StringToFloat(str)
#define STRING_TO_BOOL(str) Siligen::StringToolkit::StringToBool(str)
#define INT_TO_STRING(value) Siligen::StringToolkit::IntToString(value)
#define FLOAT_TO_STRING(value) Siligen::StringToolkit::FloatToString(value)
#define BOOL_TO_STRING(value) Siligen::StringToolkit::BoolToString(value)

}  // namespace Siligen


