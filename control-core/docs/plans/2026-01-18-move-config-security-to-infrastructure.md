# Move Config/Security Into Infrastructure Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Move `src/config` and `src/security` into the infrastructure layer while keeping build and includes working.

**Architecture:** Treat config/security as infrastructure implementations. Update CMake targets and include paths so callers keep using stable `#include` paths (or adjust them consistently) without changing runtime behavior.

**Tech Stack:** C++17, CMake, Windows (Clang-CL toolchain)

---

### Task 1: Confirm current build wiring and references

**Files:**
- Inspect: `D:\Projects\SiligenSuite\control-core\CMakeLists.txt`
- Inspect: `D:\Projects\SiligenSuite\control-core\src\infrastructure\CMakeLists.txt`
- Inspect: `D:\Projects\SiligenSuite\control-core\src\config\*.h`
- Inspect: `D:\Projects\SiligenSuite\control-core\src\security\*.h`

**Step 1: Write the failing test**

N/A (目录重构不新增测试)

**Step 2: Run test to verify it fails**

N/A

**Step 3: Write minimal implementation**

N/A

**Step 4: Run test to verify it passes**

N/A

**Step 5: Commit**

N/A

---

### Task 2: Move directories into infrastructure

**Files:**
- Move: `D:\Projects\SiligenSuite\control-core\src\config` → `D:\Projects\SiligenSuite\control-core\src\infrastructure\config`
- Move: `D:\Projects\SiligenSuite\control-core\src\security` → `D:\Projects\SiligenSuite\control-core\src\infrastructure\security`

**Step 1: Write the failing test**

N/A

**Step 2: Run test to verify it fails**

N/A

**Step 3: Write minimal implementation**

N/A

**Step 4: Run test to verify it passes**

N/A

**Step 5: Commit**

```bash
git add src/infrastructure/config src/infrastructure/security
git commit -m "refactor: move config and security under infrastructure"
```

---

### Task 3: Update include paths after move

**Files:**
- Modify: `D:\Projects\SiligenSuite\control-core\src\infrastructure\security\*.h`
- Modify: `D:\Projects\SiligenSuite\control-core\src\infrastructure\security\*.cpp`
- Modify: `D:\Projects\SiligenSuite\control-core\src\infrastructure\config\*.h`
- Modify: `D:\Projects\SiligenSuite\control-core\src\infrastructure\config\*.cpp`
- Modify: any `#include "config/..."` or `#include "security/..."` usage under `D:\Projects\SiligenSuite\control-core\src`

**Step 1: Write the failing test**

N/A

**Step 2: Run test to verify it fails**

N/A

**Step 3: Write minimal implementation**

N/A

**Step 4: Run test to verify it passes**

N/A

**Step 5: Commit**

```bash
git add src
git commit -m "refactor: update includes for infra config/security"
```

---

### Task 4: Update CMake targets to new paths

**Files:**
- Modify: `D:\Projects\SiligenSuite\control-core\CMakeLists.txt`
- Modify: `D:\Projects\SiligenSuite\control-core\src\infrastructure\CMakeLists.txt`

**Step 1: Write the failing test**

N/A

**Step 2: Run test to verify it fails**

N/A

**Step 3: Write minimal implementation**

N/A

**Step 4: Run test to verify it passes**

N/A

**Step 5: Commit**

```bash
git add D:\Projects\SiligenSuite\control-core\CMakeLists.txt D:\Projects\SiligenSuite\control-core\src\infrastructure\CMakeLists.txt
git commit -m "build: wire infra config/security paths"
```

---

### Task 5: Verification

**Files:**
- N/A (run build)

**Step 1: Write the failing test**

N/A

**Step 2: Run test to verify it fails**

N/A

**Step 3: Write minimal implementation**

N/A

**Step 4: Run test to verify it passes**

Run: `cmake -B build -S .`  
Run: `cmake --build build --config Debug`  
Expected: build succeeds

**Step 5: Commit**

N/A
