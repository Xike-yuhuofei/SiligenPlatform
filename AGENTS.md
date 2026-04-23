# Repository Guidelines

## Project Structure & Module Organization
`SiligenSuite` is a canonical workspace rooted at `apps/`, `modules/`, and `shared/`. Put process hosts and packaging shells in `apps/` (`runtime-service`, `runtime-gateway`, `planner-cli`, `hmi-app`, `trace-viewer`). Put business owners and domain workflows in `modules/`. Reserve `shared/` for stable contracts, kernel utilities, IDs, and test infrastructure only. Keep validation assets in `tests/`, architecture and runbooks in `docs/`, machine and recipe data in `config/` and `data/`, and helper automation in `scripts/`. Do not reintroduce retired roots such as `packages/`, `integration/`, or `tools/`.

## Build, Test, and Development Commands
Run everything from the repository root.

- `.\scripts\validation\install-python-deps.ps1`: install Python 3.11 test dependencies.
- `.\build.ps1`: canonical workspace build entry.
- `.\test.ps1 -Profile Local -Suite all`: local validation entry; writes reports to `tests/reports/`.
- `.\ci.ps1 -Suite all`: CI-grade build, test, freeze-doc, and gate run.
- `powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\validation\run-local-validation-gate.ps1`: publish local gate evidence.
- `python -m pytest .\apps\hmi-app\tests\unit -q`: targeted Python/HMI regression.
- `ctest --test-dir .\build -C Debug --output-on-failure`: targeted C++ regression after a successful build.

## Coding Style & Naming Conventions
Follow existing module boundaries: `apps/` wires, `modules/` owns business semantics, `shared/` stays generic. C++ uses PascalCase file names and `*Test.cpp` tests; Python uses `snake_case` modules and `test_*.py`. Match the surrounding file’s formatting; most repository code uses 4-space indentation. Keep new paths canonical and descriptive, using lowercase kebab-case for folders when creating new docs or scripts.

## Testing Guidelines
Primary frameworks are GoogleTest for C++ and `pytest` for Python. Prefer root scripts before ad hoc commands because lane metadata and evidence bundles are part of the contract. Add focused tests beside the owning module, and keep evidence-producing scenarios under `tests/integration/`, `tests/e2e/`, or `tests/contracts/`. Name tests by behavior, not implementation detail.

## Commit & Pull Request Guidelines
Recent history follows conventional titles such as `test(runtime): add online action matrix scenarios`; use `type(scope): imperative summary`. Branches must follow `<type>/<scope>/<ticket>-<short-desc>`, for example `fix/hmi/BUG-311-fix-startup-freeze`. Keep one primary ticket per branch and PR. PR descriptions should list the root-level commands you ran, link the ticket, and attach screenshots or report paths when UI, gateway, or validation evidence changes.
