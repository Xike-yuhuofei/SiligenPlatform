#include "application/services/process_path/ProcessPathFacade.h"

namespace Siligen::Application::Services::ProcessPath {

ProcessPathBuildResult ProcessPathFacade::Build(const ProcessPathBuildRequest& request) const {
    Siligen::Domain::Trajectory::DomainServices::GeometryNormalizer normalizer;
    Siligen::Domain::Trajectory::DomainServices::ProcessAnnotator annotator;
    Siligen::Domain::Trajectory::DomainServices::TrajectoryShaper shaper;

    ProcessPathBuildResult result;
    result.normalized = normalizer.Normalize(request.primitives, request.normalization);
    result.process_path = annotator.Annotate(result.normalized.path, request.process);
    result.shaped_path = shaper.Shape(result.process_path, request.shaping);
    return result;
}

}  // namespace Siligen::Application::Services::ProcessPath
