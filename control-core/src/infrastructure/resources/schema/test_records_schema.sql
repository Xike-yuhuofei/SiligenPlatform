-- 硬件测试数据库Schema
-- Feature: 007-hardware-test-functions
-- 本文件定义SQLite数据库结构,用于存储测试记录和结果

-- ===========================================
-- 测试记录主表
-- ===========================================

CREATE TABLE IF NOT EXISTS test_records (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp INTEGER NOT NULL,           -- Unix时间戳(秒)
    test_type TEXT NOT NULL,              -- 测试类型: 'jog'/'homing'/'io'/'trigger'/'cmp'/'interpolation'/'diagnostic'
    operator TEXT NOT NULL,               -- 操作人员名称
    status TEXT NOT NULL,                 -- 测试状态: 'success'/'failure'/'error'
    duration_ms INTEGER NOT NULL,         -- 测试耗时(毫秒)
    error_message TEXT,                   -- 错误信息(仅失败时)
    parameters_json TEXT NOT NULL,        -- 测试参数(JSON格式)
    result_json TEXT NOT NULL             -- 测试结果(JSON格式)
);

-- 索引优化查询性能
CREATE INDEX IF NOT EXISTS idx_timestamp ON test_records(timestamp);
CREATE INDEX IF NOT EXISTS idx_test_type ON test_records(test_type);
CREATE INDEX IF NOT EXISTS idx_operator ON test_records(operator);
CREATE INDEX IF NOT EXISTS idx_status ON test_records(status);
CREATE INDEX IF NOT EXISTS idx_test_type_timestamp ON test_records(test_type, timestamp);

-- ===========================================
-- 点动测试数据表
-- ===========================================

