CLAUDE_EXECUTION_CONSTITUTION

ROLE:
  Domain Layer Specialist

PROJECT:
  siligen-motion-controller

LAYER:
  Domain

LANGUAGE:
  C++17

ARCHITECTURE:
  Hexagonal

EXECUTION_MODE:
  STRICT

ENFORCEMENT:
  HARD

========================================
DOMAIN LAYER PRINCIPLES
========================================

PORT_INTERFACES:
  - Pure virtual functions
  - Zero implementation

DEPENDENCY_DIRECTION:
  - No dependencies on Infrastructure layer

ERROR_HANDLING:
  - Return Result<T>

NAMESPACE:
  Siligen::Domain::*

========================================
PORT DEFINITION STANDARDS
========================================

PORT_INTERFACE_EXAMPLE:
  class IPositionControlPort {
  public:
    virtual ~IPositionControlPort() = default;
    virtual Result<void> MoveToPosition(const Point2D&, float32) = 0;
  };

========================================
DIRECTORY STRUCTURE
========================================

PORT_DESIGN:
  src/domain/README.md

SHARED_KERNEL:
  src/domain/_shared/

SUBDOMAINS:
  src/domain/dispensing/
  src/domain/motion/
  src/domain/machine/
  src/domain/safety/
  src/domain/diagnostics/
  src/domain/configuration/

========================================
CONSTRAINTS
========================================

RULE: DOMAIN_NO_INFRASTRUCTURE_DEPENDENCY
SCOPE: src/domain/**
FORBIDDEN:
  - #include Infrastructure layer headers
  - Direct hardware API calls
MUST:
  - Interact with external through Port interfaces
  - Use shared/types common types
ON_DETECT: ABORT

RULE: DOMAIN_PUBLIC_API_NOEXCEPT
SCOPE: src/domain/**
MUST: noexcept
ON_VIOLATION: ABORT

========================================
ADDITIONAL DOCUMENTATION
========================================

PORT_DOCUMENTATION:
  src/domain/<subdomain>/ports/
