#include "application/services/process_path/ProcessPathFacade.h"

#include "domain/trajectory/domain-services/GeometryNormalizer.h"
#include "domain/trajectory/domain-services/ProcessAnnotator.h"
#include "domain/trajectory/domain-services/TrajectoryShaper.h"

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
