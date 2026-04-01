# third_party 仓内可跟踪物化方案

更新时间：`2026-03-27`

## 1. 目标

目标只有一个：

- GitHub 代码仓库不再跟踪 `third_party/`
- 本地开发机、CI、验证机仍可按同一清单物化出 `third_party/`
- 新 clone 默认不依赖额外环境变量或外部制品源

## 2. 交付物

- `scripts/bootstrap/third-party-manifest.json`
  - 记录顶层依赖包、压缩包名、SHA256、校验路径
- `scripts/bootstrap/bundles/`
  - 仓库默认跟踪的 zip bundles 与 `SHA256SUMS.txt`
- `scripts/bootstrap/export-third-party-bundles.ps1`
  - 从当前本地 `third_party/` 打包出仓内默认 bundles
- `scripts/bootstrap/bootstrap-third-party.ps1`
  - 优先从仓内 `scripts/bootstrap/bundles/`，其次从本地 bundle 目录或远端制品地址恢复 `third_party/`

## 3. 推荐发布方式

默认发布方式改为“仓内跟踪 + 可选外部镜像”：

- repo-tracked `scripts/bootstrap/bundles/`
- GitHub Release assets
- 私有制品库
- 内网文件服务器
- 对象存储

其中，fresh clone 的默认来源是仓内 `scripts/bootstrap/bundles/`；外部制品面只作为镜像或覆盖入口。

`bootstrap-third-party.ps1` 支持两种输入：

- 默认：`scripts/bootstrap/bundles/`
  - fresh clone 的默认 bundle 目录
- `SILIGEN_THIRD_PARTY_ARTIFACT_ROOT`
  - 指向自定义本地 bundle 目录；优先级高于仓内默认目录
- `SILIGEN_THIRD_PARTY_BASE_URI`
  - 指向远端 bundle 基础地址；仅在本地 bundle 源不可用时使用

## 4. 建议操作顺序

1. 在已有完整 `third_party/` 的机器上运行：
   - `powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\bootstrap\export-third-party-bundles.ps1 -Force`
2. 提交 `scripts/bootstrap/bundles/` 下生成的 zip 文件与 `SHA256SUMS.txt`
3. 可选：把同一批 zip 文件镜像到外部制品面
4. 在需要覆盖默认来源的环境设置：
   - `SILIGEN_THIRD_PARTY_ARTIFACT_ROOT`
   - `SILIGEN_THIRD_PARTY_BASE_URI`
5. 通过根级入口或直接运行 bootstrap：
   - `.\build.ps1`
   - `.\test.ps1`
   - `.\ci.ps1`
   - `powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\bootstrap\bootstrap-third-party.ps1`
6. 确认物化链路可用后，再执行：
   - `git rm -r --cached -- third_party`
   - 更新 `.gitignore`
   - 提交并推送

## 5. 当前边界

- 本方案只解决 `third_party/` 不展开跟踪、但 bundles 可仓内默认获取的问题
- 不改变现有 CMake 对 `third_party/` 路径的消费方式
- 不把 Python `requirements.txt` 混同为 C++ vendor 依赖管理
