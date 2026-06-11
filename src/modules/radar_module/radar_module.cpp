#include "radar_module.h"
#include "config/protocol_config.h"
#include "services/settings_store.h"
#include <ArduinoJson.h>
#include <Preferences.h>

// HCI 帧头
#define HCI_HEAD 0x5A
#define HCI_TYPE_BSD 7

// NVS 命名空间
static const char* BSD_NS = "bsd";

// ===================== sin/cos 查表 (Q15 定点) ======================
// 索引 = angle + 40, 范围 -40° ~ +40°
const int16_t RadarModule::SIN_Q15[81] = {
    -21063, -20622, -20174, -19720, -19261, -18795, -18324, -17847, -17364, -16877,
    -16384, -15886, -15384, -14876, -14365, -13848, -13328, -12803, -12275, -11743,
    -11207, -10668, -10126, -9580,  -9032,  -8481,  -7927,  -7371,  -6813,  -6252,
    -5690,  -5126,  -4560,  -3993,  -3425,  -2856,  -2286,  -1715,  -1144,  -572,
    0,      572,    1144,   1715,   2286,   2856,   3425,   3993,   4560,   5126,
    5690,   6252,   6813,   7371,   7927,   8481,   9032,   9580,   10126,  10668,
    11207,  11743,  12275,  12803,  13328,  13848,  14365,  14876,  15384,  15886,
    16384,  16877,  17364,  17847,  18324,  18795,  19261,  19720,  20174,  20622,
    21063
};

const int16_t RadarModule::COS_Q15[81] = {
    25102, 25466, 25822, 26170, 26510, 26842, 27166, 27482, 27789, 28088,
    28378, 28660, 28932, 29197, 29452, 29698, 29935, 30163, 30382, 30592,
    30792, 30983, 31164, 31336, 31499, 31651, 31795, 31928, 32052, 32166,
    32270, 32365, 32449, 32524, 32588, 32643, 32688, 32723, 32748, 32763,
    32767, 32763, 32748, 32723, 32688, 32643, 32588, 32524, 32449, 32365,
    32270, 32166, 32052, 31928, 31795, 31651, 31499, 31336, 31164, 30983,
    30792, 30592, 30382, 30163, 29935, 29698, 29452, 29197, 28932, 28660,
    28378, 28088, 27789, 27482, 27166, 26842, 26510, 26170, 25822, 25466,
    25102
};

// ===================== 初始化 =======================================

bool RadarModule::begin() {
    LOG_INFO("雷达模块", "初始化 UART2, RX=GPIO%d TX=GPIO%d, 921600bps",
             PIN_UART2_RX, PIN_UART2_TX);
    _serial.begin(921600, SERIAL_8N1, PIN_UART2_RX, PIN_UART2_TX);
    _state = ParseState::WAIT_HEAD;

    // 加载参数
    load_config();

    // 初始化 BSD
    target_track_init();
    bsd_warning_clear();

    _radar_ok = false;
    _raw_count = 0;
    _last_update_ms = 0;
    _last_event_ms = 0;

    LOG_INFO("雷达模块", "BSD 预警初始化完成, L1=%ld L2=%ld L3=%ld mm",
             _config.level1_dist, _config.level2_dist, _config.level3_dist);
    return true;
}

void RadarModule::loop() {
    process_uart_data();

    // 定期发送 bsd.status 事件
    unsigned long now = millis();
    if (now - _last_event_ms >= EVENT_INTERVAL_MS) {
        _last_event_ms = now;

        StaticJsonDocument<768> doc;
        doc["proto"] = PROTO_VERSION;
        doc["type"] = PROTO_TYPE_EVT;
        doc["event"] = PROTO_EVT_BSD_STATUS;

        JsonObject data = doc.createNestedObject("data");

        JsonObject left = data.createNestedObject("left");
        left["level"] = (uint8_t)_zone_level[BSD_ZONE_LEFT];
        left["lca_level"] = (uint8_t)_zone_lca_level[BSD_ZONE_LEFT];

        JsonObject right = data.createNestedObject("right");
        right["level"] = (uint8_t)_zone_level[BSD_ZONE_RIGHT];
        right["lca_level"] = (uint8_t)_zone_lca_level[BSD_ZONE_RIGHT];

        JsonObject rear = data.createNestedObject("rear");
        rear["level"] = (uint8_t)_zone_level[BSD_ZONE_REAR];

        data["target_count"] = _raw_count;
        data["radar_ok"] = _radar_ok;

        JsonArray targets = data.createNestedArray("targets");
        for (uint8_t i = 0; i < _target_info_count; i++) {
            JsonObject t = targets.createNestedObject();
            t["angle"] = _target_info[i].angle;
            t["dist_m"] = _target_info[i].dist_m;
            t["level"] = _target_info[i].level;
        }

        String json;
        serializeJson(doc, json);
        _pending_event = json;
        _has_event = true;
    }
}

