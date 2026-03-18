CLAUDE_EXECUTION_CONSTITUTION

ROLE:
  Application Layer Specialist

PROJECT:
  siligen-motion-controller

LAYER:
  Application

LANGUAGE:
  C++17

ARCHITECTURE:
  Hexagonal

EXECUTION_MODE:
  STRICT

ENFORCEMENT:
  HARD

========================================
APPLICATION LAYER PRINCIPLES
========================================

USECASE_ORCHESTRATION:
  - Call Ports to complete business processes

REQUEST_RESPONSE_PATTERN:
  - Request/Response pattern

DEPENDENCY_INJECTION:
  - Inject Ports through constructor

NAMESPACE:
  Siligen::Application::*

========================================
USECASE IMPLEMENTATION STANDARDS
========================================

USECASE_PATTERN:
  class SomeUseCase {
  public:
    explicit SomeUseCase(shared_ptr<IPort> port);
    Result<Response> Execute(const Request& req);
  };

========================================
DIRECTORY STRUCTURE
========================================

USECASE_DOCUMENTATION:
  src/application/usecases/README.md

DI_CONTAINER:
  apps/control-runtime/container/README.md

USECASE_EXAMPLE:
  InitializeSystemUseCase.h

========================================
CONSTRAINTS
========================================

RULE: APPLICATION_DEPENDENCY_DIRECTION
FORBIDDEN:
  - Direct Adapter implementation dependencies
  - Business logic in Application layer
MUST:
  - Only depend on Port interfaces
  - Delegate business logic to Domain layer
ON_DETECT: ABORT

RULE: APPLICATION_REQUEST_VALIDATION
SCOPE: src/application/**
MUST:
  - Request objects include Validate() method
ON_DETECT: WARNING

========================================
ADDITIONAL DOCUMENTATION
========================================

USECASE_GUIDE:
  src/application/usecases/README.md

CONTAINER_GUIDE:
  apps/control-runtime/container/README.md
