# Wave 4E Release Package Observation

- date: `2026-03-23`
- target: `C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build`

## 1. 执行摘要

```text
C:\Users\Xike\AppData\Roaming\npm\node_modules\@openai\codex\node_modules\@openai\codex-win32-x64\vendor\x86_64-pc-windows-msvc\path\rg.exe -n dxf-to-pb|path-to-trajectory|export-simulation-input|generate-dxf-preview|dxf_pipeline|proto\.dxf_primitives_pb2|config\\machine_config\.ini C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build -g *.ps1 -g *.py -g *.bat -g *.cmd -g *.scr -g *.ini -g *.json -g *.yaml -g *.yml -g *.xml -g *.cfg -g *.conf -g *.toml -g *.txt -g *.md
```

- scan_command: `C:\Users\Xike\AppData\Roaming\npm\node_modules\@openai\codex\node_modules\@openai\codex-win32-x64\vendor\x86_64-pc-windows-msvc\path\rg.exe -n dxf-to-pb|path-to-trajectory|export-simulation-input|generate-dxf-preview|dxf_pipeline|proto\.dxf_primitives_pb2|config\\machine_config\.ini C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build -g *.ps1 -g *.py -g *.bat -g *.cmd -g *.scr -g *.ini -g *.json -g *.yaml -g *.yml -g *.xml -g *.cfg -g *.conf -g *.toml -g *.txt -g *.md`
- scan_output: `integration\reports\verify\wave6b-external-20260323-104249\observation\release-package.scan.txt`
- path_hits: `integration\reports\verify\wave6b-external-20260323-104249\observation\release-package.path-hits.txt`
- legacy_content_hits: `False`
- legacy_path_hits: `False`

## 2. 裁决

```text
observation_result = Go
scope = release-package
evidence = integration\reports\verify\wave6b-external-20260323-104249\observation\release-package.scan.txt, integration\reports\verify\wave6b-external-20260323-104249\observation\release-package.path-hits.txt
next_action = release package observation passed
```

## 3. 说明

1. 未发现 legacy DXF 入口、compat shell 或 deleted alias 命中
