# Incident Report - Windows Terminal 配色方案未跟随系统

## 1. 事件描述
- 现象：Windows Terminal 的 UI 主题（	heme）可随系统深浅色切换，但终端配色方案始终固定为 Campbell。
- 影响：浅色系统主题下终端仍使用深色配色，导致观感和可读性不符合预期。

## 2. 复现步骤
1. 打开 C:\Users\Xike\AppData\Local\Packages\Microsoft.WindowsTerminal_8wekyb3d8bbwe\LocalState\settings.json。
2. 在 profiles.defaults 中配置：
   - "darkColorScheme": "Campbell"
   - "lightColorScheme": "One Half Light (复制)"
3. 保持顶层 "theme": "system"。
4. 切换 Windows 系统深浅色主题并重开 Terminal。
5. 观察到配色仍为 Campbell，未切换到浅色方案。

## 3. 根因分析
- 官方 schema（https://aka.ms/terminal-profiles-schema）中不存在 darkColorScheme / lightColorScheme 两个键。
- 该配置被 Windows Terminal 忽略，进而回落到 colorScheme 默认值 Campbell。
- 当前版本（1.23.20211.0）支持的做法是使用：
  - "colorScheme": { "dark": "...", "light": "..." }

## 4. 修复说明
- 修改文件：C:\Users\Xike\AppData\Local\Packages\Microsoft.WindowsTerminal_8wekyb3d8bbwe\LocalState\settings.json
- 修改内容：
  1. 删除 profiles.defaults.darkColorScheme
  2. 删除 profiles.defaults.lightColorScheme
  3. 新增 profiles.defaults.colorScheme = { dark, light }
- 修复后关键片段：

`json
"profiles": {
  "defaults": {
    "colorScheme": {
      "dark": "Campbell",
      "light": "One Half Light (复制)"
    }
  }
}
`

## 5. 回归验证
- 静态验证：settings.json 中已不存在无效键 darkColorScheme/lightColorScheme，并存在合法 colorScheme 的 dark/light 对。
- schema 证据：SchemePair 定义支持 colorScheme.light 与 colorScheme.dark。
- 手动验证建议：
  1. 完全关闭所有 Windows Terminal 窗口并重新打开。
  2. 切换系统到浅色模式，应显示 One Half Light (复制)。
  3. 切换系统到深色模式，应恢复 Campbell。

## 6. 防复发措施
- 后续修改 Terminal 设置时，以 https://aka.ms/terminal-profiles-schema 为准，避免使用非 schema 字段名。
- 若需要自动化脚本写入配置，增加字段白名单校验，禁止写入未知键。
