#pragma once

#include <string>

namespace Siligen {

class PathOperations {
   public:
    static std::string NormalizePath(const std::string& path);
    static std::string GetFileExtension(const std::string& filename);
    static std::string GetFileName(const std::string& filepath);
    static std::string GetDirectoryPath(const std::string& filepath);
    static std::string CombinePath(const std::string& path1, const std::string& path2);
};

}  // namespace Siligen

