# Domain Ports Docs + Prune Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 清理与 `domain/<subdomain>/ports` 相关文档，使示例与当前端口归属一致，并在文档完成后审查根端口是否仍有可删除的冗余。

**Architecture:** 先统一文档中的端口放置与命名空间示例，再通过代码引用统计确认是否存在“仅自引用”的端口文件。只有无业务引用时才删除。

**Tech Stack:** C++17, Markdown, CMake, ripgrep, PowerShell

---

### Task 1: 文档更新（端口归属与命名空间）

**Files:**
- Modify: `src/application/container/README.md`
- Modify: `src/infrastructure/adapters/README.md`
- Modify: `src/adapters/cli/硬件连接实现方案.md`
- Modify: `src/application/2026-01-16-architecture-migration-detailed-analysis.md`

**Step 1: 更新“新增端口”示例路径**

在 `src/application/container/README.md` 与 `src/infrastructure/adapters/README.md` 中，将示例从根 `domain/<subdomain>/ports` 改为子域路径，并补充跨子域说明：

```cpp
// src/domain/<subdomain>/ports/INewPort.h
// 若为跨子域端口，放在 src/domain/<subdomain>/ports/
class INewPort {
public:
    virtual ~INewPort() = default;
    virtual Result<void> DoSomething() = 0;
};
```

**Step 2: 修正文档中的硬件连接命名空间**

在 `src/adapters/cli/硬件连接实现方案.md` 的代码片段中，将以下符号从 `Domain::Ports` 改为 `Domain::Machine::Ports`：

```cpp
const Domain::Machine::Ports::HardwareConnectionConfig& config
Domain::Machine::Ports::HardwareConnectionState::Connecting
Domain::Machine::Ports::HardwareConnectionState::Connected
Domain::Machine::Ports::HardwareConnectionState::Error
```

**Step 3: 更新迁移分析文档的已修复说明**

在 `src/application/2026-01-16-architecture-migration-detailed-analysis.md` 的 “5.2 Domain Port 临时违规” 小节，将状态标注为已修复，并把示例 include 更新为当前实际依赖：

```cpp
// src/domain/planning/ports/IDXFFileParsingPort.h
#include "shared/types/Point.h"
#include "shared/types/Point2D.h"
#include "shared/types/Result.h"
#include "shared/types/TrajectoryTypes.h"
#include "shared/types/VisualizationTypes.h"
#include "domain/geometry/GeometryTypes.h"
```

并将状态文字调整为：“已修复：移除对 application/parsers 的依赖，统一引用 shared/types 与 domain/geometry。”

**Step 4: 验证文档残留**

Run: `rg -n "Domain::Ports::HardwareConnection" D:\Projects\SiligenSuite\control-core\src -g"*.md"`
Expected: 无输出

**Step 5: Commit**

```bash
git add src/application/container/README.md src/infrastructure/adapters/README.md src/adapters/cli/硬件连接实现方案.md src/application/2026-01-16-architecture-migration-detailed-analysis.md
git commit -m "docs: align ports docs with subdomain placement"
```

---

### Task 2: 根端口冗余审查（删除仅自引用文件）

**Files:**
- Modify/Delete: `src/domain/<subdomain>/ports/*.h`（仅当确认无业务引用）

**Step 1: 统计非文档引用次数**

Run:

```powershell
$ports = Get-ChildItem -File D:\Projects\SiligenSuite\control-core\src\domain\ports | Where-Object { $_.Name -ne 'README.md' }
$results = foreach ($p in $ports) {
  $name = $p.Name
  $pattern = [regex]::Escape($name)
  $hits = rg -n $pattern D:\Projects\SiligenSuite\control-core\src -g"!*\\.md" | Measure-Object | Select-Object -ExpandProperty Count
  [pscustomobject]@{File=$name;NonMdHits=$hits}
}
$results | Sort-Object NonMdHits, File | Format-Table -AutoSize
```

**Step 2: 删除仅自引用的端口（若存在）**

规则：`NonMdHits == 1` 且唯一命中为自身定义文件时，判定为可删。

执行删除与引用清理（示例）：

```powershell
Remove-Item D:\Projects\SiligenSuite\control-core\src\domain\ports\IUnusedPort.h
```

同时移除唯一引用点中的 `#include "domain/<subdomain>/ports/IUnusedPort.h"`。

**Step 3: 构建验证（仅当删除了代码文件）**

Run: `cmake --build D:\Projects\SiligenSuite\control-core\build\stage4-all-modules-check --config Debug --target siligen_control_application`
Expected: 构建成功（允许已有警告）

**Step 4: Commit（仅当有删除）**

```bash
git add src/domain/<subdomain>/ports
git commit -m "chore: remove unused root ports"
```

---


