# MultiCard SDK vendor assets

This directory is the canonical in-repo location for the vendor-provided MultiCard SDK assets consumed by `runtime-execution`.

Canonical path:
- `modules/runtime-execution/adapters/device/vendor/multicard/`

Current in-repo assets:
- `MultiCard.def`
- `MultiCard.exp`
- `MultiCard.dll`
- `MultiCard.lib`
- `README.md`
- `reference/README.md`
- `reference/SELECTION_NOTE.md`
- `reference/notes/*`
- `reference/manual/*`
- `reference/guides/*`
- `reference/api/*`

Tracked real-hardware vendor assets:
- `MultiCard.dll`
- `MultiCard.lib`

Tracked reference documents:
- selected materials copied from `D:\Projects\Document\BopaiMotionController`
- see `reference/README.md` for scope and rationale
- repository-local online validation notes also live under `reference/notes/`

Bring-up rules:
- `config/machine/machine_config.ini` is the only canonical machine config entry; do not introduce alternate bridge config paths.
- `apps/runtime-service/run.ps1` and `apps/runtime-gateway/run.ps1` default to this canonical vendor directory and fail fast when `[Hardware] mode=Real` but `MultiCard.dll` is missing.
- Missing `MultiCard.lib` does not block launching an already-built executable, but it does block rebuilding real-hardware binaries and must be treated as a bring-up prerequisite.
- Fresh clones are expected to receive both files from the repository; if they are missing locally, treat it as repository integrity drift or an incomplete checkout.
- External override is allowed only by passing `-VendorDir <path>` to the run script or by exporting `SILIGEN_MULTICARD_VENDOR_DIR` for the current shell session.
- New vendor binaries must land in this canonical directory or in the explicitly provided external override path; do not restore payloads into any bridge directory.

Build and install integration:
- `modules/runtime-execution/adapters/device/CMakeLists.txt` links `MultiCard.lib` from this directory when present
- `apps/planner-cli/CMakeLists.txt` copies `MultiCard.dll` from this directory when present
- root `CMakeLists.txt` installs `MultiCard.dll` from this directory when present
