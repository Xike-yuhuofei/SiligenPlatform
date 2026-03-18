# 当前项目代码行数报告

统计时间：2026-03-16  
统计目录：`src/`、`tests/`

说明：本报告反映 2026-03-16 的代码快照；其中 `src/adapters/cli`、`src/application/cli` 等条目仅作历史参考，后续 CLI 已正式退役，当前仓库未保留受支持的独立 CLI app。

## 统计口径

- 包含扩展名：`*.cpp`、`*.cc`、`*.c`、`*.h`、`*.hpp`、`*.hh`、`*.ipp`、`*.inl`、`*.tpp`、`*.py`
- 排除目录：`third_party`、`__pycache__`、日志目录、构建产物、文档和脚本目录
- 统计方式：物理行数，不区分代码行、注释行、空行
- 说明：`src/adapters/hmi` 下的 Python 文件已计入
- 说明：`src/siligen_pch.h`、`src/stdafx.h` 作为独立文件单列

## 总览

| 模块 | 文件数 | 行数 |
|---|---:|---:|
| `src/infrastructure` | 131 | 26991 |
| `src/domain` | 187 | 21381 |
| `src/application` | 89 | 14170 |
| `src/adapters` | 40 | 10888 |
| `src/shared` | 63 | 7832 |
| `tests/unit/domain` | 17 | 1473 |
| `tests/unit/application` | 5 | 1218 |
| `src/siligen_pch.h` | 1 | 268 |
| `src/stdafx.h` | 1 | 3 |
| **总计** | **534** | **84224** |

## 二级模块分布

| 模块 | 文件数 | 行数 |
|---|---:|---:|
| `src/infrastructure/adapters` | 86 | 17222 |
| `src/application/usecases` | 71 | 10707 |
| `src/domain/motion` | 72 | 7176 |
| `src/infrastructure/drivers` | 15 | 5500 |
| `src/adapters/hmi` | 16 | 4665 |
| `src/shared/types` | 31 | 4124 |
| `src/domain/dispensing` | 35 | 4083 |
| `src/domain/trajectory` | 22 | 3627 |
| `src/infrastructure/security` | 25 | 3212 |
| `src/adapters/tcp` | 9 | 3034 |
| `src/adapters/cli` | 13 | 3007 |
| `src/domain/diagnostics` | 13 | 2118 |
| `src/application/cli` | 5 | 1461 |
| `src/domain/machine` | 9 | 1405 |
| `src/application/container` | 7 | 1160 |
| `src/domain/configuration` | 9 | 916 |
| `src/domain/safety` | 8 | 879 |
| `src/application/services` | 6 | 842 |
| `src/shared/errors` | 3 | 831 |
| `src/shared/util` | 3 | 759 |
| `tests/unit/application/motion` | 2 | 743 |
| `src/shared/interfaces` | 3 | 715 |
| `src/infrastructure/config` | 2 | 655 |
| `src/domain/recipes` | 15 | 595 |
| `tests/unit/application/dispensing` | 3 | 475 |
| `src/shared/Strings` | 8 | 461 |
| `tests/unit/domain/dispensing` | 5 | 398 |

## 重点模块拆分

### `src/application/usecases`

| 子模块 | 文件数 | 行数 |
|---|---:|---:|
| `dispensing` | 27 | 6425 |
| `motion` | 18 | 2082 |
| `recipes` | 22 | 1499 |
| `system` | 4 | 701 |

### `src/domain`

| 子模块 | 文件数 | 行数 |
|---|---:|---:|
| `motion` | 72 | 7176 |
| `dispensing` | 35 | 4083 |
| `trajectory` | 22 | 3627 |
| `diagnostics` | 13 | 2118 |
| `machine` | 9 | 1405 |
| `configuration` | 9 | 916 |
| `safety` | 8 | 879 |
| `recipes` | 15 | 595 |
| `geometry` | 1 | 223 |
| `system` | 1 | 200 |
| `planning` | 2 | 159 |

### `src/infrastructure/adapters`

| 子模块 | 文件数 | 行数 |
|---|---:|---:|
| `diagnostics` | 25 | 4917 |
| `system` | 17 | 3634 |
| `motion` | 16 | 3470 |
| `recipes` | 12 | 2025 |
| `dispensing` | 7 | 1780 |
| `planning` | 9 | 1396 |

### `tests/unit`

| 子模块 | 文件数 | 行数 |
|---|---:|---:|
| `application/motion` | 2 | 743 |
| `application/dispensing` | 3 | 475 |
| `domain/dispensing` | 5 | 398 |
| `domain/safety` | 3 | 254 |
| `domain/motion` | 2 | 233 |
| `domain/trajectory` | 4 | 230 |
| `domain/recipes` | 1 | 175 |
| `domain/machine` | 1 | 98 |
| `domain/diagnostics` | 1 | 85 |
