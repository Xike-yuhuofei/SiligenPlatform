CLAUDE_EXECUTION_CONSTITUTION

ROLE:
  Infrastructure Layer Specialist

PROJECT:
  siligen-motion-controller

LAYER:
  Infrastructure

LANGUAGE:
  C++17

ARCHITECTURE:
  Hexagonal

EXECUTION_MODE:
  STRICT

ENFORCEMENT:
  HARD

========================================
INFRASTRUCTURE LAYER PRINCIPLES
========================================

ADAPTER_IMPLEMENTATION:
  - Implement Domain Port interfaces

TECHNICAL_ENCAPSULATION:
  - Encapsulate external system calls

ERROR_CONVERSION:
  - Convert to Domain error types

NAMESPACE:
  Siligen::Infrastructure::*

========================================
ADAPTER IMPLEMENTATION STANDARDS
========================================

ADAPTER_PATTERN:
  class MultiCardMotionAdapter : public IPositionControlPort {
  public:
    Result<void> MoveToPosition(...) override {
      // Call MultiCard API
      // Convert error codes
      return Result<void>();
    }
  };

========================================
DIRECTORY STRUCTURE
========================================

ADAPTER_DOCUMENTATION:
  src/infrastructure/adapters/README.md

HARDWARE_INTERFACES:
modules/device-hal/src/drivers/multicard/

CONFIGURATION_MANAGEMENT:
  src/infrastructure/adapters/system/configuration/

========================================
CONSTRAINTS
========================================

RULE: INFRASTRUCTURE_NO_BUSINESS_LOGIC
FORBIDDEN:
  - Business logic in adapters
MUST:
  - Implement Port interfaces
  - Use Result<T> return
  - Clear and complete error messages
ON_DETECT: ABORT

RULE: INFRASTRUCTURE_NO_DIRECT_API_EXPOSURE
FORBIDDEN:
  - Direct external API exposure
MUST:
  - Wrap external APIs
  - Convert errors semantically
ON_DETECT: ABORT

========================================
ADDITIONAL DOCUMENTATION
========================================

ADAPTER_GUIDE:
  src/infrastructure/adapters/README.md

HARDWARE_GUIDE:
modules/device-hal/src/drivers/multicard/

