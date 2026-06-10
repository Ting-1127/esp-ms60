#pragma once

#include "project.h"
#include "services/service_manager.h"

class OtaModule : private NonCopyable, public IModule {
public:
    static OtaModule& instance() {
        static OtaModule inst;
        return inst;
    }

    bool begin() override;
    void loop() override;

    bool start(const String& url, const String& version, const String& sha256);
    void cancel();

    struct Status {
        String status;    // "idle"|"downloading"|"installing"|"done"|"failed"
        int progress;     // 0-100
        String message;
    };
    Status get_status() const;

    bool has_pending_event();
    String take_pending_event();

private:
    OtaModule() = default;

    enum class State { Idle, Downloading, Installing, Done, Failed };

    void set_state(State s, int progress = 0, const String& msg = "");
    void run_ota();
    static void ota_task(void* param);

    State _state = State::Idle;
    int _progress = 0;
    String _message;
    String _url;
    String _version;
    String _sha256;
    bool _cancel_flag = false;

    // 事件
    bool _has_event = false;
    String _pending_event;
};
