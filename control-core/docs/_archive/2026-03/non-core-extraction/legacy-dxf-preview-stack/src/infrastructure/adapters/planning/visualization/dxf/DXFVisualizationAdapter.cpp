// DXFVisualizationAdapter.cpp - DXF可视化适配器实现
#include "DXFVisualizationAdapter.h"

#include "shared/types/Error.h"
#include "infrastructure/adapters/planning/visualization/dispensing/DispensingVisualizer.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>
#include <utility>

namespace Siligen {
namespace Infrastructure {
namespace Adapters {
namespace Visualization {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;
using Siligen::DispensingVisualizationConfig;

namespace {

std::string BuildUniqueTempHtmlPath() {
    namespace fs = std::filesystem;

    static std::atomic<uint64_t> counter{0};

    std::error_code ec;
    fs::path temp_dir = fs::temp_directory_path(ec);
    if (ec) {
        temp_dir = fs::current_path(ec);
        if (ec) {
            temp_dir = ".";
        }
    }

    const auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    const auto thread_hash = std::hash<std::thread::id>{}(std::this_thread::get_id());
    const uint64_t id = counter.fetch_add(1, std::memory_order_relaxed);

    std::ostringstream filename;
    filename << "dispensing_visualization_" << now << "_" << thread_hash << "_" << id << ".html";
    return (temp_dir / filename.str()).string();
}

void CleanupTempFile(const std::string& path) {
    std::error_code ec;
    std::filesystem::remove(path, ec);
}

}  // namespace

DXFVisualizationAdapter::DXFVisualizationAdapter(std::shared_ptr<DispensingVisualizer> dispensing_visualizer)
    : dispensing_visualizer_(std::move(dispensing_visualizer)) {
    if (!dispensing_visualizer_) {
        dispensing_visualizer_ = std::make_shared<DispensingVisualizer>();
    }
}

Result<std::string> DXFVisualizationAdapter::GenerateDXFVisualization(
    const std::vector<DXFSegment>& segments,
    const std::string& title) {
    (void)segments;
    (void)title;
    return Result<std::string>::Failure(
        Error(ErrorCode::NOT_IMPLEMENTED,
              "SVG static preview has been removed",
              "DXFVisualizationAdapter"));
}

Result<std::string> DXFVisualizationAdapter::GenerateTrajectoryVisualization(
    const std::vector<TrajectoryPoint>& trajectory_points,
    const std::string& title,
    const std::vector<VisualizationTriggerPoint>* planned_triggers) {
    if (!dispensing_visualizer_) {
        return Result<std::string>::Failure(
            Error(ErrorCode::ADAPTER_NOT_INITIALIZED,
                  "Dispensing visualizer not configured",
                  "DXFVisualizationAdapter"));
    }

    std::string temp_path = BuildUniqueTempHtmlPath();

    DispensingVisualizationConfig config;
    config.title = title;

    const bool success = dispensing_visualizer_->GenerateHTML(trajectory_points, temp_path, config, planned_triggers);
    if (!success) {
        return Result<std::string>::Failure(
            Error(ErrorCode::TRAJECTORY_GENERATION_FAILED,
                  "Dispensing visualization failed",
                  "DXFVisualizationAdapter"));
    }

    std::ifstream file(temp_path, std::ios::binary);
    if (!file) {
        CleanupTempFile(temp_path);
        return Result<std::string>::Failure(
            Error(ErrorCode::FILE_IO_ERROR,
                  "Cannot read generated HTML file: " + temp_path,
                  "DXFVisualizationAdapter"));
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string html_content = buffer.str();

    CleanupTempFile(temp_path);

    return Result<std::string>::Success(html_content);
}

}  // namespace Visualization
}  // namespace Adapters
}  // namespace Infrastructure
}  // namespace Siligen


