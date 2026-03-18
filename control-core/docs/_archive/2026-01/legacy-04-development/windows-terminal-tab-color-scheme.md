# Windows Terminal 选项卡颜色动态控制实现计划

## 📋 需求概述

为 Claude Code 实现**按会话状态动态改变 Windows Terminal 选项卡颜色**的功能。

### 用户确认的需求
- ✅ 支持状态：运行中、完成、空闲、需要权限
- ✅ 行为模式：状态保持（颜色持续反映当前状态）
- ✅ 恢复机制：自动恢复（空闲时恢复默认颜色）

---

## 🎨 颜色方案设计

| 状态 | 颜色名称 | HEX 颜色 | RGB 格式 | 用途 |
|------|---------|---------|----------|------|
| **运行中** | 活力蓝 | `#3b82f6` | `rgb:3b/82/f6` | Claude 执行任务中 |
| **完成** | 成功绿 | `#22c55e` | `rgb:22/c5/5e` | 任务完成（显示3秒后恢复） |
| **空闲** | 中性灰 | `#6b7280` | `rgb:6b/72/80` | 等待用户输入 |
| **需要权限** | 警告橙 | `#f59e0b` | `rgb:f5/9e/0b` | 等待用户授权 |

---

## 🏗️ 技术架构

### 核心技术
- **主要方法**: OSC 4 转义序列（24位RGB，Windows Terminal v1.15+）
- **备用方法**: DECAC 序列（256色调色板，更广泛兼容）
- **降级方案**: 仅更新窗口标题（不支持的终端）

### 关键设计决策
1. **状态持久化**: 使用 JSON 文件跟踪当前状态
2. **避免重复**: 检查状态改变才发送转义序列
3. **非阻塞**: 颜色设置失败不影响 Claude Code 运行
4. **模块化**: 核心库独立，便于测试和维护

---

## 📁 文件清单

### 新增文件（3个）

#### 1. `C:\Users\Xike\.claude\lib\TabColorLib.psm1` (~250行)
**核心颜色管理库**

**关键函数**:
```powershell
# 设置选项卡颜色（自动选择最佳方法）
Set-TabColor -State <running|completed|idle|permission> [-Force]

# 检测终端支持
Test-TerminalSupport

# 状态管理
Get-TabColorState
Set-TabColorState

# 转义序列生成
New-Osc4Sequence -ColorIndex 264 -Color "rgb:3b/82/f6"
New-DecacSequence -PaletteIndex 195
```

**颜色定义**:
```powershell
$COLOR_MAP = @{
    "running"   = "rgb:3b/82/f6"
    "completed" = "rgb:22/c5/5e"
    "idle"      = "rgb:6b/72/80"
    "permission" = "rgb:f5/9e/0b"
}
```

**状态文件格式**:
```json
{
  "sessionId": "current-session-id",
  "currentState": "running",
  "lastUpdate": "2026-01-04T10:30:00Z",
  "terminalSupport": {
    "osc4": true,
    "decac": false
  }
}
```

---

#### 2. `C:\Users\Xike\.claude\hooks\set-tab-color.ps1` (~80行)
**Hook 触发器**

**参数**:
```powershell
param(
    [Parameter(Mandatory=$true)]
    [ValidateSet("running", "completed", "idle", "permission")]
    [string]$State,

    [int]$DelayBeforeReset = 3,  # 完成状态延迟恢复时间
    [string]$SessionId = $PID
)
```

**核心逻辑**:
1. 导入 TabColorLib
2. 调用 Set-TabColor 设置颜色
3. 如果是 completed 状态，启动后台作业延迟恢复 idle

---

#### 3. `C:\Users\Xike\.claude\hooks\init-tab-color.ps1` (~50行)
**用户初始化脚本**

**功能**:
- PowerShell 启动时自动加载
- 设置初始状态为 idle
- 导出用户函数（Set-ClaudeTabColor, Get-ClaudeTabColorState）

**集成方式**: 添加到 PowerShell Profile (`$PROFILE`)

---

### 修改文件（1个）

#### `C:\Users\Xike\.claude\settings.json`
**添加以下 hooks 配置**:

