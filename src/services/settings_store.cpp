#include "settings_store.h"
#include <Preferences.h>

static Preferences s_prefs;
static const char* NS = "wifi";
static const char* KEY_SSID = "ssid";
static const char* KEY_PASS = "pass";
static const char* KEY_ENABLED = "wifi_en";

bool SettingsStore::begin() {
    LOG_INFO("设置存储", "NVS 初始化");
    return true;
}

bool SettingsStore::has_wifi_credentials() {
    s_prefs.begin(NS, true);
    bool has = s_prefs.isKey(KEY_SSID);
    s_prefs.end();
    return has;
}

bool SettingsStore::save_wifi_credentials(const String& ssid, const String& password) {
    s_prefs.begin(NS, false);
    size_t w1 = s_prefs.putString(KEY_SSID, ssid);
    size_t w2 = s_prefs.putString(KEY_PASS, password);
    s_prefs.end();
    bool ok = (w1 > 0);
    LOG_INFO("设置存储", "保存 WiFi 凭据: ssid=%s ok=%d", ssid.c_str(), ok);
    return ok;
}

bool SettingsStore::load_wifi_credentials(String& ssid, String& password) {
    s_prefs.begin(NS, true);
    ssid = s_prefs.getString(KEY_SSID, "");
    password = s_prefs.getString(KEY_PASS, "");
    s_prefs.end();
    return ssid.length() > 0;
}

void SettingsStore::clear_wifi_credentials() {
    s_prefs.begin(NS, false);
    s_prefs.clear();
    s_prefs.end();
    LOG_INFO("设置存储", "已清除 WiFi 凭据");
}

bool SettingsStore::load_wifi_enabled() {
    s_prefs.begin(NS, true);
    bool enabled = s_prefs.getBool(KEY_ENABLED, true);  // 默认开启
    s_prefs.end();
    return enabled;
}

void SettingsStore::save_wifi_enabled(bool enabled) {
    s_prefs.begin(NS, false);
    s_prefs.putBool(KEY_ENABLED, enabled);
    s_prefs.end();
    LOG_INFO("设置存储", "WiFi 开关: %s", enabled ? "开" : "关");
}
