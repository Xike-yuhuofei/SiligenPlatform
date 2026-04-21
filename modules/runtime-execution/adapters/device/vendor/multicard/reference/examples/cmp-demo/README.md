# CMP demo subset

This folder stores the second-batch demo materials copied from the vendor MultiCard demo workspace.

Scope:

- keep only the demo source and resources that directly expose compare/CMP behavior
- avoid copying the full Visual Studio project, binaries, and unrelated motion examples

Included files:

- `DemoVCDlg.cpp`
  - main demo dialog implementation
  - search for:
    - `OnBnClickedComboCmpH`
    - `OnBnClickedComboCmpL`
    - `OnBnClickedComboCmpP`
    - `OnBnClickedButtonCmpData`
    - `MC_CmpBufSts`
    - `MC_BufCmpData`
- `DemoVCDlg.h`
  - compare-related message handlers and `m_ComboBoxCMPUnit`
- `DemoVC.rc`
  - compare panel widgets such as:
    - `IDC_COMBO_CMP_H`
    - `IDC_COMBO_CMP_L`
    - `IDC_COMBO_CMP_P`
    - `IDC_COMBO_CMP_UNIT`
    - `IDC_BUTTON_CMP_DATA`
    - `IDC_STATIC_CMP_COUNT`
- `resource.h`
  - symbolic IDs for the compare controls
- `MultiCard_API_Reference.md`
  - demo-side API reference containing compare/CMP examples in one file

Why this subset matters:

- it shows how the vendor demo wires CMP actions into a real dialog workflow
- it preserves the vendor's own demo assumptions around buffer status polling and compare-data writes
- it gives direct source evidence for `MC_CmpPluse`, `MC_CmpBufData`, `MC_CmpBufSts`, and a commented `MC_BufCmpData` usage

Notes:

- these are original full demo files, not reduced excerpts
- compare/CMP logic is embedded inside a larger MFC demo dialog
- for SDK capability claims, always cross-check against the canonical files already stored under:
  - `../../api/`
  - `../../guides/`
  - `../../manual/`