CREATE TABLE IF NOT EXISTS jog_test_data (
    record_id INTEGER PRIMARY KEY,
    axis INTEGER NOT NULL,                -- 移动轴(0=X, 1=Y, 2=Z, 3=U)
    direction TEXT NOT NULL,              -- 移动方向: 'Positive'/'Negative'
    speed REAL NOT NULL,                  -- 移动速度(mm/s)
    distance REAL NOT NULL,               -- 期望移动距离(mm)
    start_position REAL NOT NULL,         -- 起始位置(mm)
    end_position REAL NOT NULL,           -- 结束位置(mm)
    actual_distance REAL NOT NULL,        -- 实际移动距离(mm)
    response_time_ms INTEGER NOT NULL,    -- 响应时间(毫秒)
    FOREIGN KEY(record_id) REFERENCES test_records(id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_jog_axis ON jog_test_data(axis);

-- ===========================================
-- 回零测试数据表
-- ===========================================

CREATE TABLE IF NOT EXISTS homing_test_data (
    record_id INTEGER PRIMARY KEY,
    axes TEXT NOT NULL,                   -- 回零轴列表(JSON数组)
    mode TEXT NOT NULL,                   -- 回零模式: 'SingleAxis'/'MultiAxisSync'/'Sequential'
    search_speed REAL NOT NULL,           -- 搜索速度(mm/s)
    return_speed REAL NOT NULL,           -- 返回速度(mm/s)
    direction TEXT NOT NULL,              -- 回零方向: 'Positive'/'Negative'
    axis_results_json TEXT NOT NULL,      -- 每个轴的回零结果(JSON)
    limit_states_json TEXT NOT NULL,      -- 限位开关状态(JSON)
    FOREIGN KEY(record_id) REFERENCES test_records(id) ON DELETE CASCADE
);

-- ===========================================
-- I/O测试数据表
-- ===========================================

CREATE TABLE IF NOT EXISTS io_test_data (
    record_id INTEGER PRIMARY KEY,
    test_duration_ms INTEGER NOT NULL,    -- 测试持续时间(毫秒)
    output_tests_json TEXT NOT NULL,      -- 输出端口测试结果(JSON)
    input_tests_json TEXT NOT NULL,       -- 输入端口测试结果(JSON)
    FOREIGN KEY(record_id) REFERENCES test_records(id) ON DELETE CASCADE
);

-- ===========================================
-- 触发测试数据表
-- ===========================================

CREATE TABLE IF NOT EXISTS trigger_test_data (
    record_id INTEGER PRIMARY KEY,
    trigger_type TEXT NOT NULL,           -- 触发类型: 'SinglePoint'/'MultiPoint'/'ZoneTrigger'
    trigger_points_json TEXT NOT NULL,    -- 触发点配置(JSON)
    trigger_events_json TEXT NOT NULL,    -- 实际触发事件(JSON)
    statistics_json TEXT NOT NULL,        -- 统计数据(JSON)
    FOREIGN KEY(record_id) REFERENCES test_records(id) ON DELETE CASCADE
);

-- ===========================================
-- CMP补偿测试数据表
-- ===========================================

CREATE TABLE IF NOT EXISTS cmp_test_data (
    record_id INTEGER PRIMARY KEY,
    trajectory_type TEXT NOT NULL,        -- 轨迹类型: 'Linear'/'CircularArc'/'BezierCurve'/'BSpline'
    control_points_json TEXT NOT NULL,    -- 控制点(JSON)
    cmp_params_json TEXT NOT NULL,        -- CMP参数(JSON)
    theoretical_path_json TEXT NOT NULL,  -- 理论轨迹点(JSON)
    actual_path_json TEXT NOT NULL,       -- 实际轨迹点(JSON)
    analysis_json TEXT NOT NULL,          -- 精度分析(JSON)
    FOREIGN KEY(record_id) REFERENCES test_records(id) ON DELETE CASCADE
);

-- ===========================================
-- 插补测试数据表
-- ===========================================

CREATE TABLE IF NOT EXISTS interpolation_test_data (
    record_id INTEGER PRIMARY KEY,
    interpolation_type TEXT NOT NULL,     -- 插补类型: 'Linear'/'CircularArc'/'BSpline'/'Bezier'
    control_points_json TEXT NOT NULL,    -- 控制点(JSON)
    interp_params_json TEXT NOT NULL,     -- 插补参数(JSON)
    interpolated_path_json TEXT NOT NULL, -- 插补轨迹(JSON)
    path_quality_json TEXT NOT NULL,      -- 路径质量指标(JSON)
    motion_quality_json TEXT NOT NULL,    -- 运动质量指标(JSON)
    FOREIGN KEY(record_id) REFERENCES test_records(id) ON DELETE CASCADE
);

-- ===========================================
-- 系统诊断结果表
-- ===========================================

CREATE TABLE IF NOT EXISTS diagnostic_result (
    record_id INTEGER PRIMARY KEY,
    hardware_check_json TEXT NOT NULL,    -- 硬件检查结果(JSON)
    comm_check_json TEXT NOT NULL,        -- 通信检查结果(JSON)
    response_check_json TEXT NOT NULL,    -- 响应时间检查结果(JSON)
    accuracy_check_json TEXT NOT NULL,    -- 精度检查结果(JSON)
    health_score INTEGER NOT NULL,        -- 健康评分(0-100)
    issues_json TEXT NOT NULL,            -- 问题列表(JSON)
    FOREIGN KEY(record_id) REFERENCES test_records(id) ON DELETE CASCADE
);

-- ===========================================
-- 数据清理触发器
-- ===========================================

-- 自动清理超出MaxStoredRecords限制的旧记录
-- 注意: 此触发器需要在应用层配合实现,SQLite不直接支持SELECT子查询在DELETE触发器中
-- 应用层应定期调用清理函数

-- ===========================================
-- 视图: 最近测试记录摘要
-- ===========================================

CREATE VIEW IF NOT EXISTS recent_tests_summary AS
SELECT
    id,
    datetime(timestamp, 'unixepoch', 'localtime') as test_time,
    test_type,
    operator,
    status,
    duration_ms,
    CASE
        WHEN error_message IS NULL THEN '成功'
        ELSE error_message
    END as result_summary
FROM test_records
ORDER BY timestamp DESC
LIMIT 1000;

-- ===========================================
-- 视图: 测试统计
-- ===========================================

CREATE VIEW IF NOT EXISTS test_statistics AS
SELECT
    test_type,
    COUNT(*) as total_tests,
    SUM(CASE WHEN status = 'success' THEN 1 ELSE 0 END) as success_count,
    SUM(CASE WHEN status = 'failure' THEN 1 ELSE 0 END) as failure_count,
    SUM(CASE WHEN status = 'error' THEN 1 ELSE 0 END) as error_count,
    ROUND(AVG(duration_ms), 2) as avg_duration_ms,
    COUNT(DISTINCT operator) as unique_operators
FROM test_records
GROUP BY test_type;

-- ===========================================
-- Schema版本管理
-- ===========================================

CREATE TABLE IF NOT EXISTS schema_version (
    version INTEGER PRIMARY KEY,
    applied_at INTEGER NOT NULL,          -- Unix时间戳
    description TEXT NOT NULL
);

-- 插入初始版本
INSERT OR IGNORE INTO schema_version (version, applied_at, description)
VALUES (1, strftime('%s', 'now'), 'Initial schema for hardware test functions');

-- ===========================================
-- 数据库初始化完成
-- ===========================================

-- 使用方式:
-- 1. 应用启动时执行此SQL文件创建表结构
-- 2. 使用TestRecordRepository类进行CRUD操作
-- 3. 定期调用清理函数维护数据库大小
