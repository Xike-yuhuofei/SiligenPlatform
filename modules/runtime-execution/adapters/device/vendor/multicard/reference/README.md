# MultiCard reference assets

This directory stores selected board-related reference materials copied from the vendor reference workspace for Bopai motion controller assets.

Selection rule:

- Keep materials directly useful for current bring-up, SDK capability review, and CMP/compare analysis
- Exclude duplicated binaries, build outputs, logs, and bulk demo artifacts that are not needed in the main repo

Included materials:

- `manual/博派运动控制卡用户手册V7.3.pdf`
  - Primary vendor manual
- `guides/MultiCard_连接开发文档.md`
  - Connection flow, IP/port conventions, and MC_Open usage
- `guides/博派运动控制卡技术文档-高级功能篇.md`
  - Higher-level vendor capability overview, including compare output APIs
- `api/MC_CmpBufData.md`
  - Buffered position compare API
- `api/MC_CmpBufSts.md`
  - Compare buffer status / remaining space API
- `api/MC_CmpPluse.md`
  - Immediate compare pulse API
- `api/MC_BufCmpData.md`
  - Coordinate-system-buffer compare API
- `modules/runtime-execution/adapters/device/vendor/multicard/reference/examples/cmp-demo/README.md`
  - Demo-native CMP/compare source subset index
- `modules/runtime-execution/adapters/device/vendor/multicard/reference/examples/cmp-demo/DemoVCDlg.cpp`
  - Vendor demo dialog source with CMP handlers and polling
- `modules/runtime-execution/adapters/device/vendor/multicard/reference/examples/cmp-demo/DemoVCDlg.h`
  - Handler declarations and CMP unit combo
- `modules/runtime-execution/adapters/device/vendor/multicard/reference/examples/cmp-demo/DemoVC.rc`
  - Demo UI resources for the compare panel
- `modules/runtime-execution/adapters/device/vendor/multicard/reference/examples/cmp-demo/resource.h`
  - Compare-related resource IDs
- `modules/runtime-execution/adapters/device/vendor/multicard/reference/examples/cmp-demo/MultiCard_API_Reference.md`
  - Demo-local consolidated API reference
- `headers/legacy-signature/MultiCardCPP.legacy-demo.h`
  - Older vendor header that exposes legacy `MC_CmpRpt` overloads for signature-drift comparison
- `notes/online-compare-validation-2026-04-19.md`
  - Repository-local real-board evidence for dual-buffer compare loads and refill behavior

Why these were copied:

- They cover the current repository's real-hardware integration path
- They give direct evidence for CMP/compare capabilities beyond the currently used minimal path
- They preserve cross-version evidence when current headers and current DLL exports disagree
- They capture repository-local online evidence when current implementation choices are narrower than board capability
- They are small enough to keep in-repo without dragging in the full external workspace
- They preserve one vendor demo implementation path for CMP wiring without importing the entire demo solution
