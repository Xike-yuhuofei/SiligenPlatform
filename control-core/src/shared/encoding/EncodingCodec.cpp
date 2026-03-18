#include "shared/Encoding/EncodingCodec.h"

#include <boost/url/encode.hpp>
#include <boost/url/encoding_opts.hpp>
#include <boost/url/pct_string_view.hpp>
#include <boost/url/rfc/unreserved_chars.hpp>
#include <boost/beast/core/detail/base64.hpp>

#include <cctype>
#include <sstream>

namespace Siligen {

std::string EncodingCodec::UrlEncode(const std::string& str) {
    boost::urls::encoding_opts opt;
    opt.space_as_plus = false;  // 与旧实现一致：空格编码为%20
    return boost::urls::encode(str, boost::urls::unreserved_chars, opt);
}

std::string EncodingCodec::UrlDecode(const std::string& str) {
    if (str.empty()) {
        return "";
    }

    boost::urls::encoding_opts opt;
    opt.space_as_plus = true;  // 保持对'+'的兼容

    auto pct = boost::urls::make_pct_string_view(str);
    if (!pct.has_error()) {
        return pct.value().decode(opt);
    }

    // 回退到旧逻辑，容忍非法转义
    std::string result;
    result.reserve(str.size());
    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '%' && i + 2 < str.length()) {
            int32 value = 0;
            std::istringstream iss(str.substr(i + 1, 2));
            iss >> std::hex >> value;
            result += static_cast<char>(value);
            i += 2;
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

