#pragma once

/**
 * @file    control_protocol.h
 * @brief   APP 控制协议解析与序列化
 */

#include "project.h"
#include <ArduinoJson.h>

struct ControlRequest {
    String id;
    String cmd;
    JsonVariantConst payload;
};

class ControlProtocol {
public:
    static bool parse_request(const String& input,
                              StaticJsonDocument<PROTO_JSON_DOC_BYTES>& doc,
                              ControlRequest& request,
                              String& error_code,
                              String& error_message);

    static String make_response(const String& id,
                                bool ok,
                                const char* code,
                                const char* message,
                                JsonVariantConst data);

    static String make_empty_response(const String& id,
                                      bool ok,
                                      const char* code,
                                      const char* message);

    static String make_event(const char* event, JsonVariantConst data);
    static String make_empty_event(const char* event);
};
