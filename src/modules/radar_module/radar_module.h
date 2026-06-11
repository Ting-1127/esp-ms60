#pragma once

/**
 * @file    radar_module.h
 * @brief   MS60 毫米波雷达 BSD 盲区监测模块
 *
 * 通过 UART2 接收 AT6010 SOC HCI 协议数据,
 * 解析 TYPE=7 BSD 目标帧, 通过事件队列转发给 BleControlModule。
 */

#include "project.h"
#include "services/service_manager.h"
#include <HardwareSerial.h>

struct BsdTarget {
    int8_t range_m;     // 距离, 米
    int8_t angle_deg;   // 角度, 度
    int8_t speed_ms;    // 速度, m/s (正=远离, 负=接近)
    int8_t obj_id;      // 目标 ID
};

class RadarModule : private NonCopyable, public IModule {
public:
    static RadarModule& instance() {
        static RadarModule inst;
        return inst;
    }

    bool begin() override;
    void loop() override;

    // BSD 状态
    bool is_radar_ok() const { return _radar_ok; }
    int get_target_count() const { return _target_count; }
    unsigned long get_last_update_ms() const { return _last_update_ms; }

    // 事件接口 (供 BleControlModule 轮询)
    bool has_pending_event() const { return _has_event; }
    String take_pending_event();

private:
    RadarModule() = default;

    // HCI 帧解析状态机
    enum class ParseState { WAIT_HEAD, READ_LEN, READ_TYPE, READ_PAYLOAD, READ_CHECK };

    void process_uart_data();
    void process_frame(uint8_t type, const uint8_t* payload, uint16_t len);
    void handle_bsd_data(const uint8_t* payload, uint16_t len);
    void emit_event(const String& json);

    HardwareSerial _serial{2};
    ParseState _state = ParseState::WAIT_HEAD;

    // 帧缓冲
    uint8_t _frame_buf[256];
    uint16_t _frame_len = 0;      // 期望 payload 长度 (LEN 字段)
    uint16_t _frame_read = 0;     // 已读取 payload 字节数
    uint8_t _frame_type = 0;
    uint8_t _frame_head_checksum = 0; // HEAD + LEN + TYPE 的累加和

    // BSD 数据
    BsdTarget _targets[8];
    int _target_count = 0;
    bool _radar_ok = false;
    unsigned long _last_update_ms = 0;

    // 事件队列 (单 slot)
    bool _has_event = false;
    String _pending_event;
};
