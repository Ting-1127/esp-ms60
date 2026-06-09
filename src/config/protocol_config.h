#pragma once

/**
 * @file    protocol_config.h
 * @brief   APP 控制协议配置
 */

#define PROTO_VERSION                "ms60.v1"
#define PROTO_JSON_DOC_BYTES         768

#define PROTO_TYPE_CMD               "cmd"
#define PROTO_TYPE_RSP               "rsp"
#define PROTO_TYPE_EVT               "evt"

#define PROTO_CODE_OK                "ok"
#define PROTO_CODE_BAD_JSON          "bad_json"
#define PROTO_CODE_BAD_PROTO         "bad_proto"
#define PROTO_CODE_BAD_TYPE          "bad_type"
#define PROTO_CODE_MISSING_CMD       "missing_cmd"
#define PROTO_CODE_UNKNOWN_CMD       "unknown_cmd"
#define PROTO_CODE_NOT_IMPLEMENTED   "not_implemented"
#define PROTO_CODE_INTERNAL_ERROR    "internal_error"

#define PROTO_CMD_SYS_INFO           "sys.info"
#define PROTO_CMD_SYS_REBOOT         "sys.reboot"
#define PROTO_CMD_DIAG_STATUS        "diag.status"

#define PROTO_EVT_SYS_READY          "sys.ready"
