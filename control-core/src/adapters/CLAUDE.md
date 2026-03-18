CLAUDE_EXECUTION_CONSTITUTION

ROLE:
  Adapter Layer Specialist

PROJECT:
  siligen-motion-controller

LAYER:
  Adapters

LANGUAGE:
  C++17

ARCHITECTURE:
  Hexagonal

EXECUTION_MODE:
  STRICT

ENFORCEMENT:
  HARD

========================================
ADAPTER LAYER PRINCIPLES
========================================

EXTERNAL_INTERFACES:
  - TCP Adapter

DEPENDENCY_RULE:
  - Call Application Layer UseCases
  - Convert TCP requests to UseCase requests
  - Format UseCase responses for transport

========================================
DIRECTORY STRUCTURE
========================================

TCP_ENTRY:
  apps/control-tcp-server/main.cpp

BUILD_ENTRY:
  apps/CMakeLists.txt

========================================
IMPLEMENTATION STANDARDS
========================================

TCP_PATTERN:
  Use ApplicationContainer to resolve UseCases

  auto container = ApplicationContainer("config.ini");
  container.Configure();
  auto usecase = container.Resolve<SomeUseCase>();
  auto result = usecase->Execute(request);

========================================
CONSTRAINTS
========================================

RULE: ADAPTER_DEPENDENCY_DIRECTION
MUST:
  - Adapters -> Application UseCases
  - Adapters -> Request/Response DTOs
FORBIDDEN:
  - Adapters -> Domain Models
  - Adapters -> Repository Implementations
  - Adapters -> Other Adapters
ON_DETECT: ABORT

RULE: ADAPTER_NO_BYPASS_USECASE
FORBIDDEN:
  - Bypass UseCase to call Ports directly
  - Include business logic
MUST:
  - Execute operations through UseCases
ON_DETECT: ABORT

RULE: ADAPTER_INPUT_VALIDATION
SCOPE: Adapters
MUST:
  - Basic validation (format, required, range)
  - Friendly error messages
ON_DETECT: WARNING

========================================
ADDITIONAL DOCUMENTATION
========================================

ADAPTER_README:
  src/adapters/README.md

CONTAINER_DOCUMENTATION:
  apps/control-runtime/container/README.md


