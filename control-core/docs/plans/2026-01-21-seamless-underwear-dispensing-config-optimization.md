# 无痕内衣点胶机配置优化方案

**日期**: 2026-01-21
**状态**: deferred
**Owner**: @engineer
**范围**: 配置参数优化、验证规则增强、最佳实践对齐

---

> **Triage(2026-03-17)**：冻结保留。该方案依赖具体机型、胶材和现场验证，不应直接提升为 `control-core` 的 canonical 默认配置。

## 背景

基于对 `build/config/machine_config.ini` 的专业分析，发现当前配置与无痕内衣点胶行业最佳实践存在差距：

| 维度 | 当前评分 | 主要问题 |
|------|----------|----------|
| 工艺参数 | 6/10 | 温度偏低(125°C vs 130-150°C)，黏度容差过宽(27%) |
| 运动性能 | 3/10 | 速度(10mm/s)和加速度(10mm/s²)严重不足 |
| 精度控制 | 5/10 | 定位精度(0.1mm)偏低，pulse_per_mm=200为硬件限制 |
| 阀门控制 | 8/10 | 时序参数合理 |
| 安全配置 | 9/10 | 完整且合理 |

---

## 六边形架构约束

根据 `.claude/rules/` 规则，本方案必须遵循：

### HEXAGONAL.md 约束
- `infrastructure_depends_on_domain` - 配置适配器依赖领域端口
- `access_domain_via_ports_only` - 通过 `IConfigurationPort` 访问配置

### PORTS_ADAPTERS.md 约束
- `PORT_INTERFACE_PURITY` - 端口接口保持纯虚函数
- `ADAPTER_SINGLE_PORT` - 适配器实现单一端口

### INDUSTRIAL.md 约束
- `INDUSTRIAL_REALTIME_SAFETY` - 实时安全不可妥协
- `INDUSTRIAL_VALVE_SAFETY` - 阀门安全必须保障

### DOMAIN.md 约束
- `DOMAIN_NO_INFRASTRUCTURE_DEPENDENCY` - 领域层不依赖基础设施
- `DOMAIN_PUBLIC_API_NOEXCEPT` - 公共API使用noexcept

---

## 变更方案

### Phase 1: 配置模型增强（领域层）

**文件**: `src/domain/configuration/ports/IConfigurationPort.h`

**变更内容**:
1. 在 `DispensingConfig` 中添加行业最佳实践推荐值常量
2. 添加配置预设枚举（用于快速切换不同工艺场景）

```cpp
// 新增：行业推荐值常量（仅供参考，不强制）
namespace SeamlessUnderwearDefaults {
    constexpr float32 kRecommendedTemperature = 135.0f;      // PUR胶推荐温度
    constexpr float32 kRecommendedTemperatureTolerance = 5.0f;
    constexpr float32 kRecommendedViscosity = 6000.0f;       // 推荐黏度
    constexpr float32 kRecommendedViscosityTolerance = 15.0f;
    constexpr float32 kRecommendedMaxSpeed = 100.0f;         // 推荐最大速度
    constexpr float32 kRecommendedMaxAcceleration = 500.0f;  // 推荐最大加速度
    constexpr float32 kRecommendedPositioningTolerance = 0.05f;
}

// 新增：工艺预设枚举
enum class DispensingPreset {
    CUSTOM = 0,           // 自定义
    SEAMLESS_UNDERWEAR,   // 无痕内衣
    ELECTRONICS,          // 电子元件
    AUTOMOTIVE            // 汽车零部件
};
```

**架构合规性**:
- 常量定义在领域层，不依赖基础设施
- 使用 `float32` 共享类型
- 枚举定义符合领域建模

---

### Phase 2: 验证规则增强（基础设施层）

**文件**: `src/infrastructure/adapters/system/configuration/validators/ConfigValidator.cpp`

**变更内容**:
1. 添加无痕内衣工艺特定验证规则
2. 添加行业最佳实践警告（非阻断）

```cpp
// 新增：无痕内衣工艺验证
ValidationResult ConfigValidator::ValidateSeamlessUnderwearConfig(
    const DispensingConfig& config) {
    ValidationResult result;

    // 温度验证（PUR胶工艺）
    if (config.temperature_target_c > 0.0f) {
        if (config.temperature_target_c < 130.0f) {
            result.AddWarning("温度偏低: " + std::to_string(config.temperature_target_c)
                + "°C，PUR胶推荐130-150°C");
        }
        if (config.temperature_tolerance_c > 10.0f) {
            result.AddWarning("温度容差过宽: ±" + std::to_string(config.temperature_tolerance_c)
                + "°C，推荐±5°C以保证黏度稳定");
        }
    }

    // 黏度验证
    if (config.viscosity_target_cps > 0.0f) {
        if (config.viscosity_target_cps > 10000.0f) {
            result.AddWarning("黏度偏高: " + std::to_string(config.viscosity_target_cps)
                + " cP，可能导致拉丝，推荐3000-8000 cP");
        }
        if (config.viscosity_tolerance_pct > 20.0f) {
            result.AddWarning("黏度容差过宽: " + std::to_string(config.viscosity_tolerance_pct)
                + "%，推荐≤15%");
        }
    }

    return result;
}

// 新增：运动参数最佳实践验证
ValidationResult ConfigValidator::ValidateMotionBestPractices(
    const MachineConfig& config) {
    ValidationResult result;

    if (config.max_speed < 50.0f) {
        result.AddWarning("最大速度偏低: " + std::to_string(config.max_speed)
            + " mm/s，生产效率受限，行业标杆100-800 mm/s");
    }

    if (config.max_acceleration < 100.0f) {
        result.AddWarning("最大加速度偏低: " + std::to_string(config.max_acceleration)
            + " mm/s²，响应速度受限，推荐500-2000 mm/s²");
    }

    if (config.positioning_tolerance > 0.05f) {
        result.AddWarning("定位精度偏低: " + std::to_string(config.positioning_tolerance)
            + " mm，无痕内衣推荐≤0.05mm");
    }

    return result;
}
```

