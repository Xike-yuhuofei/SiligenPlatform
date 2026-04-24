#pragma once

#include "shared/types/Error.h"
#include "shared/types/Result.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace Siligen::Application::UseCases::Dispensing {

class VelocityTracePathPolicy {
   public:
    static Shared::Types::Result<std::string> ResolveOutputPath(
        const std::string& source_path,
        const std::string& configured_path);

    static Shared::Types::Result<void> PrepareOutputFile(
        const std::string& output_path,
        std::ofstream& file);

   private:
    static Shared::Types::Result<std::filesystem::path> BuildBaseDirectory(const std::string& source_path);
    static Shared::Types::Result<std::filesystem::path> NormalizeCandidatePath(
        const std::filesystem::path& base_dir,
        const std::string& configured_path,
        const std::string& source_stem);
    static bool IsWithinBaseDirectory(
        const std::filesystem::path& base_dir,
        const std::filesystem::path& candidate) noexcept;
};

}  // namespace Siligen::Application::UseCases::Dispensing
