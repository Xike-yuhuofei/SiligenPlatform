#include "shared/Strings/StringToolkit.h"

namespace Siligen {

namespace {
using StringManipulator = Siligen::SharedKernel::StringManipulator;
}

// 类型转换委托
int32 StringToolkit::StringToInt(const std::string& str, int32 default_value) {
    return TypeConverter::StringToInt(str, default_value);
}

float32 StringToolkit::StringToFloat(const std::string& str, float32 default_value) {
    return TypeConverter::StringToFloat(str, default_value);
}

bool StringToolkit::StringToBool(const std::string& str, bool default_value) {
    return TypeConverter::StringToBool(str, default_value);
}

std::string StringToolkit::IntToString(int32 value) {
    return TypeConverter::IntToString(value);
}

std::string StringToolkit::FloatToString(float32 value, int32 precision) {
    return TypeConverter::FloatToString(value, precision);
}

std::string StringToolkit::BoolToString(bool value) {
    return TypeConverter::BoolToString(value);
}

// 字符串处理委托
std::string StringToolkit::Trim(const std::string& str) {
    return StringManipulator::Trim(str);
}

std::string StringToolkit::TrimLeft(const std::string& str) {
    return StringManipulator::TrimLeft(str);
}

std::string StringToolkit::TrimRight(const std::string& str) {
    return StringManipulator::TrimRight(str);
}

std::string StringToolkit::ToLower(const std::string& str) {
    return StringManipulator::ToLower(str);
}

std::string StringToolkit::ToUpper(const std::string& str) {
    return StringManipulator::ToUpper(str);
}

std::string StringToolkit::Replace(const std::string& str, const std::string& from, const std::string& to) {
    return StringManipulator::Replace(str, from, to);
}

std::string StringToolkit::ReplaceAll(const std::string& str, const std::string& from, const std::string& to) {
    return StringManipulator::ReplaceAll(str, from, to);
}

std::vector<std::string> StringToolkit::Split(const std::string& str, const std::string& delimiter, int32 max_split) {
    return StringManipulator::Split(str, delimiter, max_split);
}

std::vector<std::string> StringToolkit::SplitLines(const std::string& str) {
    return StringManipulator::SplitLines(str);
}

std::string StringToolkit::Join(const std::vector<std::string>& strings, const std::string& separator) {
    return StringManipulator::Join(strings, separator);
}

std::string StringToolkit::PadLeft(const std::string& str, int32 width, char fill_char) {
    return StringManipulator::PadLeft(str, width, fill_char);
}

std::string StringToolkit::PadRight(const std::string& str, int32 width, char fill_char) {
    return StringManipulator::PadRight(str, width, fill_char);
}

// 字符串检查委托
bool StringToolkit::IsEmpty(const std::string& str) {
    return StringValidator::IsEmpty(str);
}

bool StringToolkit::IsWhitespace(const std::string& str) {
    return StringValidator::IsWhitespace(str);
}

bool StringToolkit::StartsWith(const std::string& str, const std::string& prefix, bool ignore_case) {
    return StringValidator::StartsWith(str, prefix, ignore_case);
}

bool StringToolkit::EndsWith(const std::string& str, const std::string& suffix, bool ignore_case) {
    return StringValidator::EndsWith(str, suffix, ignore_case);
}

bool StringToolkit::Contains(const std::string& str, const std::string& substring, bool ignore_case) {
    return StringValidator::Contains(str, substring, ignore_case);
}

int32 StringToolkit::Find(const std::string& str, const std::string& substring, int32 start_pos, bool ignore_case) {
    return StringValidator::Find(str, substring, start_pos, ignore_case);
}

// 格式化委托
std::string StringToolkit::FormatNumber(float32 value, int32 decimal_places) {
    return StringFormatter::FormatNumber(value, decimal_places);
}

std::string StringToolkit::FormatScientific(float32 value, int32 precision) {
    return StringFormatter::FormatScientific(value, precision);
}

std::string StringToolkit::FormatFileSize(int64 bytes) {
    return StringFormatter::FormatFileSize(bytes);
}

std::string StringToolkit::FormatDuration(float64 seconds) {
    return StringFormatter::FormatDuration(seconds);
}

// 编码转换委托
std::string StringToolkit::UrlEncode(const std::string& str) {
    return EncodingCodec::UrlEncode(str);
}

std::string StringToolkit::UrlDecode(const std::string& str) {
    return EncodingCodec::UrlDecode(str);
}

std::string StringToolkit::Base64Encode(const uint8* data, int32 length) {
    return EncodingCodec::Base64Encode(data, length);
}

std::vector<uint8> StringToolkit::Base64Decode(const std::string& str) {
    return EncodingCodec::Base64Decode(str);
}

// 哈希和校验委托
uint32 StringToolkit::GetHashCode(const std::string& str) {
    return HashCalculator::GetHashCode(str);
}

uint32 StringToolkit::CalculateCRC32(const uint8* data, int32 length) {
    return HashCalculator::CalculateCRC32(data, length);
}

uint32 StringToolkit::CalculateCRC32(const std::string& str) {
    return HashCalculator::CalculateCRC32(str);
}

// 路径处理委托
std::string StringToolkit::NormalizePath(const std::string& path) {
    return PathOperations::NormalizePath(path);
}

std::string StringToolkit::GetFileExtension(const std::string& filename) {
    return PathOperations::GetFileExtension(filename);
}

std::string StringToolkit::GetFileName(const std::string& filepath) {
    return PathOperations::GetFileName(filepath);
}

std::string StringToolkit::GetDirectoryPath(const std::string& filepath) {
    return PathOperations::GetDirectoryPath(filepath);
}

std::string StringToolkit::CombinePath(const std::string& path1, const std::string& path2) {
    return PathOperations::CombinePath(path1, path2);
}

}  // namespace Siligen
