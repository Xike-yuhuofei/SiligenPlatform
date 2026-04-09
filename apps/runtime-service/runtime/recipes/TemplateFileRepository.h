#pragma once

#include "recipe_lifecycle/domain/recipes/ports/ITemplateRepositoryPort.h"
#include "shared/types/Result.h"

#include "nlohmann/json.hpp"

#include <filesystem>
#include <mutex>
#include <string>

namespace Siligen::Infrastructure::Adapters::Recipes {

class TemplateFileRepository : public Siligen::Domain::Recipes::Ports::ITemplateRepositoryPort {
   public:
    explicit TemplateFileRepository(const std::string& base_directory);
    ~TemplateFileRepository() override = default;

    TemplateFileRepository(const TemplateFileRepository&) = delete;
    TemplateFileRepository& operator=(const TemplateFileRepository&) = delete;
    TemplateFileRepository(TemplateFileRepository&&) = delete;
    TemplateFileRepository& operator=(TemplateFileRepository&&) = delete;

    Siligen::Shared::Types::Result<Siligen::Domain::Recipes::ValueObjects::TemplateId> SaveTemplate(
        const Siligen::Domain::Recipes::ValueObjects::Template& template_data) override;
    Siligen::Shared::Types::Result<Siligen::Domain::Recipes::ValueObjects::Template> GetTemplateById(
        const Siligen::Domain::Recipes::ValueObjects::TemplateId& template_id) const override;
    Siligen::Shared::Types::Result<Siligen::Domain::Recipes::ValueObjects::Template> GetTemplateByName(
        const std::string& name) const override;
    Siligen::Shared::Types::Result<std::vector<Siligen::Domain::Recipes::ValueObjects::Template>> ListTemplates()
        const override;

   private:
    std::filesystem::path base_directory_;
    std::filesystem::path templates_directory_;
    mutable std::mutex mutex_;

    Siligen::Shared::Types::Result<void> EnsureDirectory() const;
    Siligen::Shared::Types::Result<void> WriteJsonFile(const std::filesystem::path& path,
                                                       const nlohmann::json& data) const;
    Siligen::Shared::Types::Result<nlohmann::json> ReadJsonFile(const std::filesystem::path& path) const;

    std::filesystem::path TemplatePath(const Siligen::Domain::Recipes::ValueObjects::TemplateId& template_id) const;
};

}  // namespace Siligen::Infrastructure::Adapters::Recipes
