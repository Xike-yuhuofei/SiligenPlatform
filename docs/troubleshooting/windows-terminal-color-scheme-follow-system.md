# Windows Terminal 配色方案跟随系统（FAQ）

更新时间：`2026-03-23`

## 标准问答

Q：Windows Terminal 配色方案如何跟随系统？

A：使用 `theme: "system"` 控制 UI 深浅色，同时在 `profiles.defaults.colorScheme` 使用 `SchemePair`（`dark`/`light`）控制终端配色；不要使用无效键 `darkColorScheme`/`lightColorScheme`。

## 适用范围

- Windows Terminal（`Microsoft.WindowsTerminal`）1.23.x
- 配置文件：`%LOCALAPPDATA%\Packages\Microsoft.WindowsTerminal_8wekyb3d8bbwe\LocalState\settings.json`

## 正确配置（可直接复用）

```json
{
  "theme": "system",
  "profiles": {
    "defaults": {
      "colorScheme": {
        "dark": "Campbell",
        "light": "One Half Light (复制)"
      }
    }
  }
}
```

## 反例与根因

反例（无效字段）：

```json
{
  "profiles": {
    "defaults": {
      "darkColorScheme": "Campbell",
      "lightColorScheme": "One Half Light (复制)"
    }
  }
}
```

根因：

- `darkColorScheme`/`lightColorScheme` 不在官方 schema（`https://aka.ms/terminal-profiles-schema`）中。
- 字段被忽略后，Terminal 回退到默认 `colorScheme`，表现为始终 `Campbell`。

## 验证步骤

1. 保存 `settings.json`，完全关闭所有 Windows Terminal 窗口。
2. 重新打开 Terminal，切换 Windows 系统到浅色主题，确认使用 `light` 对应方案。
3. 切换回深色主题，确认使用 `dark` 对应方案。

## 固化建议（知识资产）

1. 把该 FAQ 作为唯一标准答案入口，后续同类问题统一引用本文档。
2. 若要自动写入 Terminal 配置，先对字段做 schema 白名单校验，禁止写入未知键。
3. 每次升级 Windows Terminal 大版本后，重新核对 `https://aka.ms/terminal-profiles-schema` 的 `colorScheme` 定义。
