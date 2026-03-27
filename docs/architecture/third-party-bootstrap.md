# third_party 外部物化方案

更新时间：`2026-03-27`

## 1. 目标

目标只有一个：

- GitHub 代码仓库不再跟踪 `third_party/`
- 本地开发机、CI、验证机仍可按同一清单物化出 `third_party/`

## 2. 交付物

- `scripts/bootstrap/third-party-manifest.json`
  - 记录顶层依赖包、压缩包名、SHA256、校验路径
- `scripts/bootstrap/export-third-party-bundles.ps1`
  - 从当前本地 `third_party/` 打包出可上传的 zip bundles
- `scripts/bootstrap/bootstrap-third-party.ps1`
  - 从本地 bundle 目录或远端制品地址恢复 `third_party/`

## 3. 推荐发布方式

推荐把 zip bundles 上传到独立制品面，而不是重新放回 Git 仓库：

- GitHub Release assets
- 私有制品库
- 内网文件服务器
- 对象存储

`bootstrap-third-party.ps1` 支持两种输入：

- `SILIGEN_THIRD_PARTY_ARTIFACT_ROOT`
  - 指向本地 bundle 目录
- `SILIGEN_THIRD_PARTY_BASE_URI`
  - 指向远端 bundle 基础地址

## 4. 建议操作顺序

1. 在已有完整 `third_party/` 的机器上运行：
   - `powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\bootstrap\export-third-party-bundles.ps1 -Force`
2. 上传 `build/third-party-bundles/` 下生成的 zip 文件与 `SHA256SUMS.txt`
3. 在 CI 或新 clone 环境设置：
   - `SILIGEN_THIRD_PARTY_BASE_URI`
4. 通过根级入口或直接运行 bootstrap：
   - `.\build.ps1`
   - `.\test.ps1`
   - `.\ci.ps1`
   - `powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\bootstrap\bootstrap-third-party.ps1`
5. 确认物化链路可用后，再执行：
   - `git rm -r --cached -- third_party`
   - 更新 `.gitignore`
   - 提交并推送

## 5. 当前边界

- 本方案只解决 `third_party/` 从 Git 仓库退出的问题
- 不改变现有 CMake 对 `third_party/` 路径的消费方式
- 不把 Python `requirements.txt` 混同为 C++ vendor 依赖管理
