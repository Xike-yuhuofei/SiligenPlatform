# shared-kernel first migrated slice

The following assets now have real implementations under `modules/shared-kernel`:

- `include/siligen/shared/numeric_types.h`
- `include/siligen/shared/axis_types.h`
- `include/siligen/shared/error.h`
- `include/siligen/shared/result.h`
- `include/siligen/shared/point2d.h`
- `include/siligen/shared/strings/string_manipulator.h`
- `src/strings/string_manipulator.cpp`

## Why these first

These types are stable, widely reusable, and low-risk:
- numeric aliases
- axis id semantics
- common error/result contracts
- 2D point primitive
- general string normalization helpers

## What remains outside shared-kernel

Still not absorbed into the shared-kernel slice:
- string formatter / validator
- path operations
- encoding codec
- hash calculator
- any config, diagnostics, hardware, or serialization-adjacent types

## Usage policy

New module code should prefer `modules/shared-kernel/include/siligen/shared/*`
headers.
