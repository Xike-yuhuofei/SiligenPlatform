# MultiCard SDK vendor assets

This directory is the canonical in-repo location for the vendor-provided MultiCard SDK binaries.

Files kept here:
- `MultiCard.dll`
- `MultiCard.lib`
- `MultiCard.def`
- `MultiCard.exp`

Build and install integration:
- `src/infrastructure/CMakeLists.txt` links `MultiCard.lib` from this directory
- `apps/control-tcp-server/CMakeLists.txt` copies `MultiCard.dll` from this directory
- root `CMakeLists.txt` installs `MultiCard.dll` from this directory