// ===================== HCI 帧解析 ===================================

void RadarModule::process_uart_data() {
    while (_serial.available()) {
        uint8_t byte = _serial.read();

        switch (_state) {
            case ParseState::WAIT_HEAD:
                if (byte == HCI_HEAD) {
                    _frame_head_checksum = byte;
                    _state = ParseState::READ_LEN;
                }
                break;

            case ParseState::READ_LEN:
                _frame_len = byte;
                _frame_head_checksum += byte;
                _frame_read = 0;
                if (_frame_len == 0) {
                    _state = ParseState::WAIT_HEAD;
                } else {
                    _state = ParseState::READ_TYPE;
                }
                break;

            case ParseState::READ_TYPE:
                _frame_type = byte;
                _frame_head_checksum += byte;
                if (_frame_len <= 1) {
                    _state = ParseState::READ_CHECK;
                } else {
                    _state = ParseState::READ_PAYLOAD;
                }
                break;

            case ParseState::READ_PAYLOAD:
                if (_frame_read < sizeof(_frame_buf)) {
                    _frame_buf[_frame_read] = byte;
                }
                _frame_read++;
                if (_frame_read >= _frame_len - 1) {
                    _state = ParseState::READ_CHECK;
                }
                break;

            case ParseState::READ_CHECK: {
                uint8_t expected = _frame_head_checksum;
                uint16_t plen = _frame_len - 1;
                if (plen > sizeof(_frame_buf)) plen = sizeof(_frame_buf);
                for (uint16_t i = 0; i < plen; i++) {
                    expected += _frame_buf[i];
                }
                expected &= 0xFF;

                if (byte == expected) {
                    process_frame(_frame_type, _frame_buf, _frame_len - 1);
                }
                _state = ParseState::WAIT_HEAD;
                break;
            }
        }
    }
}

void RadarModule::process_frame(uint8_t type, const uint8_t* payload, uint16_t len) {
    if (type == HCI_TYPE_BSD) {
        handle_bsd_data(payload, len);
    }
}

void RadarModule::handle_bsd_data(const uint8_t* payload, uint16_t len) {
    if (len < 4) return;

    uint16_t obj_num = payload[0] | (payload[1] << 8);
    if (obj_num > TARGET_MAX) obj_num = TARGET_MAX;

    _raw_count = 0;
    const uint8_t* obj_data = payload + 4;
    uint16_t available = len - 4;

    for (uint16_t i = 0; i < obj_num && (i * 4 + 3) < available; i++) {
        const uint8_t* p = obj_data + i * 4;
        _raw_targets[i].distance = p[0];
        _raw_targets[i].angle   = (int8_t)p[1];
        _raw_targets[i].speed   = (int8_t)p[2];
        _raw_targets[i].id      = p[3];
        _raw_count++;
    }

    _radar_ok = true;
    _last_update_ms = millis();

    // 执行 BSD 预警逻辑
    bsd_warning_update();
}

// ===================== BSD 预警逻辑 ==================================

void RadarModule::target_track_init() {
    for (int i = 0; i < TARGET_MAX_TRACK; i++) {
        _tracks[i].id = 0;
        _tracks[i].age = 0;
        _tracks[i].lost = 0;
        _tracks[i].flags = 0;
        _tracks[i].y_prev_mm = 0;
        _tracks[i].speed_prev = 0;
    }
}

