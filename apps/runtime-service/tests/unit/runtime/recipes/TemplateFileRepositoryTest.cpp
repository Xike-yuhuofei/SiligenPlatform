#include "runtime/recipes/TemplateFileRepository.h"

#include "domain/recipes/value-objects/RecipeTypes.h"
#include "shared/types/Error.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace {

using Siligen::Domain::Recipes::ValueObjects::Template;
using Siligen::Infrastructure::Adapters::Recipes::TemplateFileRepository;
using Siligen::Shared::Types::ErrorCode;

class ScopedTempRecipeDirectory {
public:
    explicit ScopedTempRecipeDirectory(const std::string& suffix) {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        root_ = std::filesystem::temp_directory_path() /
                ("siligen_template_repo_" + suffix + "_" + std::to_string(stamp));
        std::error_code ec;
        std::filesystem::create_directories(root_, ec);
    }

    ~ScopedTempRecipeDirectory() {
        std::error_code ec;
        std::filesystem::remove_all(root_, ec);
    }

    [[nodiscard]] const std::filesystem::path& root() const noexcept {
        return root_;
    }

    std::filesystem::path WriteFile(
        const std::filesystem::path& relative_path,
        const std::string& content) const {
        const auto absolute_path = root_ / relative_path;
        std::error_code ec;
        std::filesystem::create_directories(absolute_path.parent_path(), ec);
        std::ofstream out(absolute_path, std::ios::binary);
        out << content;
        out.close();
        return absolute_path;
    }

    std::string ReadFile(const std::filesystem::path& relative_path) const {
        std::ifstream in(root_ / relative_path, std::ios::binary);
        std::ostringstream buffer;
        buffer << in.rdbuf();
        return buffer.str();
    }

private:
    std::filesystem::path root_;
};

Template BuildTemplate(
    const std::string& id,
    const std::string& name,
    const std::string& description = "template") {
    Template value;
    value.id = id;
    value.name = name;
    value.description = description;
    value.created_at = 100;
    value.updated_at = 100;
    return value;
}

std::string BuildCanonicalTemplateFile(const Template& tmpl) {
    std::ostringstream out;
    out << "{\n"
        << "  \"id\": \"" << tmpl.id << "\",\n"
        << "  \"name\": \"" << tmpl.name << "\",\n"
        << "  \"description\": \"" << tmpl.description << "\",\n"
        << "  \"parameters\": [],\n"
        << "  \"createdAt\": " << tmpl.created_at << ",\n"
        << "  \"updatedAt\": " << tmpl.updated_at << "\n"
        << "}\n";
    return out.str();
}

std::string LegacyTemplateFileName() {
    return std::string("templates") + ".json";
}

}  // namespace

TEST(TemplateFileRepositoryTest, ListsCanonicalTemplatesOnly) {
    ScopedTempRecipeDirectory temp_dir("list_templates");
    temp_dir.WriteFile(
        "templates/canonical-template.json",
        BuildCanonicalTemplateFile(BuildTemplate("canonical-template", "Canonical Template")));

    TemplateFileRepository repository(temp_dir.root().string());
    const auto result = repository.ListTemplates();

    ASSERT_TRUE(result.IsSuccess());
    ASSERT_EQ(result.Value().size(), 1u);
    EXPECT_EQ(result.Value()[0].name, "Canonical Template");
}

TEST(TemplateFileRepositoryTest, ReadsCanonicalTemplateById) {
    ScopedTempRecipeDirectory temp_dir("get_by_id");
    temp_dir.WriteFile(
        "templates/template-id.json",
        BuildCanonicalTemplateFile(BuildTemplate("template-id", "Canonical Only Template")));

    TemplateFileRepository repository(temp_dir.root().string());
    const auto result = repository.GetTemplateById("template-id");

    ASSERT_TRUE(result.IsSuccess());
    EXPECT_EQ(result.Value().id, "template-id");
    EXPECT_EQ(result.Value().name, "Canonical Only Template");
}

TEST(TemplateFileRepositoryTest, SaveTemplateWritesCanonicalJsonOnly) {
    ScopedTempRecipeDirectory temp_dir("save_template_canonical_only");

    TemplateFileRepository repository(temp_dir.root().string());
    auto template_data = BuildTemplate("", "Canonical Write Path");
    const auto save_result = repository.SaveTemplate(template_data);

    ASSERT_TRUE(save_result.IsSuccess());
    const auto canonical_path = temp_dir.root() / "templates" / (save_result.Value() + ".json");
    EXPECT_TRUE(std::filesystem::exists(canonical_path));

    const auto by_id_result = repository.GetTemplateById(save_result.Value());
    ASSERT_TRUE(by_id_result.IsSuccess());
    EXPECT_EQ(by_id_result.Value().name, "Canonical Write Path");
}

TEST(TemplateFileRepositoryTest, SaveTemplateRejectsDuplicateCanonicalTemplateName) {
    ScopedTempRecipeDirectory temp_dir("save_template_duplicate");
    temp_dir.WriteFile(
        "templates/existing-template.json",
        BuildCanonicalTemplateFile(BuildTemplate("existing-id", "Bridge Template")));

    TemplateFileRepository repository(temp_dir.root().string());
    auto template_data = BuildTemplate("", "bridge template");
    const auto save_result = repository.SaveTemplate(template_data);

    ASSERT_TRUE(save_result.IsError());
    EXPECT_EQ(save_result.GetError().GetCode(), ErrorCode::RECIPE_ALREADY_EXISTS);
}

TEST(TemplateFileRepositoryTest, FailsFastWhenLegacyTemplateFileExistsDuringList) {
    ScopedTempRecipeDirectory temp_dir("legacy_present_list");
    temp_dir.WriteFile("templates/canonical-template.json", BuildCanonicalTemplateFile(BuildTemplate("canonical-id", "Canonical Template")));
    temp_dir.WriteFile(LegacyTemplateFileName(), "{\n  \"templates\": []\n}\n");

    TemplateFileRepository repository(temp_dir.root().string());
    const auto result = repository.ListTemplates();

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::FILE_IO_ERROR);
}

TEST(TemplateFileRepositoryTest, FailsFastWhenLegacyTemplateFileExistsDuringLookup) {
    ScopedTempRecipeDirectory temp_dir("legacy_present_lookup");
    temp_dir.WriteFile(LegacyTemplateFileName(), "{\n  \"templates\": []\n}\n");

    TemplateFileRepository repository(temp_dir.root().string());
    const auto result = repository.GetTemplateByName("Anything");

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::FILE_IO_ERROR);
}

TEST(TemplateFileRepositoryTest, FailsFastWhenLegacyTemplateFileExistsDuringSave) {
    ScopedTempRecipeDirectory temp_dir("legacy_present_save");
    temp_dir.WriteFile(LegacyTemplateFileName(), "{\n  \"templates\": []\n}\n");

    TemplateFileRepository repository(temp_dir.root().string());
    auto template_data = BuildTemplate("", "Fresh Template");
    const auto save_result = repository.SaveTemplate(template_data);

    ASSERT_TRUE(save_result.IsError());
    EXPECT_EQ(save_result.GetError().GetCode(), ErrorCode::FILE_IO_ERROR);
}
