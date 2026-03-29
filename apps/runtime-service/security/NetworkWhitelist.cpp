#include "NetworkWhitelist.h"

#include "SecurityLogHelper.h"

#include <boost/asio/ip/address_v4.hpp>

namespace Siligen {

using Shared::Types::LogLevel;

void NetworkWhitelist::LoadFromConfig(const std::vector<std::string>& ip_list) {
    std::lock_guard<std::mutex> lock(mutex_);
    whitelist_.clear();

    for (const auto& ip : ip_list) {
        if (ValidateIPFormat(ip)) {
            whitelist_.insert(ip);
            SecurityLogHelper::Log(LogLevel::INFO, "NetworkWhitelist", "Added IP to whitelist: " + ip);
        } else {
            SecurityLogHelper::Log(LogLevel::WARNING, "NetworkWhitelist", "Invalid IP format ignored: " + ip);
        }
    }

    SecurityLogHelper::Log(LogLevel::INFO,
                           "NetworkWhitelist",
                           "Loaded " + std::to_string(whitelist_.size()) + " IPs from config");
}

bool NetworkWhitelist::AddIP(const std::string& ip) {
    if (!ValidateIPFormat(ip)) {
        SecurityLogHelper::Log(LogLevel::WARNING, "NetworkWhitelist", "Invalid IP format: " + ip);
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    auto result = whitelist_.insert(ip);

    if (result.second) {
        SecurityLogHelper::Log(LogLevel::INFO, "NetworkWhitelist", "Added IP: " + ip);
    }

    return result.second;
}

bool NetworkWhitelist::RemoveIP(const std::string& ip) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto count = whitelist_.erase(ip);

    if (count > 0) {
        SecurityLogHelper::Log(LogLevel::INFO, "NetworkWhitelist", "Removed IP: " + ip);
    }

    return count > 0;
}

bool NetworkWhitelist::IsAllowed(const std::string& ip) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return whitelist_.find(ip) != whitelist_.end();
}

std::vector<std::string> NetworkWhitelist::GetAllIPs() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::vector<std::string>(whitelist_.begin(), whitelist_.end());
}

void NetworkWhitelist::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    whitelist_.clear();
    SecurityLogHelper::Log(LogLevel::INFO, "NetworkWhitelist", "Cleared all IPs");
}

bool NetworkWhitelist::ValidateIPFormat(const std::string& ip) {
    boost::system::error_code ec;
    boost::asio::ip::make_address_v4(ip, ec);
    return !ec;
}

}  // namespace Siligen