uint16_t RadarModule::target_track_update() {
    uint16_t matched = 0;
    uint16_t stable_mask = 0;

    for (uint8_t i = 0; i < _raw_count; i++) {
        uint8_t id = _raw_targets[i].id;
        bool found = false;

        for (uint8_t j = 0; j < TARGET_MAX_TRACK; j++) {
            if (_tracks[j].age > 0 && _tracks[j].id == id) {
                _tracks[j].age++;
                if (_tracks[j].age > 250) _tracks[j].age = 250;
                _tracks[j].lost = 0;
                matched |= (1 << j);
                found = true;
                if (_tracks[j].age >= TARGET_STABLE_AGE) {
                    stable_mask |= (1 << i);
                }
                break;
            }
        }

        if (!found) {
            for (uint8_t j = 0; j < TARGET_MAX_TRACK; j++) {
                if (_tracks[j].age == 0) {
                    _tracks[j].id = id;
                    _tracks[j].age = 1;
                    _tracks[j].lost = 0;
                    _tracks[j].flags = 0;
                    _tracks[j].y_prev_mm = 0;
                    _tracks[j].speed_prev = 0;
                    matched |= (1 << j);
                    break;
                }
            }
        }
    }

    // 清理丢失目标
    for (uint8_t j = 0; j < TARGET_MAX_TRACK; j++) {
        if (_tracks[j].age > 0 && !(matched & (1 << j))) {
            _tracks[j].lost++;
            if (_tracks[j].lost >= 3) {
                _tracks[j].age = 0;
            }
        }
    }

    return stable_mask;
}

uint8_t RadarModule::target_get_dead_zone_flag(uint8_t id) {
    for (uint8_t j = 0; j < TARGET_MAX_TRACK; j++) {
        if (_tracks[j].age > 0 && _tracks[j].id == id) {
            return (_tracks[j].flags & 0x01) != 0;
        }
    }
    return 0;
}

void RadarModule::target_set_dead_zone_flag(uint8_t id, uint8_t in_dead) {
    for (uint8_t j = 0; j < TARGET_MAX_TRACK; j++) {
        if (_tracks[j].age > 0 && _tracks[j].id == id) {
            if (in_dead) _tracks[j].flags |= 0x01;
            else _tracks[j].flags &= ~0x01;
            return;
        }
    }
}

uint8_t RadarModule::target_is_receding(uint8_t id, int32_t y_mm, int8_t speed) {
    for (uint8_t j = 0; j < TARGET_MAX_TRACK; j++) {
        if (_tracks[j].age > 0 && _tracks[j].id == id) {
            int32_t prev_y = _tracks[j].y_prev_mm;
            int8_t prev_sp = _tracks[j].speed_prev;
            _tracks[j].y_prev_mm = y_mm;
            _tracks[j].speed_prev = speed;
            if (prev_y <= 0) return 0;
            if (y_mm > prev_y && speed > prev_sp) return 1;
            return (y_mm > prev_y + TARGET_RECEDE_THRESH_MM);
        }
    }
    return 0;
}

BsdLevel RadarModule::get_level_by_distance(int32_t y_mm) const {
    if (y_mm < _config.level3_dist) return BSD_LEVEL_3;
    if (y_mm < _config.level2_dist) return BSD_LEVEL_2;
    if (y_mm <= _config.level1_dist) return BSD_LEVEL_1;
    return BSD_LEVEL_NONE;
}

BsdLevel RadarModule::level_max(BsdLevel a, BsdLevel b) const {
    return (a > b) ? a : b;
}

uint8_t RadarModule::hold_frames(BsdLevel level) const {
    switch (level) {
        case BSD_LEVEL_3: return LEVEL_HOLD_FRAMES_L3;
        case BSD_LEVEL_2: return LEVEL_HOLD_FRAMES_L2;
        default:          return LEVEL_HOLD_FRAMES_L1;
    }
}

void RadarModule::bsd_warning_clear() {
    for (int i = 0; i < BSD_ZONE_MAX; i++) {
        _zone_level[i] = BSD_LEVEL_NONE;
        _zone_lca_level[i] = BSD_LEVEL_NONE;
        _zone_level_prev[i] = BSD_LEVEL_NONE;
        _zone_level_hold[i] = 0;
    }
    _target_info_count = 0;
    _radar_ok = false;
    _last_data_time = 0;
}

