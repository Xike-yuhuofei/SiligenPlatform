# Selection note

This subset was copied for repository-local reference on `2026-04-19`.

Not copied on purpose:

- full demo projects
- repeated copies of `MultiCardCPP.h`
- duplicate `dll/lib` payloads already present under the canonical vendor directory
- upgrade tools, runtime logs, Python experiments, and generated build artifacts

If later work needs more evidence for specific APIs such as `MC_GpioCmpBufData`, `MC_LnXYCmpPluse`, or demo UI flows, pull them in as a second batch instead of copying the whole external directory.

Second batch completed on `2026-04-19`:

- copied a focused demo subset under `reference/examples/cmp-demo/`
- limited to compare/CMP dialog source, UI resources, and demo-local API reference
