CLAUDE_EXECUTION_CONSTITUTION

ROLE:
  Shared Kernel Specialist

PROJECT:
  siligen-motion-controller

LAYER:
  Shared

LANGUAGE:
  C++17

ARCHITECTURE:
  Hexagonal

EXECUTION_MODE:
  STRICT

ENFORCEMENT:
  HARD

========================================
SHARED LAYER PRINCIPLES
========================================

ZERO_DEPENDENCIES:
  - No dependencies on other modules
  - Stable interfaces (changes require careful evaluation)
  - High reusability (used by all layers)

NAMESPACE:
  Siligen::Shared::*

========================================
CORE TYPES
========================================

RESULT_TYPE:
  Result<T>
  PURPOSE: Unified error handling

POINT_TYPES:
  Point2D
  PURPOSE: Two-dimensional coordinates

  Point3D
  PURPOSE: Three-dimensional coordinates

ERROR_TYPE:
  Error
  PURPOSE: Error type representation

========================================
DIRECTORY STRUCTURE
========================================

TYPES:
  src/shared/types/Result.h

UTILITIES:
  src/shared/utils/

========================================
CONSTRAINTS
========================================

RULE: SHARED_NO_BUSINESS_LOGIC
FORBIDDEN:
  - Business logic dependencies
  - External library dependencies (except standard library)
MUST:
  - Only include common types and utilities
ON_DETECT: ABORT

RULE: SHARED_HEADER_ONLY
PREFERRED:
  - Header-only implementation
  - constexpr optimization
  - inline optimization
ON_DETECT: WARNING

========================================
ADDITIONAL DOCUMENTATION
========================================

RESULT_DOCUMENTATION:
  src/shared/types/Result.h

