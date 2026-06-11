#include "radar_module.h"
#include "config/protocol_config.h"
#include <ArduinoJson.h>

// HCI 帧头
#define HCI_HEAD 0x5A

// HCI TYPE 定义
#define HCI_TYPE_BSD 7

bool RadarModule::begin() {
    LOG_INFO("雷达模块", "初始化 UART2, RX=GPIO%d TX=GPIO%d, 波特率=921600",
             PIN_UART2_RX, PIN_UART2_TX);
    _serial.begin(921600, SERIAL_8N1, PIN_UART2_RX, PIN_UART2_TX);
    _state = ParseState::WAIT_HEAD;
    _radar_ok = false;
    _target_count = 0;
    _last_update_ms = 0;
    return true;
}

void RadarModule::loop() {
    process_uart_data();
}

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
                } else if (_frame_len == 1) {
                    // LEN 包含 TYPE 字节, 所以 LEN=1 意味着只有 TYPE, 无 payload
                    _state = ParseState::READ_TYPE;
                } else {
                    _state = ParseState::READ_TYPE;
                }
                break;

            case ParseState::READ_TYPE:
                _frame_type = byte;
                _frame_head_checksum += byte;
                if (_frame_len <= 1) {
                    // 无 payload, 直接读 CHECK
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
                uint8_t expected_check = _frame_head_checksum;
                // 累加 payload 字节
                uint16_t payload_len = _frame_len - 1;
                if (payload_len > sizeof(_frame_buf)) payload_len = sizeof(_frame_buf);
                for (uint16_t i = 0; i < payload_len; i++) {
                    expected_check += _frame_buf[i];
                }
                expected_check &= 0xFF;

                if (byte == expected_check) {
                    process_frame(_frame_type, _frame_buf, _frame_len - 1);
                } else {
                    LOG_WARN("雷达模块", "校验失败: type=%02X expected=%02X got=%02X",
                             _frame_type, expected_check, byte);
                }
                _state = ParseState::WAIT_HEAD;
                break;
            }
        }
    }
}

void RadarModule::process_frame(uint8_t type, const uint8_t* payload, uint16_t len) {
    switch (type) {
        case HCI_TYPE_BSD:
            handle_bsd_data(payload, len);
            break;
        default:
            // 其他帧类型忽略
            break;
    }
}

void RadarModule::handle_bsd_data(const uint8_t* payload, uint16_t len) {
    // 最小长度: obj_num(2) + reserved(2) = 4 字节
    if (len < 4) {
        LOG_WARN("雷达模块", "BSD 数据过短: len=%u", len);
        return;
    }

    uint16_t obj_num = payload[0] | (payload[1] << 8);
    // payload[2..3] = reserved

    if (obj_num > 8) obj_num = 8;

    _target_count = 0;
    const uint8_t* obj_data = payload + 4;
    uint16_t available = len - 4;

    for (uint16_t i = 0; i < obj_num && (i * 4 + 3) < available; i++) {
        const uint8_t* p = obj_data + i * 4;
        _targets[i].range_m   = (int8_t)p[0];
        _targets[i].angle_deg = (int8_t)p[1];
        _targets[i].speed_ms  = (int8_t)p[2];
        _targets[i].obj_id    = (int8_t)p[3];
        _target_count++;
    }

    _radar_ok = true;
    _last_update_ms = millis();

    // 构建 JSON 事件
    StaticJsonDocument<512> doc;
    doc["proto"] = PROTO_VERSION;
    doc["type"] = PROTO_TYPE_EVT;
    doc["event"] = PROTO_EVT_BSD_DETECTED;

    JsonObject data = doc.createNestedObject("data");
    data["count"] = _target_count;

    JsonArray targets = data.createNestedArray("targets");
    for (int i = 0; i < _target_count; i++) {
        JsonObject t = targets.createNestedObject();
        t["id"] = _targets[i].obj_id;
        t["range"] = _targets[i].range_m;
        t["angle"] = _targets[i].angle_deg;
        t["speed"] = _targets[i].speed_ms;
    }

    String json;
    serializeJson(doc, json);
    emit_event(json);
}

void RadarModule::emit_event(const String& json) {
    _pending_event = json;
    _has_event = true;
}

String RadarModule::take_pending_event() {
    _has_event = false;
    String evt = _pending_event;
    _pending_event = "";
    return evt;
}
