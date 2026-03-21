# 实体 ID 生成规范 v1.0.0

## 1. 目标

`entity_id` 必须满足：

1. 同一代码实体在重复扫描中稳定不变。
2. 重载、模板实例、匿名命名空间可被区分。
3. 与文件移动、格式化等非语义变化弱耦合。

## 2. 输入字段

生成 `entity_id` 仅使用以下规范化字段：

1. `repo_relative_path`
2. `entity_type`
3. `normalized_symbol`
4. `normalized_signature`
5. `disambiguator`

## 3. 规范化规则

### 3.1 路径规范化

1. 一律使用仓库相对路径。
2. 路径分隔符统一为 `/`。
3. 移除 `.` 与重复分隔符。
4. Windows 盘符与绝对前缀不得进入 ID 计算。

示例：  
`D:\Projects\SiligenSuite\packages\process-runtime-core\src\application\usecases\system\InitializeSystemUseCase.cpp`  
规范化后：  
`packages/process-runtime-core/src/application/usecases/system/InitializeSystemUseCase.cpp`

### 3.2 符号规范化

1. 保留完整命名空间与类限定名。
2. 多余空白、换行折叠为单空格后移除两端空白。
3. 运算符重载保持原名（如 `operator()`）。

### 3.3 签名规范化

1. 参数列表使用类型全名，忽略参数变量名。
2. `const`、引用、指针限定必须保留。
3. 模板参数按编译器可判定顺序展开。
4. 文件级实体（`file`、`cmake_target`）签名为空字符串。

### 3.4 消歧字段 disambiguator

1. 匿名命名空间：`anon_ns@<line>`。
2. lambda：`lambda@<line>:<column>`。
3. 模板实例：`tmpl@<specialization-hash>`。
4. 常规命名实体：空字符串。

## 4. 生成算法

1. 组装基础串：  
`raw = repo_relative_path + "|" + entity_type + "|" + normalized_symbol + "|" + normalized_signature + "|" + disambiguator`
2. 计算散列：`sha1(raw)`。
3. 产出：`entity_id = "ent_" + sha1(raw)`。

## 5. 示例

输入：

- path: `packages/process-runtime-core/src/application/usecases/system/InitializeSystemUseCase.cpp`
- entity_type: `method`
- symbol: `siligen::process::application::InitializeSystemUseCase::Execute`
- signature: `(const Request&) const`
- disambiguator: ``

输出：

- `entity_id = ent_<sha1>`

注：文档不固化具体 hash 值，避免示例路径变更导致误导。

## 6. 变更策略

1. 修改规则属于破坏性变更，需升级主版本。
2. 如需兼容旧 ID，必须提供 `legacy_entity_id` 映射表。
