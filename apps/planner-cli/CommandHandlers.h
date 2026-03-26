#pragma once

#include "CommandLineParser.h"
#include "container/ApplicationContainer.h"
#include "shared/types/AxisTypes.h"
#include "shared/types/Result.h"

#include <memory>
#include <string>
#include <vector>

namespace Siligen::Adapters::CLI {

class CLICommandHandlers {
   public:
    explicit CLICommandHandlers(std::shared_ptr<Siligen::Application::Container::ApplicationContainer> container);

    int Execute(const Siligen::Application::CommandLineConfig& config);
    int RunInteractive();

   private:
    std::shared_ptr<Siligen::Application::Container::ApplicationContainer> container_;

    Siligen::Shared::Types::Result<void> EnsureConnected(const Siligen::Application::CommandLineConfig& config);
    Siligen::Shared::Types::Result<void> EnsureAxesHomed(
        const std::vector<Siligen::Shared::Types::LogicalAxisId>& axes);
    Siligen::Shared::Types::Result<std::vector<Siligen::Shared::Types::LogicalAxisId>> GetNotHomedAxes(
        const std::vector<Siligen::Shared::Types::LogicalAxisId>& axes);
    Siligen::Shared::Types::Result<void> ConnectHardware(
        const Siligen::Application::CommandLineConfig& config,
        bool verbose_output);
    void ShowConnectionResult(const Siligen::Shared::Types::Result<void>& result) const;
    void PrintError(const Siligen::Shared::Types::Error& error) const;
    void PrintHomingStatusDiagnostics(
        const std::vector<Siligen::Shared::Types::LogicalAxisId>& axes) const;
    Siligen::Shared::Types::Result<void> WaitForPurgeKey(
        const Siligen::Application::CommandLineConfig& config,
        bool wait_for_release);

    int HandleConnect(const Siligen::Application::CommandLineConfig& config);
    int HandleDisconnect(const Siligen::Application::CommandLineConfig& config);
    int HandleStatus(const Siligen::Application::CommandLineConfig& config);
    int HandleHome(const Siligen::Application::CommandLineConfig& config);
    int HandleJog(const Siligen::Application::CommandLineConfig& config);
    int HandleMove(const Siligen::Application::CommandLineConfig& config);
    int HandleStopAll(const Siligen::Application::CommandLineConfig& config);
    int HandleEmergencyStop(const Siligen::Application::CommandLineConfig& config);
    int HandleDispenser(const Siligen::Application::CommandLineConfig& config);
    int HandleSupply(const Siligen::Application::CommandLineConfig& config);
    int HandleDXFPlan(const Siligen::Application::CommandLineConfig& config);
    int HandleDXFDispense(const Siligen::Application::CommandLineConfig& config);
    int HandleDXFPreview(const Siligen::Application::CommandLineConfig& config);
    int HandleDXFAugment(const Siligen::Application::CommandLineConfig& config);
    int HandleRecipeCreate(const Siligen::Application::CommandLineConfig& config);
    int HandleRecipeUpdate(const Siligen::Application::CommandLineConfig& config);
    int HandleRecipeDraftCreate(const Siligen::Application::CommandLineConfig& config);
    int HandleRecipeDraftUpdate(const Siligen::Application::CommandLineConfig& config);
    int HandleRecipePublish(const Siligen::Application::CommandLineConfig& config);
    int HandleRecipeList(const Siligen::Application::CommandLineConfig& config);
    int HandleRecipeGet(const Siligen::Application::CommandLineConfig& config);
    int HandleRecipeListVersions(const Siligen::Application::CommandLineConfig& config);
    int HandleRecipeArchive(const Siligen::Application::CommandLineConfig& config);
    int HandleRecipeVersionCreate(const Siligen::Application::CommandLineConfig& config);
    int HandleRecipeCompare(const Siligen::Application::CommandLineConfig& config);
    int HandleRecipeActivate(const Siligen::Application::CommandLineConfig& config);
    int HandleRecipeAudit(const Siligen::Application::CommandLineConfig& config);
    int HandleRecipeExport(const Siligen::Application::CommandLineConfig& config);
    int HandleRecipeImport(const Siligen::Application::CommandLineConfig& config);

    Siligen::Application::CommandLineConfig ApplyDxfDefaults(
        const Siligen::Application::CommandLineConfig& config) const;
    std::string LoadDefaultDxfFilePath(const Siligen::Application::CommandLineConfig& config) const;

    void PrintHeader() const;
    void Pause() const;
};

}  // namespace Siligen::Adapters::CLI