**架构合规性**:
- 验证逻辑在基础设施层适配器中
- 使用 `ValidationResult` 返回类型
- 警告不阻断流程，仅提示

---

### Phase 3: 配置文件模板更新

**文件**: `build/config/machine_config.ini`

**变更内容**:
1. 添加无痕内衣工艺推荐值注释
2. 调整默认值为更合理的范围

```ini
[Dispensing]
# 点胶模式: 0=接触式, 1=非接触式, 2=喷射式, 3=位置触发
# 无痕内衣推荐: 喷射式(2)
mode=2

# === 工艺控制参数 ===
# 目标温度 (C) - PUR热熔胶推荐 130-150C（当前125C偏低）
# 行业最佳实践: 135C
temperature_target_c=135.0
# 温度容差 (C) - 推荐 ±5C（当前±15C过宽）
temperature_tolerance_c=5.0

# 目标黏度 (cP) - 推荐 3000-8000 cP（当前11000偏高）
# 行业最佳实践: 6000 cP @135C
viscosity_target_cps=6000.0
# 黏度容差 (%) - 推荐 ≤15%（当前27%过宽）
viscosity_tolerance_pct=15.0

[Machine]
# 最大速度 (mm/s) - 行业标杆 100-800 mm/s
# 当前10mm/s严重偏低，建议根据硬件能力提升
# 注意：需确认伺服驱动器和机械结构支持
max_speed=100.0

# 最大加速度 (mm/s²) - 行业标杆 500-2000 mm/s²
# 当前10mm/s²严重偏低
max_acceleration=500.0

# 定位精度 (mm) - 无痕内衣推荐 ≤0.05mm
positioning_tolerance=0.05

# 每毫米脉冲数 - 硬件限制，不可更改
# pulse_per_mm=200 (固定值，由电机编码器和丝杆导程决定)
```

---

### Phase 4: 端口接口扩展（可选）

**文件**: `src/domain/configuration/ports/IConfigurationPort.h`

**变更内容**:
添加工艺预设加载接口

```cpp
/**
 * @brief 加载工艺预设配置
 * @param preset 工艺预设类型
 * @return Result<DispensingConfig> 预设配置
 */
virtual Result<DispensingConfig> LoadDispensingPreset(DispensingPreset preset) = 0;

/**
 * @brief 获取配置最佳实践建议
 * @return Result<std::vector<std::string>> 建议列表
 */
virtual Result<std::vector<std::string>> GetBestPracticeRecommendations() const = 0;
```

**架构合规性**:
- 纯虚函数定义在端口接口
- 返回 `Result<T>` 类型
- 不依赖基础设施实现细节

---

## 文件变更清单

| 文件 | 变更类型 | 优先级 |
|------|----------|--------|
| `src/domain/configuration/ports/IConfigurationPort.h` | 修改 | P1 |
| `src/infrastructure/adapters/system/configuration/validators/ConfigValidator.h` | 修改 | P1 |
| `src/infrastructure/adapters/system/configuration/validators/ConfigValidator.cpp` | 修改 | P1 |
| `build/config/machine_config.ini` | 修改 | P2 |
| `src/infrastructure/adapters/system/configuration/ConfigFileAdapter.cpp` | 修改 | P2 |

---

## 验证方式

1. **单元测试**: 验证新增验证规则正确触发警告
2. **集成测试**: 验证配置加载后最佳实践建议正确生成
3. **手动测试**: 修改配置文件，确认警告信息可读

---

## 风险与缓解

| 风险 | 缓解措施 |
|------|----------|
| 运动参数提升可能超出硬件能力 | 保留原值作为注释，提示用户根据硬件调整 |
| 温度/黏度参数与实际胶水不匹配 | 使用警告而非错误，不阻断流程 |
| 配置文件格式变更导致兼容性问题 | 新增字段使用默认值，向后兼容 |

---

## 不在本方案范围

- 运动控制算法优化
- 硬件驱动层修改
- CMP触发方式从软件切换到硬件
- 视觉反馈系统集成

---

## 参考资料

- [PUR Hot Melt Adhesive 5701 - GC Adhesives](https://www.gcadhesives.com/pur-hot-melt-adhesive-5701/)
- [FISNAR F4500N Desktop Robot Specifications](https://www.esdshop.eu/stolovy-3-osovy-robot-3-fisnar/)
- [Types of Bonding Machines for Seamless Women's Underwear](https://www.alsterindustry.com/bonding-machines-for-seamless-womens-underwear/)
