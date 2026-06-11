#pragma once

/**
 * @file    radar_module.h
 * @brief   MS60 毫米波雷达 BSD 盲区监测模块
 *
 * 通过 UART2 接收 AT6010 SOC HCI 协议数据,
 * 解析 TYPE=7 BSD 目标帧, 执行完整的 BSD 预警逻辑:
 * 极坐标→笛卡尔转换、目标跟踪、区域路由、距离分级、帧间平滑。
 * 预警结果通过事件队列转发给 BleControlModule。
 *
 * 移植自 STM32 端 bsd_warning.c
 */

#include "project.h"
#include "services/service_manager.h"
#include <HardwareSerial.h>

// ===================== 区域/等级枚举 ================================
enum BsdZone : uint8_t {
    BSD_ZONE_LEFT  = 0,
    BSD_ZONE_RIGHT = 1,
    BSD_ZONE_REAR  = 2,
    BSD_ZONE_MAX
};

enum BsdLevel : uint8_t {
    BSD_LEVEL_NONE = 0,
    BSD_LEVEL_1    = 1,   // 远端低风险
    BSD_LEVEL_2    = 2,   // 中风险
    BSD_LEVEL_3    = 3,   // 极近高风险
};

// ===================== 可调参数结构体 ================================
struct BsdConfig {
    int32_t level1_dist;      // LEVEL_1 最远距离 mm (默认 50000)
    int32_t level2_dist;      // LEVEL_2 最远距离 mm (默认 35000)
    int32_t level3_dist;      // LEVEL_3 最远距离 mm (默认 6093)
    int32_t rcw_half_width;   // RCW 中间区域半宽 mm (默认 1000)
    int32_t lca_outer_edge;   // LCA 外边界 mm (默认 5250)
    int8_t  angle_dead_zone;  // 角度死区 ° (默认 3)
    int8_t  angle_dead_hyst;  // 角度死区退出迟滞 ° (默认 5)
    uint8_t ttc_threshold;    // RCW TTC 阈值 s (默认 7)
    uint8_t sensitivity;      // 灵敏度 0-50 (默认 0, 越大越不灵敏)
};

// ===================== 原始目标结构 ==================================
struct BsdRawTarget {
    uint8_t distance;   // 距离 m
    int8_t  angle;      // 角度 °
    int8_t  speed;      // 速度 m/s
    uint8_t id;         // 目标 ID
};

// ===================== RadarModule ===================================
class RadarModule : private NonCopyable, public IModule {
public:
    static RadarModule& instance() {
        static RadarModule inst;
        return inst;
    }

    bool begin() override;
    void loop() override;

    // 雷达状态
    bool is_radar_ok() const { return _radar_ok; }
    unsigned long get_last_update_ms() const { return _last_update_ms; }

    // BSD 预警查询
    BsdLevel get_zone_level(BsdZone zone) const;
    BsdLevel get_zone_lca_level(BsdZone zone) const;
    uint8_t get_target_count() const { return _raw_count; }

    // 可调参数
    const BsdConfig& get_config() const { return _config; }
    void set_config(const BsdConfig& cfg);
    void load_config();
    void save_config();

    // 事件接口
    bool has_pending_event() const { return _has_event; }
    String take_pending_event();

private:
    RadarModule() = default;

    // ---- HCI 帧解析 ----
    enum class ParseState { WAIT_HEAD, READ_LEN, READ_TYPE, READ_PAYLOAD, READ_CHECK };

    void process_uart_data();
    void process_frame(uint8_t type, const uint8_t* payload, uint16_t len);
    void handle_bsd_data(const uint8_t* payload, uint16_t len);

    HardwareSerial _serial{2};
    ParseState _state = ParseState::WAIT_HEAD;
    uint8_t _frame_buf[128];
    uint16_t _frame_len = 0;
    uint16_t _frame_read = 0;
    uint8_t _frame_type = 0;
    uint8_t _frame_head_checksum = 0;

    // ---- BSD 预警逻辑 ----
    static constexpr int TARGET_MAX = 8;
    static constexpr int TARGET_MAX_TRACK = 16;
    static constexpr int TARGET_STABLE_AGE = 2;
    static constexpr int TARGET_RECEDE_THRESH_MM = 200;
    static constexpr int BSD_ANGLE_MAX = 40;
    static constexpr int BSD_DIST_SCALE_FACTOR = 1000;
    static constexpr int LEVEL_HOLD_FRAMES_L3 = 6;
    static constexpr int LEVEL_HOLD_FRAMES_L2 = 4;
    static constexpr int LEVEL_HOLD_FRAMES_L1 = 2;
    static constexpr uint32_t BSD_DATA_TIMEOUT_MS = 1000;

    struct TargetTrack {
        uint8_t id;
        uint8_t age;
        uint8_t lost;
        uint8_t flags;
        int32_t y_prev_mm;
        int8_t  speed_prev;
    };

    void bsd_warning_update();
    void bsd_warning_clear();
    BsdLevel get_level_by_distance(int32_t y_mm) const;
    BsdLevel level_max(BsdLevel a, BsdLevel b) const;
    uint8_t hold_frames(BsdLevel level) const;

    // 目标跟踪
    void target_track_init();
    uint16_t target_track_update();
    uint8_t target_get_dead_zone_flag(uint8_t id);
    void target_set_dead_zone_flag(uint8_t id, uint8_t in_dead);
    uint8_t target_is_receding(uint8_t id, int32_t y_mm, int8_t speed);

    // 原始目标数据
    BsdRawTarget _raw_targets[TARGET_MAX];
    uint8_t _raw_count = 0;
    bool _radar_ok = false;
    unsigned long _last_update_ms = 0;

    // 目标跟踪
    TargetTrack _tracks[TARGET_MAX_TRACK];

    // 预警结果
    BsdLevel _zone_level[BSD_ZONE_MAX];
    BsdLevel _zone_lca_level[BSD_ZONE_MAX];
    BsdLevel _zone_level_prev[BSD_ZONE_MAX];
    uint8_t _zone_level_hold[BSD_ZONE_MAX];
    unsigned long _last_data_time = 0;

    // 可调参数
    BsdConfig _config;

    // sin/cos 查表 (Q15 定点)
    static const int16_t SIN_Q15[81];
    static const int16_t COS_Q15[81];

    // 事件
    bool _has_event = false;
    String _pending_event;
    unsigned long _last_event_ms = 0;
    static constexpr unsigned long EVENT_INTERVAL_MS = 100;
};
