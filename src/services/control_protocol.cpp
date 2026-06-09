#include "control_protocol.h"

bool ControlProtocol::parse_request(const String& input,
                                    StaticJsonDocument<PROTO_JSON_DOC_BYTES>& doc,
                                    ControlRequest& request,
                                    String& error_code,
                                    String& error_message) {
    DeserializationError error = deserializeJson(doc, input);
    if (error) {
        error_code = PROTO_CODE_BAD_JSON;
        error_message = error.c_str();
        return false;
    }

    const char* proto = doc["proto"];
    if (!proto || strcmp(proto, PROTO_VERSION) != 0) {
        error_code = PROTO_CODE_BAD_PROTO;
        error_message = "protocol mismatch";
        return false;
    }

    const char* type = doc["type"];
    if (!type || strcmp(type, PROTO_TYPE_CMD) != 0) {
        error_code = PROTO_CODE_BAD_TYPE;
        error_message = "type must be cmd";
        return false;
    }

    const char* cmd = doc["cmd"];
    if (!cmd || cmd[0] == '\0') {
        error_code = PROTO_CODE_MISSING_CMD;
        error_message = "cmd is required";
        return false;
    }

    const char* id = doc["id"];
    request.id = id ? id : "";
    request.cmd = cmd;
    request.payload = doc["payload"].as<JsonVariantConst>();
    return true;
}

String ControlProtocol::make_response(const String& id,
                                      bool ok,
                                      const char* code,
                                      const char* message,
                                      JsonVariantConst data) {
    StaticJsonDocument<PROTO_JSON_DOC_BYTES> doc;
    doc["proto"] = PROTO_VERSION;
    doc["id"] = id;
    doc["type"] = PROTO_TYPE_RSP;
    doc["ok"] = ok;
    doc["code"] = code;
    doc["message"] = message;
    doc["data"] = data;

    String output;
    serializeJson(doc, output);
    return output;
}

String ControlProtocol::make_empty_response(const String& id,
                                            bool ok,
                                            const char* code,
                                            const char* message) {
    StaticJsonDocument<64> data;
    JsonObject object = data.to<JsonObject>();
    return make_response(id, ok, code, message, object);
}

String ControlProtocol::make_event(const char* event, JsonVariantConst data) {
    StaticJsonDocument<PROTO_JSON_DOC_BYTES> doc;
    doc["proto"] = PROTO_VERSION;
    doc["type"] = PROTO_TYPE_EVT;
    doc["event"] = event;
    doc["data"] = data;

    String output;
    serializeJson(doc, output);
    return output;
}

String ControlProtocol::make_empty_event(const char* event) {
    StaticJsonDocument<64> data;
    JsonObject object = data.to<JsonObject>();
    return make_event(event, object);
}