void RadarModule::bsd_warning_update() {
    // 清空本轮
    for (int i = 0; i < BSD_ZONE_MAX; i++) {
        _zone_level[i] = BSD_LEVEL_NONE;
        _zone_lca_level[i] = BSD_LEVEL_NONE;
    }
    _target_info_count = 0;

    _last_data_time = millis();
    uint16_t stable_mask = target_track_update();

    for (uint8_t i = 0; i < _raw_count; i++) {
        int32_t dist_mm = (int32_t)_raw_targets[i].distance * BSD_DIST_SCALE_FACTOR;
        int8_t angle = _raw_targets[i].angle;
        int8_t speed = _raw_targets[i].speed;

        if (angle < -BSD_ANGLE_MAX || angle > BSD_ANGLE_MAX) continue;
        if (dist_mm == 0) continue;

        // 灵敏度缩放
        if (_config.sensitivity > 0) {
            int32_t scaled = dist_mm * (100 - _config.sensitivity) / 100;
            if (scaled < dist_mm) dist_mm = scaled;
        }

        // 极坐标→笛卡尔
        int idx = (int)angle + 40;
        int16_t sin_val = SIN_Q15[idx];
        int16_t cos_val = COS_Q15[idx];

        int32_t x_mm = (dist_mm * (int32_t)sin_val) >> 15;
        int32_t y_mm = (dist_mm * (int32_t)cos_val) >> 15;
        int32_t mask = x_mm >> 31;
        int32_t abs_x_mm = (x_mm ^ mask) - mask;

        if (y_mm <= 0) continue;

        uint8_t target_id = _raw_targets[i].id;
        uint8_t is_receding = target_is_receding(target_id, y_mm, speed);

        // 只处理稳定目标
        if (!(stable_mask & (1 << i))) continue;

        BsdLevel dist_level = get_level_by_distance(y_mm);
        if (dist_level == BSD_LEVEL_NONE) continue;

        // 速度过滤
        if (dist_level == BSD_LEVEL_3 && speed >= 2 && is_receding) continue;
        if (dist_level != BSD_LEVEL_3 && speed >= 0) continue;
        if (dist_level != BSD_LEVEL_3 && is_receding) continue;

        // 角度死区 + 区域路由
        uint8_t was_dead = target_get_dead_zone_flag(target_id);
        int dead_angle = was_dead ? _config.angle_dead_hyst : _config.angle_dead_zone;

        uint8_t via_dead_zone = (y_mm >= _config.level3_dist
                              && angle >= -dead_angle && angle <= dead_angle
                              && abs_x_mm < _config.rcw_half_width);
        uint8_t in_rcw = (abs_x_mm < _config.rcw_half_width) || via_dead_zone;

        if (in_rcw) {
            _zone_level[BSD_ZONE_REAR] = level_max(_zone_level[BSD_ZONE_REAR], dist_level);
            // TTC
            int32_t ttc_scaled = (int32_t)_config.ttc_threshold * BSD_DIST_SCALE_FACTOR;
            if (dist_mm < ttc_scaled * (int32_t)(-speed)) {
                _zone_level[BSD_ZONE_REAR] = level_max(_zone_level[BSD_ZONE_REAR], BSD_LEVEL_1);
            }
            target_set_dead_zone_flag(target_id, via_dead_zone && abs_x_mm >= _config.rcw_half_width);
        } else if (abs_x_mm < _config.lca_outer_edge) {
            target_set_dead_zone_flag(target_id, 0);
            if (x_mm < 0) {
                _zone_level[BSD_ZONE_LEFT] = level_max(_zone_level[BSD_ZONE_LEFT], dist_level);
                _zone_lca_level[BSD_ZONE_LEFT] = level_max(_zone_lca_level[BSD_ZONE_LEFT], dist_level);
                int32_t ttc_scaled = (int32_t)_config.ttc_threshold * BSD_DIST_SCALE_FACTOR;
                if (dist_mm < ttc_scaled * (int32_t)(-speed)) {
                    _zone_level[BSD_ZONE_LEFT] = level_max(_zone_level[BSD_ZONE_LEFT], BSD_LEVEL_1);
                    _zone_lca_level[BSD_ZONE_LEFT] = level_max(_zone_lca_level[BSD_ZONE_LEFT], BSD_LEVEL_1);
                }
            } else {
                _zone_level[BSD_ZONE_RIGHT] = level_max(_zone_level[BSD_ZONE_RIGHT], dist_level);
                _zone_lca_level[BSD_ZONE_RIGHT] = level_max(_zone_lca_level[BSD_ZONE_RIGHT], dist_level);
                int32_t ttc_scaled = (int32_t)_config.ttc_threshold * BSD_DIST_SCALE_FACTOR;
                if (dist_mm < ttc_scaled * (int32_t)(-speed)) {
                    _zone_level[BSD_ZONE_RIGHT] = level_max(_zone_level[BSD_ZONE_RIGHT], BSD_LEVEL_1);
                    _zone_lca_level[BSD_ZONE_RIGHT] = level_max(_zone_lca_level[BSD_ZONE_RIGHT], BSD_LEVEL_1);
                }
            }
        } else {
            target_set_dead_zone_flag(target_id, 0);
        }

        // 记录目标信息给 BLE
        if (_target_info_count < TARGET_MAX) {
            _target_info[_target_info_count].angle = angle;
            _target_info[_target_info_count].dist_m = _raw_targets[i].distance;
            _target_info[_target_info_count].level = (uint8_t)dist_level;
            _target_info_count++;
        }
    }

    // 帧间平滑
    for (int i = 0; i < BSD_ZONE_MAX; i++) {
        if (_zone_level[i] >= _zone_level_prev[i]) {
            _zone_level_prev[i] = _zone_level[i];
            _zone_level_hold[i] = hold_frames(_zone_level[i]);
        } else {
            if (_zone_level_hold[i] > 0) {
                _zone_level_hold[i]--;
                _zone_level[i] = _zone_level_prev[i];
            } else {
                _zone_level_prev[i] = _zone_level[i];
            }
        }
    }

    // 数据超时检查
    if (millis() - _last_data_time > BSD_DATA_TIMEOUT_MS) {
        bsd_warning_clear();
    }
}

