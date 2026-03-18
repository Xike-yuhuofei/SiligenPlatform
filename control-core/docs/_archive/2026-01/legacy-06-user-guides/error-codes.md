# System Error Codes Reference

## Success Codes
- `0`: **SUCCESS** - Operation completed successfully

## Hardware Connection Errors (-6 to -9)
- `-6`: **CARD_OPEN_FAILURE** - Cannot open control card
  - **Cause**: Power off, network issue, IP mismatch
  - **Recovery**: Check 24V power, verify network cable, ping 192.168.0.1
  - **Prevention**: Use proper shutdown sequence

- `-7`: **RESPONSE_TIMEOUT** - No response from control card
  - **Cause**: Network timeout, card busy, cable issue
  - **Recovery**: Check network stability, retry connection
  - **Prevention**: Increase timeout values, check network quality

- `-8`: **AXIS_NOT_READY** - Axis not initialized or in error state
  - **Cause**: Homing not complete, limit switch triggered
  - **Recovery**: Execute homing sequence, check limit switches
  - **Prevention**: Proper system startup sequence

- `-9`: **INVALID_PARAMETER** - Parameter out of range
  - **Cause**: Invalid position, speed, or acceleration values
  - **Recovery**: Validate parameters before sending
  - **Prevention**: Use input validation functions

## System Execution Errors (1-10)
- `1`: **EXECUTION_FAILURE** - General execution error
  - **Cause**: Command rejected by hardware
  - **Recovery**: Check hardware state, retry with valid parameters
  - **Prevention**: Validate hardware state before commands

- `2`: **INVALID_CONFIGURATION** - Configuration parameters invalid
  - **Cause**: Config file corrupted, parameters out of range
  - **Recovery**: Reload default config, validate parameters
  - **Prevention**: Config validation on startup

- `3`: **MEMORY_ALLOCATION_ERROR** - Cannot allocate required memory
  - **Cause**: Insufficient RAM, memory fragmentation
  - **Recovery**: Free unused memory, restart application
  - **Prevention**: Memory pooling, avoid large allocations

- `4`: **FILE_NOT_FOUND** - Required file missing
  - **Cause**: Config file, DLL, or data file missing
  - **Recovery**: Check file paths, reinstall missing files
  - **Prevention**: File existence check on startup

- `5`: **PERMISSION_DENIED** - Insufficient system permissions
  - **Cause**: Access denied to hardware or files
  - **Recovery**: Run as administrator, check file permissions
  - **Prevention**: Proper installation with correct permissions

## Motion Control Errors (11-20)
- `11`: **MOTION_ABORTED** - Motion stopped by emergency stop
  - **Cause**: E-stop pressed, safety trigger
  - **Recovery**: Reset emergency stop, clear safety condition
  - **Prevention**: Proper safety circuit design

- `12`: **LIMIT_SWITCH_HIT** - Hardware limit triggered
  - **Cause**: Axis reached physical limit
  - **Recovery**: Move away from limit, check home position
  - **Prevention**: Set proper software limits

- `13`: **POSITION_OVERFLOW** - Position counter overflow
  - **Cause**: Exceeded maximum position range
  - **Recovery**: Reset position counter, re-home axis
  - **Prevention**: Use 64-bit position counters

## Network Communication Errors (21-30)
- `21`: **NETWORK_DISCONNECTED** - Lost connection to card
  - **Cause**: Network cable unplugged, card power loss
  - **Recovery**: Reconnect network cable, restore power
  - **Prevention**: Network monitoring, redundant connections

- `22`: **IP_ADDRESS_CONFLICT** - IP conflict on network
  - **Cause**: Another device using same IP address
  - **Recovery**: Change IP address, find conflicting device
  - **Prevention**: Use IP reservation in router

## Error Recovery Procedures

### Standard Recovery Pattern
1. **Log Error**: Record error code, timestamp, context
2. **Stop Motion**: Immediately halt all moving operations
3. **Safe State**: Move to known safe position if possible
4. **Diagnose**: Use diagnostic tools to identify root cause
5. **Recover**: Apply specific recovery procedure
6. **Verify**: Test system before resuming normal operation
7. **Document**: Record resolution for future reference

### Emergency Procedures
- **Emergency Stop**: Immediately cut power to motors
- **Power Cycle**: Complete system shutdown and restart
- **Safe Mode**: Operate with reduced functionality
- **Manual Recovery**: Manual intervention if automated recovery fails

## Debugging Tools
- **Connection Diagnostic**: `scripts\build_connection_diagnostic.bat`
- **System Monitor**: Real-time error tracking
- **Log Analysis**: Error pattern analysis
- **Hardware Test**: `connection_diagnostic.exe`

## Prevention Strategies
- **Regular Maintenance**: Scheduled hardware inspections
- **Monitoring**: Continuous system health monitoring
- **Backup Systems**: Redundant hardware where critical
- **Training**: Proper operator training procedures