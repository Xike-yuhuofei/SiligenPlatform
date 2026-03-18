# shared-kernel candidates

This document records what should move from `src/shared` into `modules/shared-kernel`
in the repository-internal modularization phase.

## Move first

- `src/shared/types/Result.h`
- `src/shared/types/Error.h`
- `src/shared/types/Point2D.h`
- `src/shared/types/Types.h` (only stable base aliases)
- `src/shared/strings/StringManipulator.*`
- `src/shared/strings/StringFormatter.*`
- `src/shared/strings/StringValidator.*`
- `src/shared/paths/PathOperations.*`
- `src/shared/encoding/EncodingCodec.*`
- `src/shared/hashing/HashCalculator.*`

## Review before moving

- `src/shared/types/AxisStatus.h`
- `src/shared/types/AxisTypes.h`
- `src/shared/types/TrajectoryTypes.h`
- `src/shared/types/VisualizationTypes.*`
- `src/shared/types/DispensingStrategy.h`
- `src/shared/errors/ErrorDescriptions.h`
- `src/shared/errors/ErrorHandler.*`
- `src/shared/geometry/BoostGeometryAdapter.h`

These files likely carry motion, process, visualization, or infrastructure semantics and
should not be copied into `shared-kernel` without decomposition.

## Do not move into shared-kernel

- `src/shared/interfaces/IHardwareController.h`
- `apps/control-runtime/bootstrap/InfrastructureBindings.h`
- `src/shared/di/LoggingServiceLocator.h`
- `src/shared/types/HardwareConfiguration.h`
- `src/shared/types/ConfigTypes.h`
- `src/shared/types/DiagnosticsConfig.h`
- `src/shared/types/SerializationTypes.h`
- `src/shared/types/PerformanceBenchmark.h`
- `src/shared/util/*`
- `src/shared/Testing/*`

These files are tied to runtime, hardware, diagnostics, architecture checks, or tests.
They should remain outside `shared-kernel` or be redistributed later.