// ===================== 预警查询 =====================================

BsdLevel RadarModule::get_zone_level(BsdZone zone) const {
    return (zone < BSD_ZONE_MAX) ? _zone_level[zone] : BSD_LEVEL_NONE;
}

BsdLevel RadarModule::get_zone_lca_level(BsdZone zone) const {
    return (zone < BSD_ZONE_MAX) ? _zone_lca_level[zone] : BSD_LEVEL_NONE;
}

// ===================== 可调参数 =====================================

void RadarModule::set_config(const BsdConfig& cfg) {
    _config = cfg;
    save_config();
    LOG_INFO("雷达模块", "参数更新: L1=%ld L2=%ld L3=%ld rcw=%ld lca=%ld adz=%d ttc=%d sens=%d",
             _config.level1_dist, _config.level2_dist, _config.level3_dist,
             _config.rcw_half_width, _config.lca_outer_edge,
             _config.angle_dead_zone, _config.ttc_threshold, _config.sensitivity);
}

static BsdConfig default_config() {
    BsdConfig cfg;
    cfg.level1_dist = 50000;
    cfg.level2_dist = 35000;
    cfg.level3_dist = 6093;
    cfg.rcw_half_width = 1000;
    cfg.lca_outer_edge = 5250;
    cfg.angle_dead_zone = 3;
    cfg.angle_dead_hyst = 5;
    cfg.ttc_threshold = 7;
    cfg.sensitivity = 0;
    return cfg;
}

void RadarModule::load_config() {
    Preferences prefs;
    prefs.begin(BSD_NS, true);

    if (prefs.isKey("l1_dist")) {
        _config.level1_dist = prefs.getInt("l1_dist", 50000);
        _config.level2_dist = prefs.getInt("l2_dist", 35000);
        _config.level3_dist = prefs.getInt("l3_dist", 6093);
        _config.rcw_half_width = prefs.getInt("rcw_hw", 1000);
        _config.lca_outer_edge = prefs.getInt("lca_outer", 5250);
        _config.angle_dead_zone = prefs.getChar("adz", 3);
        _config.angle_dead_hyst = prefs.getChar("adz_hyst", 5);
        _config.ttc_threshold = prefs.getUChar("ttc", 7);
        _config.sensitivity = prefs.getUChar("sens", 0);
        LOG_INFO("雷达模块", "从 NVS 加载参数");
    } else {
        _config = default_config();
        LOG_INFO("雷达模块", "使用默认参数");
    }

    prefs.end();
}

void RadarModule::save_config() {
    Preferences prefs;
    prefs.begin(BSD_NS, false);
    prefs.putInt("l1_dist", _config.level1_dist);
    prefs.putInt("l2_dist", _config.level2_dist);
    prefs.putInt("l3_dist", _config.level3_dist);
    prefs.putInt("rcw_hw", _config.rcw_half_width);
    prefs.putInt("lca_outer", _config.lca_outer_edge);
    prefs.putChar("adz", _config.angle_dead_zone);
    prefs.putChar("adz_hyst", _config.angle_dead_hyst);
    prefs.putUChar("ttc", _config.ttc_threshold);
    prefs.putUChar("sens", _config.sensitivity);
    prefs.end();
    LOG_INFO("雷达模块", "参数已保存到 NVS");
}

// ===================== 事件接口 =====================================

String RadarModule::take_pending_event() {
    _has_event = false;
    String evt = _pending_event;
    _pending_event = "";
    return evt;
}
