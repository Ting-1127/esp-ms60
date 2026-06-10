#pragma once

/**
 * @file    settings_store.h
 * @brief   持久化设置存储
 */

#include "project.h"

class SettingsStore : private NonCopyable {
public:
    static SettingsStore& instance() {
        static SettingsStore inst;
        return inst;
    }

    bool begin();
    bool has_wifi_credentials();
    bool save_wifi_credentials(const String& ssid, const String& password);
    bool load_wifi_credentials(String& ssid, String& password);
    void clear_wifi_credentials();

private:
    SettingsStore() = default;
};