```json
{
  "hooks": {
    "UserPromptSubmit": [
      {
        "matcher": "",
        "hooks": [
          {
            "type": "command",
            "command": "powershell -NoProfile -ExecutionPolicy Bypass -File \"C:/Users/Xike/.claude/hooks/set-tab-color.ps1\" -State \"running\"",
            "timeout": 5
          }
        ]
      }
    ],
    "Notification": [
      {
        "matcher": "permission_prompt",
        "hooks": [
          {
            "type": "command",
            "command": "powershell -NoProfile -ExecutionPolicy Bypass -File \"C:/Users/Xike/.claude/hooks/set-tab-color.ps1\" -State \"permission\"",
            "timeout": 5
          }
          // ... 保持现有 beep 和 notify.ps1
        ]
      },
      {
        "matcher": "idle_prompt",
        "hooks": [
          {
            "type": "command",
            "command": "powershell -NoProfile -ExecutionPolicy Bypass -File \"C:/Users/Xike/.claude/hooks/set-tab-color.ps1\" -State \"idle\"",
            "timeout": 5
          }
          // ... 保持现有 beep 和 notify.ps1
        ]
      }
    ],
    "Stop": [
      {
        "matcher": "",
        "hooks": [
          {
            "type": "command",
            "command": "powershell -NoProfile -ExecutionPolicy Bypass -File \"C:/Users/Xike/.claude/hooks/set-tab-color.ps1\" -State \"completed\" -DelayBeforeReset 3",
            "timeout": 10
          }
          // ... 保持现有 beep, notify.ps1, stop-hook.ps1
        ]
      }
    ]
  }
}
```

**关键变化**:
- ✅ 新增 `UserPromptSubmit` hook（之前缺失）
- ✅ 在 `Notification` hooks 中添加颜色设置
- ✅ 在 `Stop` hook 中添加颜色设置（带延迟恢复）
- ✅ 保持现有功能不变（beep, notify.ps1, stop-hook.ps1）

---

### 自动生成文件（2个）

#### `C:\Users\Xike\.claude\tab-color-state.json`
首次运行时自动创建，用于状态持久化

#### `C:\Users\Xike\.claude\logs\tab-color.log`
调试日志，记录状态变化和错误

---

## 🔄 状态转换逻辑

```
用户提交任务 (UserPromptSubmit)
  ↓
选项卡变为蓝色 (running)
  ↓
保持蓝色，直到任务完成

任务完成 (Stop)
  ↓
选项卡变为绿色 (completed)
  ↓
等待 3 秒
  ↓
自动恢复灰色 (idle)

空闲超时 (idle_prompt)
  ↓
选项卡变为灰色 (idle)
  ↓
保持灰色，直到新任务

需要权限 (permission_prompt)
  ↓
选项卡变为橙色 (permission)
  ↓
保持橙色，直到权限授予
```

---

## 🧪 测试计划

### 单元测试

```powershell
# 1. 导入核心库
Import-Module "C:\Users\Xike\.claude\lib\TabColorLib.psm1"

# 2. 测试终端兼容性
Test-TerminalSupport

# 3. 测试颜色设置
@("running", "completed", "idle", "permission") | ForEach-Object {
    Write-Host "测试状态: $_" -ForegroundColor Yellow
    Set-TabColor -State $_ -Force
    Start-Sleep -Seconds 2
}

# 4. 验证状态持久化
Get-TabColorState
```

### 集成测试

```powershell
# 测试 1: UserPromptSubmit
# 1. 设置初始状态为 idle
# 2. 向 Claude 提交任务
# 预期: 选项卡立即变为蓝色

# 测试 2: Stop
# 1. 等待任务完成
# 预期: 选项卡变为绿色（3秒）→ 自动恢复灰色

# 测试 3: Notification (permission)
# 1. 创建需要权限的任务
# 预期: 选项卡变为橙色

# 测试 4: Notification (idle)
# 1. 等待 Claude 空闲60秒
# 预期: 选项卡变为灰色
```

---

## 📝 实施步骤

