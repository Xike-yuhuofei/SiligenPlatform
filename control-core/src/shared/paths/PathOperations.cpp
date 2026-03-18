#include "shared/Paths/PathOperations.h"

#include <filesystem>

namespace Siligen {

std::string PathOperations::NormalizePath(const std::string& path) {
    if (path.empty()) {
        return "";
    }
    std::filesystem::path p(path);
    p.make_preferred();
    return p.string();
}

std::string PathOperations::GetFileExtension(const std::string& filename) {
    if (filename.empty()) {
        return "";
    }
    std::filesystem::path p(filename);
    return p.has_extension() ? p.extension().string() : "";
}

std::string PathOperations::GetFileName(const std::string& filepath) {
    if (filepath.empty()) {
        return "";
    }
    char last_char = filepath.back();
    if (last_char == '\\' || last_char == '/') {
        return filepath;
    }
    std::filesystem::path p(filepath);
    std::string name = p.filename().string();
    return name.empty() ? filepath : name;
}

std::string PathOperations::GetDirectoryPath(const std::string& filepath) {
    if (filepath.empty()) {
        return "";
    }
    std::filesystem::path p(filepath);
    std::filesystem::path parent = p.parent_path();
    return parent.empty() ? "" : parent.string();
}

std::string PathOperations::CombinePath(const std::string& path1, const std::string& path2) {
    if (path1.empty()) return path2;
    if (path2.empty()) return path1;

    char last_char = path1.back();
    if (last_char == '\\' || last_char == '/') {
        return path1 + path2;
    }
    return path1 + std::string(1, std::filesystem::path::preferred_separator) + path2;
}

}  // namespace Siligen

