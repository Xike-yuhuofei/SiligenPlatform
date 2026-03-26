#include "shared/Encoding/EncodingCodec.h"

#include <boost/beast/core/detail/base64.hpp>

#include <cctype>
#include <iomanip>
#include <sstream>

namespace Siligen {

namespace {

bool IsUnreservedUrlChar(unsigned char ch) {
    return std::isalnum(ch) || ch == '-' || ch == '_' || ch == '.' || ch == '~';
}

int HexDigitToValue(unsigned char ch) {
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return 10 + (ch - 'a');
    }
    if (ch >= 'A' && ch <= 'F') {
        return 10 + (ch - 'A');
    }
    return -1;
}

}  // namespace

std::string EncodingCodec::UrlEncode(const std::string& str) {
    if (str.empty()) {
        return "";
    }

    std::ostringstream encoded;
    encoded << std::uppercase << std::hex;

    for (unsigned char ch : str) {
        if (IsUnreservedUrlChar(ch)) {
            encoded << static_cast<char>(ch);
            continue;
        }

        encoded << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(ch);
    }

    return encoded.str();
}

std::string EncodingCodec::UrlDecode(const std::string& str) {
    if (str.empty()) {
        return "";
    }

    std::string result;
    result.reserve(str.size());
    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '%' && i + 2 < str.length()) {
            const int high = HexDigitToValue(static_cast<unsigned char>(str[i + 1]));
            const int low = HexDigitToValue(static_cast<unsigned char>(str[i + 2]));
            if (high >= 0 && low >= 0) {
                result += static_cast<char>((high << 4) | low);
                i += 2;
                continue;
            }

            result += str[i];
        } else if (str[i] == '+') {
            result += ' ';
        } else {
            result += str[i];
        }
    }
    return result;
}

std::string EncodingCodec::Base64Encode(const uint8* data, int32 length) {
    if (!data || length <= 0) {
        return "";
    }

    const std::size_t input_len = static_cast<std::size_t>(length);
    std::string result;
    result.resize(boost::beast::detail::base64::encoded_size(input_len));

    std::size_t written = boost::beast::detail::base64::encode(result.data(), data, input_len);
    result.resize(written);
    return result;
}

std::vector<uint8> EncodingCodec::Base64Decode(const std::string& str) {
    std::vector<uint8> output;
    if (str.empty()) {
        return output;
    }

    std::string cleaned;
    cleaned.reserve(str.size());
    for (unsigned char c : str) {
        if (!std::isspace(c)) {
            cleaned.push_back(static_cast<char>(c));
        }
    }

    if (cleaned.empty()) {
        return output;
    }

    output.resize(boost::beast::detail::base64::decoded_size(cleaned.size()));
    auto result = boost::beast::detail::base64::decode(output.data(), cleaned.data(), cleaned.size());
    output.resize(result.first);
    return output;
}

}  // namespace Siligen

