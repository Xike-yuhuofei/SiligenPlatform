# Hardware Configuration Guide

Complete hardware setup and configuration for the Siligen dispenser control system.

## Dispenser Valve Specifications

### FESTO MHE3-MS1H-3/2G-1/8-K

本系统使用 FESTO MHE3 系列微型电磁阀作为点胶控制阀:

| Parameter | Value | Description |
|-----------|-------|-------------|
| **Model** | MHE3-MS1H-3/2G-1/8-K | FESTO micro solenoid valve |
| **Part Number** | 525149 | FESTO product code |
| **Voltage** | 24V DC ±10% | DC power supply |
| **Power** | 6.5W | Coil power consumption |
| **Inrush Current** | 1A | Ion |
| **Protection** | IP65 | Dust and water resistant |
| **Pressure Range** | -0.09 ~ 0.8 MPa | (-0.9 ~ 8 bar) |
| **Valve Type** | 3/2G | 3-way, 2-position, single solenoid |
| **Port Size** | G1/8" | Thread connection |

### Response Time Characteristics

| Parameter | Typical Value | Description |
|-----------|---------------|-------------|
| **Opening Time** | 1.5 ~ 3 ms | Time from energized to fully open |
| **Closing Time** | 1 ~ 2 ms | Time from de-energized to fully closed |
| **Total Response** | 2.5 ~ 5 ms | Complete switching cycle |
| **Min Pulse Width** | ~5 ms | Minimum for complete valve actuation |

### Minimum Dispensing Time Calculation

System configured `min_duration_ms = 10 ms` based on:

```
Minimum Dispense Time = Valve Opening + Stabilization + Safety Margin
                      = 3 ms + 2 ms + 5 ms
                      = 10 ms
```

This provides approximately 2x safety margin over the hardware minimum.

---

## Network Configuration

### IP Settings
- **Control Card IP**: 192.168.0.1
- **Local Computer IP**: 192.168.0.200
- **Port Configuration**: Auto-assigned (set to 0 in code)
- **Subnet Mask**: 255.255.255.0

### Network Setup Steps
1. Configure local computer IP to 192.168.0.200
2. Connect network cable to control card
3. Verify ping connection to 192.168.0.1
4. Power cycle system if needed

## Power Requirements

### Control Card Power
- **Voltage**: 24V DC
- **Current**: Minimum 2A
- **Connector**: Standard DC barrel jack
- **Polarity**: Center-positive

### Power Supply Specifications
- Input: 100-240V AC, 50/60Hz
- Output: 24V DC, regulated
- Protection: Over-current, short-circuit

## Hardware Connections

### MultiCard Connection
```cpp
// Connection parameters (from Types.h)
constexpr const char* CONTROL_CARD_IP = "192.168.0.1";
constexpr const char* LOCAL_IP = "192.168.0.200";
constexpr uint16 CONTROL_CARD_PORT = 0;  // Auto-assign
constexpr uint16 LOCAL_PORT = 0;         // Auto-assign
```

### Physical Connection Diagram
```
Computer (192.168.0.200) ←→ Network Cable ←→ Control Card (192.168.0.1)
                                                    ↓
                                               24V DC Power
                                                    ↓
                                               Motion Axes
```

## Troubleshooting

### Network Issues
1. **Cannot ping control card**
   - Check power supply (24V DC)
   - Verify network cable connection
   - Confirm IP configuration

2. **Connection timeout**
   - Disable Windows firewall temporarily
   - Check for IP conflicts
   - Verify network adapter settings

### Power Issues
1. **Control card not powering on**
   - Verify 24V DC supply is working
   - Check power cable connections
   - Test with known good power supply

## Configuration Files

### Machine Configuration
- File: `config/machine_config.ini`
- Contains: [Network], [Dispensing], [CMP] sections
- Requires restart for changes to take effect

### Runtime Constants
- File: `src/core/Types.h`
- Contains: PULSES_PER_MM, IP addresses, port settings
- Requires recompile for changes

---
*For detailed API usage, see `src/hardware/MultiCardCPP.h`*