### 阶段 1: 创建核心库（30分钟）
1. 创建 `C:\Users\Xike\.claude\lib\` 目录
2. 创建 `TabColorLib.psm1` 文件
3. 实现核心函数（Set-TabColor, Test-TerminalSupport, 等）
4. 测试基本功能

### 阶段 2: 创建 Hooks 触发器（20分钟）
5. 创建 `set-tab-color.ps1` 文件
6. 实现状态切换逻辑
7. 实现延迟恢复机制
8. 测试 hook 触发

### 阶段 3: 更新配置（10分钟）
9. 备份 `settings.json`
10. 添加 `UserPromptSubmit` hook
11. 更新 `Notification` 和 `Stop` hooks
12. 验证配置格式

### 阶段 4: 创建初始化脚本（10分钟）
13. 创建 `init-tab-color.ps1` 文件
14. 更新 PowerShell Profile
15. 测试自动加载

### 阶段 5: 端到端测试（20分钟）
16. 测试所有 4 种状态
17. 验证状态保持
18. 验证自动恢复
19. 检查日志输出

### 阶段 6: 文档和优化（可选，10分钟）
20. 创建 README 文档
21. 添加故障排查指南
22. 性能优化

**总计**: 约 90-120 分钟

---

## ⚠️ 注意事项

### 兼容性
- ✅ Windows Terminal v1.15+ (OSC 4)
- ✅ Windows Terminal v1.0+ (DECAC 备用)
- ⚠️ 其他终端：仅更新窗口标题

### 性能要求
- Hook 执行时间目标: < 100ms
- 状态文件读写目标: < 50ms

### 错误处理
- 所有函数包含 try-catch
- 颜色设置失败不影响 Claude 运行
- 详细日志记录到 `tab-color.log`

### 与现有功能的协同
- ✅ 与 `notify.ps1` 协同（系统通知 + 选项卡颜色）
- ✅ 与 `stop-hook.ps1` 协同（不冲突，各司其职）
- ✅ 与 `emit-event.ps1` 兼容（可未来扩展）

---

## 🔍 故障排查

### 常见问题

**Q: 颜色没有变化**
```powershell
# 检查终端支持
Import-Module "C:\Users\Xike\.claude\lib\TabColorLib.psm1"
Test-TerminalSupport

# 手动测试
Set-TabColor -State "running" -Force

# 查看日志
Get-Content "$env:USERPROFILE\.claude\logs\tab-color.log" -Tail 20
```

**Q: Hook 超时**
- 增加 settings.json 中的 timeout 值（5 → 10秒）
- 检查状态文件是否被锁定

**Q: 颜色闪烁**
- 检查是否有多个 hook 同时设置颜色
- 清理后台作业: `Remove-Job -State Completed`

---

## 📚 参考资料

- [GitHub Discussion #16490 - Set Tab Color Programmatically](https://github.com/microsoft/terminal/discussions/16490)
- [GitHub Issue #6574 - Command to Set Tab Color](https://github.com/microsoft/terminal/issues/6574)
- [Microsoft Learn - Windows Terminal Tips](https://learn.microsoft.com/en-us/windows/terminal/tips-and-tricks)
- [Super User - Change Tab Colors (July 2025)](https://superuser.com/questions/1909260/how-to-change-windows-terminal-wt-exe-tab-colors-and-default-foreground-and-ba)
- [PowerShell - about_ANSI_Terminals](https://learn.microsoft.com/en-us/powershell/module/microsoft.powershell.core/about/about_ANSI_terminals?view=powershell-7.5)

---

## ✅ 验收标准

- [ ] 所有 4 种状态的颜色正确显示
- [ ] 状态保持：颜色持续反映当前状态
- [ ] 自动恢复：完成状态3秒后恢复空闲灰色
- [ ] Hook 执行时间 < 100ms
- [ ] 日志正常记录
- [ ] 不影响现有功能（beep, notify.ps1, stop-hook.ps1）
- [ ] 终端不支持时静默降级

---

## 🎯 关键文件路径

**必须创建**:
1. `C:\Users\Xike\.claude\lib\TabColorLib.psm1`
2. `C:\Users\Xike\.claude\hooks\set-tab-color.ps1`
3. `C:\Users\Xike\.claude\hooks\init-tab-color.ps1`

**必须修改**:
4. `C:\Users\Xike\.claude\settings.json`

**自动创建**:
5. `C:\Users\Xike\.claude\tab-color-state.json`
6. `C:\Users\Xike\.claude\logs\tab-color.log`
