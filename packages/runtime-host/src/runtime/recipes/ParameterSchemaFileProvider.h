#pragma once

#include "domain/recipes/ports/IParameterSchemaPort.h"

#include <mutex>
#include <string>

namespace Siligen::Infrastructure::Adapters::Recipes {

using Siligen::Domain::Recipes::ValueObjects::ParameterSchema;

class ParameterSchemaFileProvider : public Siligen::Domain::Recipes::Ports::IParameterSchemaPort {
   public:
    explicit ParameterSchemaFileProvider(const std::string& primary_directory,
                                         const std::string& fallback_directory = "");

    ~ParameterSchemaFileProvider() override = default;

    ParameterSchemaFileProvider(const ParameterSchemaFileProvider&) = delete;
    ParameterSchemaFileProvider& operator=(const ParameterSchemaFileProvider&) = delete;
    ParameterSchemaFileProvider(ParameterSchemaFileProvider&&) = delete;
    ParameterSchemaFileProvider& operator=(ParameterSchemaFileProvider&&) = delete;

    Siligen::Shared::Types::Result<ParameterSchema> GetSchemaById(const std::string& schema_id) const override;
    Siligen::Shared::Types::Result<ParameterSchema> GetDefaultSchema() const override;
    Siligen::Shared::Types::Result<std::vector<ParameterSchema>> ListSchemas() const override;

   private:
    std::string primary_directory_;
    std::string fallback_directory_;
    mutable std::mutex mutex_;
};

}  // namespace Siligen::Infrastructure::Adapters::Recipes
