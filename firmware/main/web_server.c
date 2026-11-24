#include "web_server.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_http_server.h"
#include "esp_ota_ops.h"
#include "lwip/sockets.h"
#include "cJSON.h"
#include "rule_engine.h"
#include "scheduler.h"
#include "sd_logger.h"
#include "event_bus.h"
#include "rtc.h"
#include "supervisor.h"
#include "wifi_provisioning.h"
#include "config_manager.h"
#include "mqtt_adapter.h"
#include "relay_controller.h"
#include "display.h"
#include "ble_adapter.h"

static const char *TAG = "WEB_SERVER";

static bool constant_time_eq(const char *a, const char *b) {
    if (!a || !b) return false;
    size_t la = strlen(a);
    size_t lb = strlen(b);
    if (la != lb) return false;
    unsigned char diff = 0;
    for (size_t i = 0; i < la; i++) {
        diff |= (unsigned char)(a[i] ^ b[i]);
    }
    return (diff == 0);
}

static bool check_api_auth(httpd_req_t *req) {
    char key_hdr[40];
    if (httpd_req_get_hdr_value_str(req, "X-API-Key", key_hdr, sizeof(key_hdr)) == ESP_OK) {
        const gateway_config_t *cfg = config_manager_get();
        if (cfg && cfg->api_key[0] != '\0' && constant_time_eq(key_hdr, cfg->api_key)) {
            return true;
        }
    }
    httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Missing or invalid X-API-Key header");
    return false;
}

static bool is_softap_client(httpd_req_t *req) {
    if (!req) return false;
    int sockfd = httpd_req_to_sockfd(req);
    if (sockfd < 0) return false;

    struct sockaddr_storage addr;
    socklen_t len = sizeof(addr);
    if (getsockname(sockfd, (struct sockaddr *)&addr, &len) != 0) {
        return false;
    }

    if (addr.ss_family == AF_INET) {
        return ntohl(((struct sockaddr_in *)&addr)->sin_addr.s_addr) == 0xC0A80401UL;
    }
    if (addr.ss_family == AF_INET6) {
        const uint8_t *b = ((const struct sockaddr_in6 *)&addr)->sin6_addr.s6_addr;
        static const uint8_t v4mapped_prefix[12] = {0,0,0,0,0,0,0,0,0,0,0xff,0xff};
        return (memcmp(b, v4mapped_prefix, 12) == 0 &&
                b[12] == 192 && b[13] == 168 && b[14] == 4 && b[15] == 1);
    }
    return false;
}

static httpd_handle_t s_server = NULL;

static const char SET_PASSWORD_HTML[] =
"<!DOCTYPE html><html lang='vi'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>Cài Đặt Wi-Fi & Mật Khẩu Gateway</title>"
"<style>"
"* { box-sizing: border-box; margin:0; padding:0; }"
"body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; background: #0d1117; color: #c9d1d9; padding: 16px; display: flex; justify-content: center; align-items: center; min-height: 100vh; }"
".card { background: #161b22; padding: 24px; border-radius: 12px; border: 1px solid #30363d; width: 100%; max-width: 420px; box-shadow: 0 8px 24px rgba(0,0,0,0.6); }"
"h2 { color: #58a6ff; margin: 0 0 6px; text-align: center; font-size: 19px; }"
"p.desc { color: #8b949e; font-size: 13px; text-align: center; margin: 0 0 16px; line-height: 1.5; }"
"label { font-size: 13px; font-weight: 600; display: block; margin-bottom: 6px; color: #e6edf3; }"
"input, select { width: 100%; padding: 10px 12px; border-radius: 6px; border: 1px solid #30363d; background: #0d1117; color: #fff; font-size: 14px; margin-bottom: 14px; outline: none; }"
"input:focus, select:focus { border-color: #58a6ff; }"
".btn-submit { width: 100%; padding: 12px; background: #238636; color: #fff; border: none; border-radius: 6px; font-weight: 600; font-size: 15px; cursor: pointer; margin-top: 8px; transition: 0.2s; }"
".btn-submit:hover { background: #2ea043; }"
".btn-rescan { float: right; background: #21262d; border: 1px solid #30363d; color: #58a6ff; padding: 2px 8px; font-size: 12px; border-radius: 4px; cursor: pointer; }"
".error { color: #f85149; font-size: 12px; margin-bottom: 12px; display: none; }"
".scan-status { font-size: 12px; color: #8b949e; margin-bottom: 8px; }"
".divider { border-top: 1px solid #30363d; margin: 16px 0; }"
".footer { margin-top: 16px; text-align: center; font-size: 11px; color: #484f58; }"
"</style></head>"
"<body><div class='card'>"
"<h2>⚙️ Cài Đặt Wi-Fi & Mật Khẩu</h2>"
"<p class='desc'>Chọn mạng Wi-Fi nhà bạn để Gateway kết nối Internet và đặt mật khẩu bảo vệ thiết bị.</p>"
"<form id='setup-form' onsubmit='submitSetup(event)'>"
"<div id='err-msg' class='error'></div>"
"<div style='display:flex;justify-content:space-between;align-items:center;margin-bottom:6px;'>"
"<label style='margin:0;'>Mạng Wi-Fi (SSID)</label>"
"<button type='button' class='btn-rescan' onclick='fetchScan(true)'>&#x21bb; Quét Lại</button>"
"</div>"
"<div id='scan-status' class='scan-status'>Đang tìm kiếm sóng Wi-Fi...</div>"
"<select id='ssid-select' onchange='onSsidChange(this.value)'>"
"<option value='' disabled selected>-- Bấm Quét Lại để tìm Wi-Fi --</option>"
"</select>"
"<div id='manual-box' style='display:none;'>"
"<label>Tên Mạng Thủ Công (Mạng ẩn)</label>"
"<input type='text' id='manual-ssid' placeholder='Nhập tên SSID...'>"
"</div>"
"<label>Mật Khẩu Wi-Fi</label>"
"<input type='password' id='wifi-pass' placeholder='Nhập mật khẩu Wi-Fi nhà bạn...'>"
"<div class='divider'></div>"
"<label>Mật Khẩu Bảo Vệ SoftAP Mới (Tối thiểu 8 ký tự)</label>"
"<input type='password' id='softap-pass' placeholder='Nhập tối thiểu 8 ký tự...' minlength='8' required>"
"<button type='submit' class='btn-submit' id='sub-btn'>🚀 Lưu Cấu Hình &amp; Kết Nối Ngay</button>"
"</form>"
"<div class='footer'>Smart Clock Gateway • AICLP Firmware v2.0</div></div>"
"<script>"
"var s_scanAutoTried = false;"
"function fetchScan(force) {"
"  var st = document.getElementById('scan-status');"
"  var sel = document.getElementById('ssid-select');"
"  st.innerText = 'Đang tìm kiếm sóng Wi-Fi...';"
"  var url = force ? '/scan?refresh=1' : '/scan';"
"  fetch(url).then(r => r.json()).then(list => {"
"    if (!force && list.length === 0 && !s_scanAutoTried) {"
"      s_scanAutoTried = true;"
"      fetchScan(true);"
"      return;"
"    }"
"    sel.innerHTML = '<option value=\"\" disabled selected>-- Chọn mạng Wi-Fi (' + list.length + ') --</option>';"
"    st.innerText = 'Tìm thấy ' + list.length + ' mạng Wi-Fi:';"
"    list.forEach(item => {"
"      var opt = document.createElement('option');"
"      opt.value = item.ssid;"
"      opt.text = item.ssid + ' (' + item.rssi + ' dBm)' + (item.auth > 0 ? ' [Khóa]' : '');"
"      sel.appendChild(opt);"
"    });"
"    var man = document.createElement('option');"
"    man.value = '__manual__';"
"    man.text = '+ [Nhập mạng ẩn / thủ công]';"
"    sel.appendChild(man);"
"    if (list.length === 0) { document.getElementById('manual-box').style.display = 'block'; }"
"  }).catch(e => {"
"    st.innerText = 'Không thể quét tự động, vui lòng bấm Quét Lại hoặc nhập thủ công:';"
"    document.getElementById('manual-box').style.display = 'block';"
"  });"
"}"
"function onSsidChange(val) {"
"  document.getElementById('manual-box').style.display = (val === '__manual__') ? 'block' : 'none';"
"}"
"function submitSetup(e) {"
"  e.preventDefault();"
"  var selVal = document.getElementById('ssid-select').value;"
"  var ssid = (selVal === '__manual__' || !selVal) ? document.getElementById('manual-ssid').value : selVal;"
"  var wifiPass = document.getElementById('wifi-pass').value;"
"  var apPass = document.getElementById('softap-pass').value;"
"  var err = document.getElementById('err-msg');"
"  if (apPass.length < 8) {"
"    err.innerText = 'Mật khẩu SoftAP phải từ 8 ký tự trở lên!';"
"    err.style.display = 'block'; return;"
"  }"
"  err.style.display = 'none';"
"  var btn = document.getElementById('sub-btn');"
"  btn.disabled = true; btn.innerText = 'Đang lưu cấu hình & kết nối...';"
"  fetch('/save', {"
"    method: 'POST',"
"    headers: {'Content-Type': 'application/x-www-form-urlencoded'},"
"    body: 'ssid=' + encodeURIComponent(ssid) + '&password=' + encodeURIComponent(wifiPass) + '&softap_pass=' + encodeURIComponent(apPass)"
"  }).then(r => r.text()).then(html => {"
"    document.body.innerHTML = html;"
"  }).catch(() => {"
"    alert('Đã gửi cấu hình! Thiết bị đang kết nối Wi-Fi...');"
"  });"
"}"
"fetchScan(false);"
"</script></body></html>";

static const char DASHBOARD_HTML[] =
"<!DOCTYPE html><html lang='vi'><head><meta charset='utf-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>Smart Clock & Industrial Gateway Dashboard</title>"
"<style>"
"* { box-sizing: border-box; margin: 0; padding: 0; }"
"body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; background: #0d1117; color: #c9d1d9; padding: 16px; }"
".container { max-width: 900px; margin: 0 auto; }"
"header { display: flex; justify-content: space-between; align-items: center; padding-bottom: 16px; border-bottom: 1px solid #30363d; margin-bottom: 16px; flex-wrap: wrap; gap: 10px; }"
"h1 { font-size: 20px; color: #58a6ff; display: flex; align-items: center; gap: 8px; }"
".badges { display: flex; gap: 8px; align-items: center; flex-wrap: wrap; }"
".badge { font-size: 12px; padding: 4px 10px; border-radius: 20px; font-weight: 600; background: #21262d; border: 1px solid #30363d; }"
".badge.green { color: #3fb950; border-color: rgba(63,185,80,0.3); background: rgba(63,185,80,0.1); }"
".badge.blue { color: #58a6ff; border-color: rgba(88,166,255,0.3); background: rgba(88,166,255,0.1); }"
".badge.orange { color: #d29922; border-color: rgba(210,153,34,0.3); background: rgba(210,153,34,0.1); }"
".tabs { display: flex; gap: 8px; margin-bottom: 16px; border-bottom: 1px solid #21262d; padding-bottom: 8px; }"
".tab-btn { background: transparent; border: 1px solid transparent; color: #8b949e; padding: 8px 16px; border-radius: 6px; cursor: pointer; font-weight: 600; font-size: 14px; transition: 0.2s; }"
".tab-btn:hover { color: #c9d1d9; background: #161b22; }"
".tab-btn.active { color: #58a6ff; background: #1f242c; border-color: #388bfd40; }"
".tab-content { display: none; }"
".tab-content.active { display: block; }"
".grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(260px, 1fr)); gap: 16px; margin-bottom: 16px; }"
".card { background: #161b22; border: 1px solid #30363d; border-radius: 8px; padding: 16px; box-shadow: 0 4px 12px rgba(0,0,0,0.2); }"
".card-title { font-size: 13px; color: #8b949e; text-transform: uppercase; font-weight: 700; margin-bottom: 12px; display: flex; justify-content: space-between; align-items: center; }"
".metric-val { font-size: 32px; font-weight: 700; color: #f0f6fc; }"
".metric-unit { font-size: 16px; color: #8b949e; font-weight: 400; margin-left: 4px; }"
".metric-sub { font-size: 12px; color: #8b949e; margin-top: 6px; }"
".relay-row { display: flex; justify-content: space-between; align-items: center; padding: 10px 0; border-bottom: 1px solid #21262d; }"
".relay-row:last-child { border-bottom: none; }"
".btn { padding: 6px 14px; border-radius: 6px; font-size: 13px; font-weight: 600; cursor: pointer; border: 1px solid transparent; transition: 0.2s; }"
".btn-on { background: #238636; color: #fff; border-color: #2ea043; }"
".btn-on:hover { background: #2ea043; }"
".btn-off { background: #da3633; color: #fff; border-color: #f85149; }"
".btn-off:hover { background: #f85149; }"
".btn-secondary { background: #21262d; border-color: #30363d; color: #c9d1d9; }"
".btn-secondary:hover { background: #30363d; }"
".btn-primary { background: #1f6feb; border-color: #388bfd; color: #fff; width: 100%; padding: 10px; font-size: 14px; }"
".btn-primary:hover { background: #388bfd; }"
".btn-danger { background: rgba(218,54,51,0.2); border-color: rgba(248,81,73,0.4); color: #f85149; }"
".btn-danger:hover { background: #da3633; color: #fff; }"
"table { width: 100%; border-collapse: collapse; font-size: 13px; text-align: left; }"
"th, td { padding: 10px; border-bottom: 1px solid #21262d; }"
"th { color: #8b949e; font-weight: 600; background: #0d1117; }"
"tr:hover { background: #1f242c; }"
"input, select { background: #0d1117; border: 1px solid #30363d; color: #c9d1d9; padding: 8px 10px; border-radius: 6px; width: 100%; font-size: 13px; outline: none; margin-bottom: 12px; }"
"input:focus, select:focus { border-color: #58a6ff; }"
"label { font-size: 12px; color: #8b949e; font-weight: 600; display: block; margin-bottom: 4px; }"
".row-inputs { display: grid; grid-template-columns: repeat(auto-fit, minmax(130px, 1fr)); gap: 10px; }"
"</style></head><body>"
"<div class='container'>"
"<header>"
"<div><h1>⚙️ Smart Clock Edge Gateway</h1><div style='font-size:12px;color:#8b949e;'>ESP32-S3 AICLP Architecture</div></div>"
"<div class='badges'>"
"<span id='clock-badge' class='badge blue'>--:--:-- (ICT)</span>"
"<span id='wifi-badge' class='badge orange'>Wi-Fi: ...</span>"
"<span id='mqtt-badge' class='badge orange'>MQTT: ...</span>"
"<span id='heap-badge' class='badge'>RAM: -- KB</span>"
"</div>"
"</header>"
"<div class='tabs'>"
"<button class='tab-btn active' onclick='switchTab(\"tab-wifi\", this)'>📶 Quản Lý Wi-Fi</button>"
"<button class='tab-btn' onclick='switchTab(\"tab-dash\", this)'>📊 Giám Sát &amp; Rơ-le</button>"
"<button class='tab-btn' onclick='switchTab(\"tab-rules\", this)'>⚡ Quy Tắc Tự Động</button>"
"<button class='tab-btn' onclick='switchTab(\"tab-schedules\", this)'>⏰ Lịch &amp; Báo Thức</button>"
"<button class='tab-btn' onclick='switchTab(\"tab-system\", this)'>⚙️ Hệ Thống</button>"
"</div>"
"<div id='tab-wifi' class='tab-content active'>"
"<div class='card' style='margin-bottom:16px;'>"
"<div class='card-title'><span>Mạng Đang Hoạt Động</span><span id='active-status-badge' class='badge orange'>Đang ngắt</span></div>"
"<div style='font-size:15px;line-height:1.6;'>"
"<div><strong>SSID Hiện Tại:</strong> <span id='active-ssid' style='color:#58a6ff;'>(Chưa kết nối)</span></div>"
"<div><strong>Địa chỉ IP:</strong> <span id='active-ip'>192.168.4.1</span></div>"
"</div>"
"</div>"
"<div class='card' style='margin-bottom:16px;'>"
"<div class='card-title'><span>Danh Sách Wi-Fi Đã Lưu</span><span id='saved-wifi-count' class='badge'>0/5</span></div>"
"<div style='overflow-x:auto;'>"
"<table><thead><tr><th>#</th><th>Tên Mạng (SSID)</th><th>Trạng Thái</th><th>Thao Tác</th></tr></thead>"
"<tbody id='saved-wifi-body'><tr><td colspan='4' style='text-align:center;'>Đang nạp danh sách...</td></tr></tbody></table>"
"</div>"
"</div>"
"<div class='card'>"
"<div class='card-title'><span>Thêm Mạng Wi-Fi Mới</span><button type='button' class='btn btn-secondary' style='padding:4px 10px;' onclick='fetchWifiScan(true)'>&#x21bb; Quét Lại</button></div>"
"<div id='wifi-scan-status' style='font-size:12px;color:#8b949e;margin-bottom:8px;'>Bấm Quét Lại để tìm sóng Wi-Fi...</div>"
"<form onsubmit='submitAddWifi(event)'>"
"<label>Chọn Mạng Wi-Fi Tìm Thấy (SSID)</label>"
"<select id='wifi-ssid-select' onchange='onSsidChange(this.value)'><option value='' disabled selected>-- Bấm quét lại để tìm Wi-Fi --</option></select>"
"<div id='wifi-manual-box' style='display:none;'><label>Tên Mạng Thủ Công (Mạng ẩn)</label><input type='text' id='wifi-manual-ssid' placeholder='Nhập tên SSID...'></div>"
"<label>Mật Khẩu Wi-Fi</label>"
"<input type='password' id='wifi-password' placeholder='Nhập mật khẩu Wi-Fi...'>"
"<div style='display:flex;align-items:center;gap:8px;margin-bottom:12px;'>"
"<input type='checkbox' id='wifi-connect-now' checked style='width:auto;margin:0;'>"
"<label for='wifi-connect-now' style='margin:0;cursor:pointer;font-size:13px;'>Kết nối mạng này ngay sau khi lưu</label>"
"</div>"
"<button type='submit' class='btn btn-primary'>+ Lưu Vào Danh Sách &amp; Kết Nối</button>"
"</form>"
"</div>"
"</div>"
"<div id='tab-dash' class='tab-content'>"
"<div class='grid'>"
"<div class='card'>"
"<div class='card-title'><span>🌡️ Cảm Biến BLE Nhiệt Độ</span><span id='temp-status' class='badge green'>OK</span></div>"
"<div><span id='temp-val' class='metric-val'>--</span><span class='metric-unit'>°C</span></div>"
"<div class='metric-sub'>BLE BTHome / Sensor 01</div>"
"</div>"
"<div class='card'>"
"<div class='card-title'><span>💧 Cảm Biến BLE Độ Ẩm</span><span id='hum-status' class='badge green'>OK</span></div>"
"<div><span id='hum-val' class='metric-val'>--</span><span class='metric-unit'>%RH</span></div>"
"<div class='metric-sub'>BLE BTHome / Sensor 01</div>"
"</div>"
"<div class='card'>"
"<div class='card-title'><span>⏱️ Thời Gian &amp; Hệ Thống</span></div>"
"<div style='font-size:14px;line-height:1.8;'>"
"<div><strong>Uptime:</strong> <span id='uptime-val'>--</span></div>"
"<div><strong>Địa chỉ IP:</strong> <span id='ip-val'>--</span></div>"
"<div><strong>Giám sát Task:</strong> <span id='task-health' class='badge green'>HEALTHY</span></div>"
"</div>"
"</div>"
"</div>"
"<div class='card' style='margin-bottom:16px;'>"
"<div class='card-title'><span>⚡ Điều Khiển 4-Kênh Relay (Active HIGH)</span></div>"
"<div class='relay-row'><div><strong>Relay 1: Quạt Làm Mát (fan_relay_01)</strong><div style='font-size:12px;color:#8b949e;'>GPIO 15</div></div><button id='btn-r1' class='btn btn-secondary' onclick='toggleRelay(\"fan_relay_01\", 0)'>...</button></div>"
"<div class='relay-row'><div><strong>Relay 2: Máy Sưởi (heater_relay_01)</strong><div style='font-size:12px;color:#8b949e;'>GPIO 16 (Tự ngắt sau 30m)</div></div><button id='btn-r2' class='btn btn-secondary' onclick='toggleRelay(\"heater_relay_01\", 1)'>...</button></div>"
"<div class='relay-row'><div><strong>Relay 3: Máy Hút Ẩm (dehumidifier_01)</strong><div style='font-size:12px;color:#8b949e;'>GPIO 17</div></div><button id='btn-r3' class='btn btn-secondary' onclick='toggleRelay(\"dehumidifier_01\", 2)'>...</button></div>"
"<div class='relay-row'><div><strong>Relay 4: Thiết Bị Phụ (relay_ch4)</strong><div style='font-size:12px;color:#8b949e;'>GPIO 18</div></div><button id='btn-r4' class='btn btn-secondary' onclick='toggleRelay(\"relay_ch4\", 3)'>...</button></div>"
"</div>"
"</div>"
"</div>"
"<div id='tab-rules' class='tab-content'>"
"<div class='card' style='margin-bottom:16px;'>"
"<div class='card-title'><span>Danh Sách Quy Tắc Tự Động Hóa (Rule Engine)</span><button class='btn btn-danger' style='padding:4px 10px;' onclick='clearAllRules()'>Xóa Hết</button></div>"
"<div style='overflow-x:auto;'>"
"<table><thead><tr><th>ID</th><th>Cảm Biến</th><th>Điều Kiện</th><th>Ngưỡng</th><th>Độ Trễ (Hyst)</th><th>Thiết Bị Đích</th><th>Lệnh</th><th>Trạng Thái</th><th>Thao Tác</th></tr></thead>"
"<tbody id='rules-table-body'><tr><td colspan='9' style='text-align:center;'>Đang tải quy tắc...</td></tr></tbody></table>"
"</div>"
"</div>"
"<div class='card'>"
"<div class='card-title'><span>Thêm Quy Tắc Tự Động Mới</span></div>"
"<form id='add-rule-form' onsubmit='submitNewRule(event)'>"
"<div class='row-inputs'>"
"<div><label>ID Quy Tắc</label><input type='number' id='new-rule-id' required min='1' max='100' placeholder='VD: 4'></div>"
"<div><label>Nguồn Cảm Biến</label><select id='new-rule-sensor' onchange='onRuleSensorChange(this.value)'><option value='sensor_temp_01'>sensor_temp_01 (Nhiệt độ)</option><option value='sensor_hum_01'>sensor_hum_01 (Độ ẩm)</option></select></div>"
"<div><label>Toán Tử So Sánh</label><select id='new-rule-cond'><option value='>'>Lớn hơn (&gt;)</option><option value='<'>Nhỏ hơn (&lt;)</option></select></div>"
"<div><label>Ngưỡng Kích Hoạt</label><input type='number' step='0.1' id='new-rule-thresh' required placeholder='VD: 30.0'></div>"
"<div><label>Độ Trễ Bảo Vệ (Hysteresis)</label><input type='number' step='0.1' min='0.1' id='new-rule-hyst' value='1.5' required placeholder='VD: 1.5'></div>"
"</div>"
"<div class='row-inputs'>"
"<div><label>Thiết Bị Chấp Hành</label><select id='new-rule-target'><option value='fan_relay_01'>fan_relay_01 (Quạt)</option><option value='heater_relay_01'>heater_relay_01 (Sưởi)</option><option value='dehumidifier_01'>dehumidifier_01 (Hút ẩm)</option><option value='relay_ch4'>relay_ch4 (Relay 4)</option></select></div>"
"<div><label>Lệnh Điều Khiển</label><select id='new-rule-cmd'><option value='ON'>Bật (ON)</option><option value='OFF'>Tắt (OFF)</option><option value='START'>Khởi động (START)</option><option value='TOGGLE'>Đảo trạng thái (TOGGLE)</option></select></div>"
"</div>"
"<button type='submit' class='btn btn-primary' style='margin-top:8px;'>+ Thêm Quy Tắc Vào Hệ Thống</button>"
"</form>"
"</div>"
"</div>"
"<div id='tab-schedules' class='tab-content'>"
"<div class='card' style='margin-bottom:16px;'>"
"<div class='card-title'><span>🔔 Điều Khiển Báo Thức Trực Tiếp</span><span id='alarm-state-badge' class='badge'>IDLE</span></div>"
"<div style='display:flex;gap:10px;flex-wrap:wrap;'>"
"<button type='button' class='btn btn-off' style='flex:1;min-width:140px;' onclick='controlAlarm(\"dismiss\")'>🔕 Tắt Chuông (Dismiss)</button>"
"<button type='button' class='btn btn-secondary' style='flex:1;min-width:140px;' onclick='controlAlarm(\"snooze\")'>💤 Báo Lại 9 Phút (Snooze)</button>"
"<button type='button' class='btn btn-on' style='flex:1;min-width:140px;' onclick='controlAlarm(\"trigger\")'>🔔 Thử Chuông Báo Thức</button>"
"</div>"
"</div>"
"<div class='card' style='margin-bottom:16px;'>"
"<div class='card-title'><span>Danh Sách Lịch Hẹn Giờ &amp; Báo Thức (Schedules &amp; Alarms)</span><button class='btn btn-danger' style='padding:4px 10px;' onclick='clearAllSchedules()'>Xóa Hết</button></div>"
"<div style='overflow-x:auto;'>"
"<table><thead><tr><th>ID</th><th>Giờ</th><th>Loại</th><th>Nội Dung / Thiết Bị</th><th>Lặp Lại</th><th>Trạng Thái</th><th>Thao Tác</th></tr></thead>"
"<tbody id='schedules-table-body'><tr><td colspan='7' style='text-align:center;'>Đang tải lịch biểu...</td></tr></tbody></table>"
"</div>"
"</div>"
"<div class='card'>"
"<div class='card-title'><span>Thêm Lịch Hẹn Giờ / Báo Thức Mới</span></div>"
"<form id='add-schedule-form' onsubmit='submitNewSchedule(event)'>"
"<div class='row-inputs'>"
"<div><label>ID Lịch</label><input type='number' id='new-sched-id' required min='1' max='64' placeholder='VD: 1'></div>"
"<div><label>Loại Lịch</label><select id='new-sched-type' onchange='onSchedTypeChange(this.value)'><option value='alarm'>Báo thức (alarm)</option><option value='automation'>Điều khiển thiết bị (automation)</option></select></div>"
"<div><label>Giờ (0-23)</label><input type='number' id='new-sched-hour' required min='0' max='23' value='6'></div>"
"<div><label>Phút (0-59)</label><input type='number' id='new-sched-min' required min='0' max='59' value='30'></div>"
"</div>"
"<div class='row-inputs'>"
"<div><label>Tiêu Đề Báo Thức</label><input type='text' id='new-sched-title' placeholder='VD: Thức dậy'></div>"
"<div id='sched-target-box' style='display:none;'><label>Thiết Bị Đích</label><select id='new-sched-target'><option value='relay_1'>relay_1 (Quạt)</option><option value='relay_2'>relay_2 (Sưởi)</option><option value='relay_3'>relay_3 (Hút ẩm)</option><option value='relay_4'>relay_4 (Đèn/Phụ trợ)</option></select></div>"
"<div id='sched-cmd-box' style='display:none;'><label>Lệnh</label><select id='new-sched-cmd'><option value='ON'>ON</option><option value='OFF'>OFF</option></select></div>"
"<div><label>Các Ngày Lặp Lại</label><input type='text' id='new-sched-days' value='SUN,MON,TUE,WED,THU,FRI,SAT'></div>"
"</div>"
"<button type='submit' class='btn btn-primary' style='margin-top:8px;'>+ Lưu Lịch Biểu</button>"
"</form>"
"</div>"
"</div>"
"<div id='tab-system' class='tab-content'>"
"<div class='card' style='margin-bottom:16px;'>"
"<div class='card-title'><span>Đổi Mật Khẩu SoftAP</span></div>"
"<form onsubmit='submitChangeSoftapPass(event)'>"
"<label>Mật Khẩu SoftAP Mới (Tối thiểu 8 ký tự)</label>"
"<input type='password' id='sys-new-softap-pass' placeholder='Nhập mật khẩu mới...' minlength='8' required>"
"<button type='submit' class='btn btn-primary'>Cập Nhật Mật Khẩu SoftAP</button>"
"</form>"
"</div>"
"<div class='card'>"
"<div class='card-title'><span>Bảo Trì Thiết Bị</span></div>"
"<div style='display:flex;gap:12px;flex-wrap:wrap;'>"
"<button type='button' class='btn btn-secondary' style='flex:1;min-width:140px;padding:12px;' onclick='rebootGateway()'>🔄 Khởi Động Lại Gateway</button>"
"<button type='button' class='btn btn-danger' style='flex:1;min-width:140px;padding:12px;' onclick='factoryResetGateway()'>⚠️ Khôi Phục Cài Đặt Gốc</button>"
"</div>"
"</div>"
"</div>"
"</div>"
"<script>"
"function api(u,o){o=o||{};o.headers=Object.assign({},o.headers,{'X-API-Key':localStorage.k||''});return fetch(u,o).then(r=>{if(r.status==401){var k=prompt('API key (serial: config show)');if(k){localStorage.k=k;return api(u,o);}}return r;});}"
"var s_relays = [0, 0, 0, 0];"
"function switchTab(id, btn) {"
"  document.querySelectorAll('.tab-content').forEach(el => el.classList.remove('active'));"
"  document.querySelectorAll('.tab-btn').forEach(el => el.classList.remove('active'));"
"  document.getElementById(id).classList.add('active');"
"  btn.classList.add('active');"
"  if (id === 'tab-rules') fetchRules();"
"  if (id === 'tab-schedules') fetchSchedules();"
"  if (id === 'tab-wifi') loadSavedWifi();"
"}"
"function onRuleSensorChange(val) {"
"  var h = document.getElementById('new-rule-hyst');"
"  if (h) h.value = (val.indexOf('hum') !== -1) ? '2.0' : '1.5';"
"}"
"function onSchedTypeChange(val) {"
"  var isAuto = (val === 'automation');"
"  document.getElementById('sched-target-box').style.display = isAuto ? 'block' : 'none';"
"  document.getElementById('sched-cmd-box').style.display = isAuto ? 'block' : 'none';"
"}"
"function fetchStatus() {"
"  fetch('/api/status').then(r => r.json()).then(d => {"
"    if (d.time) document.getElementById('clock-badge').innerText = d.time;"
"    if (d.uptime_s !== undefined) {"
"      var m = Math.floor(d.uptime_s / 60); var s = d.uptime_s % 60;"
"      document.getElementById('uptime-val').innerText = m + ' phút ' + s + ' giây';"
"    }"
"    if (d.ip) document.getElementById('ip-val').innerText = d.ip;"
"    if (d.free_heap) document.getElementById('heap-badge').innerText = 'RAM: ' + Math.round(d.free_heap / 1024) + ' KB';"
"    var wBadge = document.getElementById('wifi-badge');"
"    if (d.wifi_connected) { wBadge.innerText = 'Wi-Fi: Đã nối (' + (d.ip||'') + ')'; wBadge.className = 'badge green'; }"
"    else { wBadge.innerText = 'Wi-Fi: SoftAP'; wBadge.className = 'badge orange'; }"
"    var mBadge = document.getElementById('mqtt-badge');"
"    if (d.mqtt_connected) { mBadge.innerText = 'MQTT: Online'; mBadge.className = 'badge green'; }"
"    else { mBadge.innerText = 'MQTT: Offline'; mBadge.className = 'badge orange'; }"
"    var aBadge = document.getElementById('alarm-state-badge');"
"    if (aBadge) {"
"      if (d.alarm_active) { aBadge.innerText = 'ĐANG KÊU!'; aBadge.className = 'badge orange'; }"
"      else { aBadge.innerText = 'IDLE'; aBadge.className = 'badge green'; }"
"    }"
"  }).catch(e => console.error(e));"
"}"
"function fetchTelemetry() {"
"  fetch('/api/telemetry').then(r => r.json()).then(d => {"
"    if (d.temp !== undefined) document.getElementById('temp-val').innerText = d.temp.toFixed(1);"
"    if (d.hum !== undefined) document.getElementById('hum-val').innerText = d.hum.toFixed(1);"
"    var tSt = document.getElementById('temp-status');"
"    var hSt = document.getElementById('hum-status');"
"    if (d.valid) { tSt.innerText = 'OK'; tSt.className = 'badge green'; hSt.innerText = 'OK'; hSt.className = 'badge green'; }"
"    else { tSt.innerText = 'LỖI'; tSt.className = 'badge orange'; hSt.innerText = 'LỖI'; hSt.className = 'badge orange'; }"
"    if (d.relays) {"
"      s_relays = d.relays;"
"      updateRelayBtn('btn-r1', d.relays[0]);"
"      updateRelayBtn('btn-r2', d.relays[1]);"
"      updateRelayBtn('btn-r3', d.relays[2]);"
"      updateRelayBtn('btn-r4', d.relays[3]);"
"    }"
"  }).catch(e => console.error(e));"
"}"
"function updateRelayBtn(id, val) {"
"  var b = document.getElementById(id);"
"  if (!b) return;"
"  if (val) { b.innerText = 'ĐANG BẬT'; b.className = 'btn btn-on'; }"
"  else { b.innerText = 'ĐANG TẮT'; b.className = 'btn btn-off'; }"
"}"
"function toggleRelay(target, idx) {"
"  var nextCmd = s_relays[idx] ? 'OFF' : 'ON';"
"  api('/api/relay', {"
"    method: 'POST',"
"    headers: {'Content-Type': 'application/json'},"
"    body: JSON.stringify({ target: target, command: nextCmd })"
"  }).then(r => r.json()).then(() => {"
"    s_relays[idx] = s_relays[idx] ? 0 : 1;"
"    updateRelayBtn('btn-r' + (idx+1), s_relays[idx]);"
"  }).catch(e => alert('Lỗi gửi lệnh relay: ' + e));"
"}"
"function fetchRules() {"
"  fetch('/api/rules').then(r => r.json()).then(rules => {"
"    var tbody = document.getElementById('rules-table-body');"
"    tbody.innerHTML = '';"
"    if (!rules || rules.length === 0) {"
"      tbody.innerHTML = '<tr><td colspan=\"9\" style=\"text-align:center;color:#8b949e;\">Chưa có quy tắc nào</td></tr>';"
"      return;"
"    }"
"    rules.forEach(r => {"
"      var tr = document.createElement('tr');"
"      var hystVal = (r.hysteresis !== undefined) ? Number(r.hysteresis).toFixed(1) : '1.5';"
"      var relVal = (r.condition === '<') ? (Number(r.threshold) + Number(hystVal)).toFixed(1) : (Number(r.threshold) - Number(hystVal)).toFixed(1);"
"      var relOp = (r.condition === '<') ? '>=' : '<=';"
"      tr.innerHTML = '<td><strong>#' + r.id + '</strong></td>' +"
"        '<td>' + r.device_id + '</td>' +"
"        '<td>' + r.condition + '</td>' +"
"        '<td>' + r.threshold + '</td>' +"
"        '<td><span class=\"badge blue\">' + hystVal + ' (Tắt ' + relOp + ' ' + relVal + ')</span></td>' +"
"        '<td>' + r.target + '</td>' +"
"        '<td><code>' + r.action + '</code></td>' +"
"        '<td>' + (r.enabled ? '<span class=\"badge green\">Bật</span>' : '<span class=\"badge\">Tắt</span>') + '</td>' +"
"        '<td><button class=\"btn btn-danger\" style=\"padding:2px 8px;font-size:11px;\" onclick=\"deleteRule(' + r.id + ')\">Xóa</button></td>';"
"      tbody.appendChild(tr);"
"    });"
"  }).catch(e => console.error(e));"
"}"
"function submitNewRule(e) {"
"  e.preventDefault();"
"  var payload = {"
"    id: parseInt(document.getElementById('new-rule-id').value),"
"    device_id: document.getElementById('new-rule-sensor').value,"
"    condition: document.getElementById('new-rule-cond').value,"
"    threshold: parseFloat(document.getElementById('new-rule-thresh').value),"
"    hysteresis: parseFloat(document.getElementById('new-rule-hyst').value) || 1.5,"
"    cooldown_s: 60,"
"    target: document.getElementById('new-rule-target').value,"
"    action: document.getElementById('new-rule-cmd').value,"
"    enabled: true"
"  };"
"  api('/api/rules', {"
"    method: 'POST',"
"    headers: {'Content-Type': 'application/json'},"
"    body: JSON.stringify(payload)"
"  }).then(r => r.json()).then(d => {"
"    alert('Đã lưu quy tắc thành công!');"
"    fetchRules();"
"    document.getElementById('add-rule-form').reset();"
"    document.getElementById('new-rule-hyst').value = '1.5';"
"  }).catch(e => alert('Lỗi: ' + e));"
"}"
"function fetchSchedules() {"
"  fetch('/api/schedules').then(r => r.json()).then(list => {"
"    var tbody = document.getElementById('schedules-table-body');"
"    if (!tbody) return;"
"    tbody.innerHTML = '';"
"    if (!list || list.length === 0) {"
"      tbody.innerHTML = '<tr><td colspan=\"7\" style=\"text-align:center;color:#8b949e;\">Chưa có lịch hẹn giờ / báo thức nào</td></tr>';"
"      return;"
"    }"
"    list.forEach(s => {"
"      var tr = document.createElement('tr');"
"      var hh = String(s.hour || 0).padStart(2, '0');"
"      var mm = String(s.minute || 0).padStart(2, '0');"
"      var desc = (s.type === 'automation') ? ((s.target_device_id || '') + ' -> <code>' + (s.action_command || '') + '</code>') : (s.title || 'Báo thức');"
"      tr.innerHTML = '<td><strong>#' + s.id + '</strong></td>' +"
"        '<td><strong>' + hh + ':' + mm + '</strong></td>' +"
"        '<td><span class=\"badge blue\">' + (s.type || 'alarm') + '</span></td>' +"
"        '<td>' + desc + '</td>' +"
"        '<td>' + (s.repeat_days || 'Hàng ngày') + '</td>' +"
"        '<td>' + (s.enabled ? '<span class=\"badge green\">Bật</span>' : '<span class=\"badge\">Tắt</span>') + '</td>' +"
"        '<td><button class=\"btn btn-danger\" style=\"padding:2px 8px;font-size:11px;\" onclick=\"deleteSchedule(' + s.id + ')\">Xóa</button></td>';"
"      tbody.appendChild(tr);"
"    });"
"  }).catch(e => console.error(e));"
"}"
"function submitNewSchedule(e) {"
"  e.preventDefault();"
"  var sType = document.getElementById('new-sched-type').value;"
"  var payload = {"
"    id: parseInt(document.getElementById('new-sched-id').value),"
"    type: sType,"
"    hour: parseInt(document.getElementById('new-sched-hour').value),"
"    minute: parseInt(document.getElementById('new-sched-min').value),"
"    title: document.getElementById('new-sched-title').value || (sType === 'alarm' ? 'Báo thức' : ''),"
"    target_device_id: (sType === 'automation') ? document.getElementById('new-sched-target').value : '',"
"    action_command: (sType === 'automation') ? document.getElementById('new-sched-cmd').value : '',"
"    repeat_days: document.getElementById('new-sched-days').value || 'SUN,MON,TUE,WED,THU,FRI,SAT',"
"    enabled: true,"
"    buzzer: (sType !== 'automation')"
"  };"
"  api('/api/schedules', {"
"    method: 'POST',"
"    headers: {'Content-Type': 'application/json'},"
"    body: JSON.stringify(payload)"
"  }).then(r => r.json()).then(() => {"
"    alert('Đã lưu lịch biểu thành công!');"
"    fetchSchedules();"
"  }).catch(e => alert('Lỗi lưu lịch: ' + e));"
"}"
"function deleteSchedule(id) {"
"  if (!confirm('Xóa lịch #' + id + '?')) return;"
"  api('/api/schedules?id=' + id, { method: 'DELETE' }).then(r => r.json()).then(() => {"
"    fetchSchedules();"
"  }).catch(e => alert('Lỗi xóa lịch: ' + e));"
"}"
"function clearAllSchedules() {"
"  if (!confirm('Xóa TẤT CẢ lịch biểu?')) return;"
"  api('/api/schedules', { method: 'DELETE' }).then(r => r.json()).then(() => {"
"    fetchSchedules();"
"  }).catch(e => alert('Lỗi: ' + e));"
"}"
"function controlAlarm(action) {"
"  var body = (action === 'snooze') ? { action: 'snooze', minutes: 9 } : (action === 'trigger' ? { action: 'trigger', title: 'Local Web Alarm', buzzer: true } : { action: 'dismiss' });"
"  api('/api/alarm', {"
"    method: 'POST',"
"    headers: {'Content-Type': 'application/json'},"
"    body: JSON.stringify(body)"
"  }).then(r => r.json()).then(() => {"
"    fetchStatus();"
"  }).catch(e => alert('Lỗi điều khiển chuông: ' + e));"
"}"
"function deleteRule(id) {"
"  if (!confirm('Bạn có chắc muốn xóa Quy tắc #' + id + '?')) return;"
"  api('/api/rules?id=' + id, { method: 'DELETE' }).then(r => r.json()).then(() => {"
"    fetchRules();"
"  }).catch(e => alert('Lỗi xóa: ' + e));"
"}"
"function clearAllRules() {"
"  if (!confirm('Xóa TẤT CẢ quy tắc tự động?')) return;"
"  api('/api/rules', { method: 'DELETE' }).then(r => r.json()).then(() => {"
"    fetchRules();"
"  }).catch(e => alert('Lỗi: ' + e));"
"}"
"function loadSavedWifi() {"
"  api('/api/wifi/saved').then(r => r.json()).then(data => {"
"    var tbody = document.getElementById('saved-wifi-body');"
"    tbody.innerHTML = '';"
"    var actSSID = data.active_ssid || '';"
"    document.getElementById('active-ssid').innerText = actSSID || '(Chưa kết nối)';"
"    document.getElementById('active-ip').innerText = data.ip || '192.168.4.1';"
"    var actBadge = document.getElementById('active-status-badge');"
"    if (data.connected) { actBadge.innerText = 'Đã kết nối'; actBadge.className = 'badge green'; }"
"    else { actBadge.innerText = 'Đang ngắt'; actBadge.className = 'badge orange'; }"
"    var list = data.saved || [];"
"    document.getElementById('saved-wifi-count').innerText = list.length + '/5';"
"    if (list.length === 0) {"
"      tbody.innerHTML = '<tr><td colspan=\"4\" style=\"text-align:center;color:#8b949e;\">Chưa có mạng Wi-Fi nào được lưu.</td></tr>';"
"      return;"
"    }"
"    list.forEach((item, idx) => {"
"      var tr = document.createElement('tr');"
"      var isAct = item.active;"
"      var safeSsid = String(item.ssid).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/\"/g,'&quot;');"
"      var jsSsid = String(item.ssid).replace(/\\\\/g,'\\\\\\\\').replace(/'/g,\"\\\\'\").replace(/\"/g,'&quot;');"
"      tr.innerHTML = '<td>' + (idx + 1) + '</td>' +"
"        '<td><strong>' + safeSsid + '</strong></td>' +"
"        '<td>' + (isAct ? '<span class=\"badge green\">Đang dùng</span>' : '<span class=\"badge\">Đã lưu</span>') + '</td>' +"
"        '<td style=\"display:flex;gap:6px;\">' +"
"          '<button class=\"btn btn-primary\" style=\"padding:4px 8px;font-size:11px;width:auto;\" onclick=\"connectWifi(\\'' + jsSsid + '\\')\">⚡ Nối</button>' +"
"          '<button class=\"btn btn-secondary\" style=\"padding:4px 8px;font-size:11px;\" onclick=\"editWifiPass(\\'' + jsSsid + '\\')\">✏️ Đổi Pass</button>' +"
"          '<button class=\"btn btn-danger\" style=\"padding:4px 8px;font-size:11px;\" onclick=\"deleteSavedWifi(\\'' + jsSsid + '\\')\">🗑️ Xóa</button>' +"
"        '</td>';"
"      tbody.appendChild(tr);"
"    });"
"  }).catch(e => console.error(e));"
"}"
"function connectWifi(ssid) {"
"  if (!confirm('Chuyển sang kết nối mạng \"' + ssid + '\"?')) return;"
"  api('/api/wifi/connect', {"
"    method: 'POST',"
"    headers: {'Content-Type': 'application/json'},"
"    body: JSON.stringify({ssid: ssid})"
"  }).then(r => r.json()).then(d => {"
"    alert('Đang kết nối tới \"' + ssid + '\"...');"
"    setTimeout(loadSavedWifi, 2000);"
"  }).catch(e => alert('Lỗi: ' + e));"
"}"
"function editWifiPass(ssid) {"
"  var newPass = prompt('Nhập mật khẩu mới cho mạng \"' + ssid + '\":');"
"  if (newPass === null) return;"
"  api('/api/wifi/save', {"
"    method: 'POST',"
"    headers: {'Content-Type': 'application/json'},"
"    body: JSON.stringify({ssid: ssid, password: newPass, connect: false})"
"  }).then(r => r.json()).then(d => {"
"    alert('Đã cập nhật mật khẩu cho \"' + ssid + '\"!');"
"    loadSavedWifi();"
"  }).catch(e => alert('Lỗi: ' + e));"
"}"
"function deleteSavedWifi(ssid) {"
"  if (!confirm('Bạn có chắc muốn xóa mạng \"' + ssid + '\" khỏi danh sách đã lưu?')) return;"
"  api('/api/wifi/delete', {"
"    method: 'POST',"
"    headers: {'Content-Type': 'application/json'},"
"    body: JSON.stringify({ssid: ssid})"
"  }).then(r => r.json()).then(d => {"
"    alert('Đã xóa mạng \"' + ssid + '\"!');"
"    loadSavedWifi();"
"  }).catch(e => alert('Lỗi: ' + e));"
"}"
"function submitAddWifi(e) {"
"  e.preventDefault();"
"  var selVal = document.getElementById('wifi-ssid-select').value;"
"  var ssid = (selVal === '__manual__') ? document.getElementById('wifi-manual-ssid').value : selVal;"
"  var pass = document.getElementById('wifi-password').value;"
"  var conn = document.getElementById('wifi-connect-now').checked;"
"  if (!ssid) { alert('Vui lòng chọn hoặc nhập tên SSID!'); return; }"
"  api('/api/wifi/save', {"
"    method: 'POST',"
"    headers: {'Content-Type': 'application/json'},"
"    body: JSON.stringify({ssid: ssid, password: pass, connect: conn})"
"  }).then(r => r.json()).then(d => {"
"    alert('Đã lưu Wi-Fi \"' + ssid + '\"!');"
"    loadSavedWifi();"
"    document.getElementById('wifi-password').value = '';"
"  }).catch(e => alert('Lỗi: ' + e));"
"}"
"function submitChangeSoftapPass(e) {"
"  e.preventDefault();"
"  var p = document.getElementById('sys-new-softap-pass').value;"
"  if (p.length < 8) { alert('Mật khẩu SoftAP phải từ 8 ký tự trở lên!'); return; }"
"  api('/api/softap/set_password', {"
"    method: 'POST',"
"    headers: {'Content-Type': 'application/x-www-form-urlencoded'},"
"    body: 'password=' + encodeURIComponent(p)"
"  }).then(r => r.json()).then(d => {"
"    alert('Đã đổi mật khẩu SoftAP thành công!');"
"    document.getElementById('sys-new-softap-pass').value = '';"
"  }).catch(e => alert('Lỗi: ' + e));"
"}"
"function rebootGateway() {"
"  if (!confirm('Khởi động lại Gateway?')) return;"
"  api('/api/system/reboot', { method: 'POST' }).then(() => {"
"    alert('Gateway đang khởi động lại...');"
"  }).catch(() => alert('Gateway đang khởi động lại...'));"
"}"
"function factoryResetGateway() {"
"  if (!confirm('CẢNH BÁO: Xóa sạch toàn bộ cấu hình Wi-Fi và đưa SoftAP về chế độ OPEN?')) return;"
"  api('/api/system/factory_reset', { method: 'POST' }).then(() => {"
"    alert('Đã xóa cấu hình! Thiết bị đang khởi động lại ở chế độ OPEN...');"
"  }).catch(() => alert('Thiết bị đang khởi động lại...'));"
"}"
"var s_wifiScanAutoTried = false;"
"function fetchWifiScan(force) {"
"  var st = document.getElementById('wifi-scan-status');"
"  var sel = document.getElementById('wifi-ssid-select');"
"  st.innerText = 'Đang tìm kiếm sóng Wi-Fi...';"
"  var url = force ? '/scan?refresh=1' : '/scan';"
"  fetch(url).then(r => r.json()).then(list => {"
"    if (!force && list.length === 0 && !s_wifiScanAutoTried) {"
"      s_wifiScanAutoTried = true;"
"      fetchWifiScan(true);"
"      return;"
"    }"
"    sel.innerHTML = '<option value=\"\" disabled selected>-- Chọn mạng Wi-Fi (' + list.length + ') --</option>';"
"    st.innerText = 'Tìm thấy ' + list.length + ' mạng Wi-Fi:';"
"    list.forEach(item => {"
"      var opt = document.createElement('option');"
"      opt.value = item.ssid;"
"      opt.text = item.ssid + ' (' + item.rssi + ' dBm)' + (item.auth>0?' [Khóa]':'');"
"      sel.appendChild(opt);"
"    });"
"    var man = document.createElement('option'); man.value = '__manual__'; man.text = '+ [Nhập mạng ẩn / thủ công]'; sel.appendChild(man);"
"    if (list.length === 0) { document.getElementById('wifi-manual-box').style.display = 'block'; }"
"  }).catch(e => {"
"    st.innerText = 'Không thể quét tự động, vui lòng nhập thủ công:';"
"    document.getElementById('wifi-manual-box').style.display = 'block';"
"  });"
"}"
"function onSsidChange(val) {"
"  document.getElementById('wifi-manual-box').style.display = (val === '__manual__') ? 'block' : 'none';"
"}"
"loadSavedWifi(); fetchStatus(); fetchTelemetry(); fetchRules(); fetchSchedules(); fetchWifiScan(false);"
"setInterval(fetchStatus, 5000);"
"setInterval(fetchTelemetry, 4000);"
"</script></body></html>";

// ==========================================
// HTTP HANDLERS
// ==========================================

static inline void set_common_headers(httpd_req_t *req, const char *content_type)
{
    if (content_type) {
        httpd_resp_set_type(req, content_type);
    }
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Connection", "close");
}

static esp_err_t get_dashboard_handler(httpd_req_t *req)
{
    set_common_headers(req, "text/html; charset=utf-8");
    const gateway_config_t *cfg = config_manager_get();
    if (!cfg || cfg->softap_password[0] == '\0') {
        return httpd_resp_send(req, SET_PASSWORD_HTML, HTTPD_RESP_USE_STRLEN);
    }
    return httpd_resp_send(req, DASHBOARD_HTML, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t get_status_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    uint32_t active = 0, alive = 0, free_heap = 0;
    supervisor_get_status(&active, &alive, &free_heap);

    uint64_t uptime_s = esp_timer_get_time() / 1000000ULL;
    char time_str[32] = {0};
    app_rtc_get_formatted_time(time_str, sizeof(time_str));

    const gateway_config_t *cfg = config_manager_get();
    uint32_t sq_total = 0, sq_unacked = 0;
    sd_logger_get_sync_queue_stats(&sq_total, &sq_unacked);

    cJSON_AddStringToObject(root, "gateway_id",
                            (cfg && cfg->gateway_id[0] != '\0') ? cfg->gateway_id : "gw_smartclock_01");
    cJSON_AddNumberToObject(root, "uptime_s", (double)uptime_s);
    cJSON_AddNumberToObject(root, "free_heap", free_heap);
    cJSON_AddStringToObject(root, "ip", wifi_provisioning_get_ip());
    cJSON_AddBoolToObject(root, "wifi_connected", wifi_provisioning_is_connected());
    cJSON_AddBoolToObject(root, "mqtt_connected", mqtt_adapter_is_connected());
    cJSON_AddNumberToObject(root, "active_mask", active);
    cJSON_AddNumberToObject(root, "alive_mask", alive);
    cJSON_AddStringToObject(root, "time", time_str);
    cJSON_AddBoolToObject(root, "rtc_valid", rtc_is_valid());
    cJSON_AddBoolToObject(root, "sd_mounted", sd_logger_is_mounted());
    cJSON_AddNumberToObject(root, "sync_queue_total", (double)sq_total);
    cJSON_AddNumberToObject(root, "sync_queue_unacked", (double)sq_unacked);
    cJSON_AddNumberToObject(root, "ram_fallback_count", sd_logger_get_ram_fallback_count());
    cJSON_AddBoolToObject(root, "alarm_active", display_is_alarm_active());

    char *resp_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!resp_str) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    set_common_headers(req, "application/json");
    esp_err_t ret = httpd_resp_send(req, resp_str, strlen(resp_str));
    free(resp_str);
    return ret;
}

static void add_rounded_number(cJSON *root, const char *key, float val)
{
    if (!isfinite(val)) {
        cJSON_AddNumberToObject(root, key, 0.0);
        return;
    }
    double d = (val > -0.005f && val < 0.005f) ? 0.0 : (double)val;
    char buf[24];
    int len = snprintf(buf, sizeof(buf), "%.2f", d);
    if (len > 0 && (size_t)len < sizeof(buf)) {
        if (buf[len - 1] == '0' && len >= 2 && buf[len - 2] != '.') {
            buf[len - 1] = '\0';
        }
        cJSON_AddRawToObject(root, key, buf);
    } else {
        cJSON_AddNumberToObject(root, key, d);
    }
}

static esp_err_t get_telemetry_handler(httpd_req_t *req)
{
    float temp = display_get_latest_temp();
    float hum = display_get_latest_hum();
    bool valid = display_get_latest_valid();

    uint8_t coils[4] = {0};
    for (int i = 0; i < 4; i++) {
        coils[i] = relay_controller_get_state(i + 1) ? 1 : 0;
    }

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    add_rounded_number(root, "temp", temp);
    add_rounded_number(root, "hum", hum);
    cJSON_AddBoolToObject(root, "valid", valid);

    cJSON *relays = cJSON_CreateArray();
    for (int i = 0; i < 4; i++) {
        cJSON_AddItemToArray(relays, cJSON_CreateNumber(coils[i]));
    }
    cJSON_AddItemToObject(root, "relays", relays);

    char *resp_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    set_common_headers(req, "application/json");
    esp_err_t ret = httpd_resp_send(req, resp_str, strlen(resp_str));
    free(resp_str);
    return ret;
}

static esp_err_t post_relay_handler(httpd_req_t *req)
{
    if (!check_api_auth(req)) return ESP_OK;
    char buf[128];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    cJSON *json = cJSON_Parse(buf);
    if (!json) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    cJSON *target = cJSON_GetObjectItem(json, "target");
    cJSON *command = cJSON_GetObjectItem(json, "command");

    if (target && target->valuestring && command && command->valuestring) {
        rule_action_request_t *req_act = (rule_action_request_t *)malloc(sizeof(rule_action_request_t));
        if (req_act != NULL) {
            memset(req_act, 0, sizeof(rule_action_request_t));
            strncpy(req_act->command_id, "cmd-webui", sizeof(req_act->command_id) - 1);
            strncpy(req_act->target_device_id, target->valuestring, sizeof(req_act->target_device_id) - 1);
            strncpy(req_act->action_command, command->valuestring, sizeof(req_act->action_command) - 1);
            strncpy(req_act->reason, "MANUAL_CMD", sizeof(req_act->reason) - 1);
            req_act->source_rule_id = 0; // Web UI override

            ESP_LOGI(TAG, "Web UI dispatched action to '%s': '%s'", req_act->target_device_id, req_act->action_command);
            event_bus_post(EVT_RULE_ACTION_REQUEST, req_act, sizeof(rule_action_request_t), false);
        }
    }

    cJSON_Delete(json);

    set_common_headers(req, "application/json");
    return httpd_resp_send(req, "{\"status\":\"ok\"}", HTTPD_RESP_USE_STRLEN);
}

static esp_err_t get_rules_handler(httpd_req_t *req)
{
    automation_rule_t *rules = (automation_rule_t *)calloc(MAX_RULES, sizeof(automation_rule_t));
    if (!rules) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    int count = rule_engine_get_rules(rules, MAX_RULES);

    cJSON *arr = cJSON_CreateArray();
    for (int i = 0; i < count; i++) {
        cJSON *r = cJSON_CreateObject();
        cJSON_AddNumberToObject(r, "id", rules[i].id);
        cJSON_AddStringToObject(r, "device_id", rules[i].device_id);
        cJSON_AddStringToObject(r, "condition", (rules[i].condition_type == RULE_CONDITION_SENSOR_GT) ? ">" : "<");
        cJSON_AddNumberToObject(r, "threshold", rules[i].threshold);
        cJSON_AddNumberToObject(r, "hysteresis", rules[i].hysteresis);
        cJSON_AddNumberToObject(r, "cooldown_s", rules[i].cooldown_s);
        cJSON_AddStringToObject(r, "target", rules[i].target_device_id);
        cJSON_AddStringToObject(r, "action", rules[i].action_command);
        cJSON_AddBoolToObject(r, "enabled", rules[i].enabled);
        cJSON_AddItemToArray(arr, r);
    }
    free(rules);

    char *resp_str = cJSON_PrintUnformatted(arr);
    cJSON_Delete(arr);

    set_common_headers(req, "application/json");
    esp_err_t ret = httpd_resp_send(req, resp_str, strlen(resp_str));
    free(resp_str);
    return ret;
}

static esp_err_t post_rules_handler(httpd_req_t *req)
{
    if (!check_api_auth(req)) return ESP_OK;
    char buf[384];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    cJSON *json = cJSON_Parse(buf);
    if (!json) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    automation_rule_t rule;
    memset(&rule, 0, sizeof(rule));

    cJSON *id = cJSON_GetObjectItem(json, "id");
    cJSON *dev = cJSON_GetObjectItem(json, "device_id");
    cJSON *cond = cJSON_GetObjectItem(json, "condition");
    cJSON *thresh = cJSON_GetObjectItem(json, "threshold");
    cJSON *hyst = cJSON_GetObjectItem(json, "hysteresis");
    cJSON *cool = cJSON_GetObjectItem(json, "cooldown_s");
    cJSON *target = cJSON_GetObjectItem(json, "target");
    cJSON *act = cJSON_GetObjectItem(json, "action");
    cJSON *en = cJSON_GetObjectItem(json, "enabled");

    if (!id || !dev || !cond || !thresh || !target || !act) {
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing required rule fields");
        return ESP_FAIL;
    }

    rule.id = id->valueint;
    strncpy(rule.device_id, dev->valuestring, sizeof(rule.device_id) - 1);
    rule.condition_type = (strcmp(cond->valuestring, "<") == 0) ? RULE_CONDITION_SENSOR_LT : RULE_CONDITION_SENSOR_GT;
    rule.threshold = (float)thresh->valuedouble;
    float default_hyst = (strstr(rule.device_id, "hum") != NULL) ? 2.0f : 1.5f;
    rule.hysteresis = (hyst && cJSON_IsNumber(hyst) && hyst->valuedouble > 0) ? (float)hyst->valuedouble : default_hyst;
    rule.cooldown_s = (cool && cJSON_IsNumber(cool) && cool->valueint > 0) ? (uint32_t)cool->valueint : 60;
    strncpy(rule.target_device_id, target->valuestring, sizeof(rule.target_device_id) - 1);
    strncpy(rule.action_command, act->valuestring, sizeof(rule.action_command) - 1);
    rule.enabled = (en != NULL) ? cJSON_IsTrue(en) : true;

    cJSON_Delete(json);

    bool ok = rule_engine_add_rule(&rule);
    set_common_headers(req, "application/json");
    if (ok) {
        return httpd_resp_send(req, "{\"status\":\"ok\",\"message\":\"Rule saved\"}", HTTPD_RESP_USE_STRLEN);
    } else {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to save rule");
    }
}

static esp_err_t delete_rules_handler(httpd_req_t *req)
{
    if (!check_api_auth(req)) return ESP_OK;
    char query[64] = {0};
    int query_len = httpd_req_get_url_query_len(req);

    if (query_len > 0 && query_len < sizeof(query)) {
        httpd_req_get_url_query_str(req, query, sizeof(query));
        char param[16] = {0};
        if (httpd_query_key_value(query, "id", param, sizeof(param)) == ESP_OK) {
            int rule_id = atoi(param);
            rule_engine_delete_rule(rule_id);
            httpd_resp_set_type(req, "application/json");
            httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
            return httpd_resp_send(req, "{\"status\":\"ok\",\"message\":\"Rule deleted\"}", HTTPD_RESP_USE_STRLEN);
        }
    }

    // If no ID specified, clear all
    rule_engine_clear_rules();
    set_common_headers(req, "application/json");
    return httpd_resp_send(req, "{\"status\":\"ok\",\"message\":\"All rules cleared\"}", HTTPD_RESP_USE_STRLEN);
}

static cJSON *recv_json_body(httpd_req_t *req, size_t max_len)
{
    if (req->content_len <= 0 || (size_t)req->content_len >= max_len) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid content length");
        return NULL;
    }

    char *buf = (char *)malloc((size_t)req->content_len + 1);
    if (!buf) {
        httpd_resp_send_500(req);
        return NULL;
    }

    int received = 0;
    while (received < req->content_len) {
        int r = httpd_req_recv(req, buf + received, req->content_len - received);
        if (r == HTTPD_SOCK_ERR_TIMEOUT) {
            continue;
        }
        if (r <= 0) {
            free(buf);
            httpd_resp_send_500(req);
            return NULL;
        }
        received += r;
    }
    buf[received] = '\0';

    cJSON *json = cJSON_Parse(buf);
    free(buf);
    if (!json) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return NULL;
    }
    return json;
}

static esp_err_t send_cjson_response(httpd_req_t *req, cJSON *root)
{
    if (!root) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    char *resp_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!resp_str) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    set_common_headers(req, "application/json");
    esp_err_t ret = httpd_resp_send(req, resp_str, strlen(resp_str));
    free(resp_str);
    return ret;
}

static const char *s_sched_day_names[7] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};

static void parse_repeat_days_str_local(const char *str, bool out_days[7])
{
    if (!str) return;
    char buf[64];
    strncpy(buf, str, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *saveptr = NULL;
    char *tok = strtok_r(buf, ",; ", &saveptr);
    while (tok != NULL) {
        for (int d = 0; d < 7; d++) {
            if (strcasecmp(tok, s_sched_day_names[d]) == 0 ||
                (tok[0] == ('0' + d) && tok[1] == '\0')) {
                out_days[d] = true;
                break;
            }
        }
        tok = strtok_r(NULL, ",; ", &saveptr);
    }
}

static esp_err_t get_schedules_handler(httpd_req_t *req)
{
    schedule_entry_t *entries = (schedule_entry_t *)calloc(MAX_SCHEDULE_ENTRIES, sizeof(schedule_entry_t));
    if (!entries) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    int count = scheduler_get_entries(entries, MAX_SCHEDULE_ENTRIES);
    cJSON *arr = cJSON_CreateArray();
    if (!arr) {
        free(entries);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    for (int i = 0; i < count; i++) {
        const schedule_entry_t *e = &entries[i];
        cJSON *item = cJSON_CreateObject();
        if (!item) continue;

        char rd_str[32] = {0};
        size_t pos = 0;
        for (int d = 0; d < 7; d++) {
            if (!e->repeat_days[d]) continue;
            int n = snprintf(rd_str + pos, sizeof(rd_str) - pos, "%s%s", (pos > 0) ? "," : "", s_sched_day_names[d]);
            if (n < 0 || (size_t)n >= sizeof(rd_str) - pos) break;
            pos += (size_t)n;
        }

        cJSON_AddNumberToObject(item, "id", e->id);
        cJSON_AddStringToObject(item, "type", e->type);
        cJSON_AddNumberToObject(item, "hour", e->hour);
        cJSON_AddNumberToObject(item, "minute", e->minute);
        cJSON_AddStringToObject(item, "repeat_days", rd_str);
        cJSON_AddStringToObject(item, "one_time_date", e->one_time_date);
        cJSON_AddBoolToObject(item, "enabled", e->enabled);
        cJSON_AddStringToObject(item, "target_device_id", e->target_device_id);
        cJSON_AddStringToObject(item, "action_command", e->action_command);
        cJSON_AddStringToObject(item, "title", e->title);
        cJSON_AddBoolToObject(item, "buzzer", e->buzzer);
        cJSON_AddItemToArray(arr, item);
    }

    free(entries);
    return send_cjson_response(req, arr);
}

static esp_err_t post_schedules_handler(httpd_req_t *req)
{
    if (!check_api_auth(req)) return ESP_OK;
    cJSON *json = recv_json_body(req, 1024);
    if (!json) return ESP_FAIL;

    cJSON *j_hour = cJSON_GetObjectItem(json, "hour");
    cJSON *j_min = cJSON_GetObjectItem(json, "minute");
    if (!j_hour || !cJSON_IsNumber(j_hour) || !j_min || !cJSON_IsNumber(j_min)) {
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing hour or minute");
        return ESP_FAIL;
    }

    schedule_entry_t entry;
    memset(&entry, 0, sizeof(entry));

    cJSON *j_id = cJSON_GetObjectItem(json, "id");
    entry.id = (j_id && cJSON_IsNumber(j_id)) ? j_id->valueint : (scheduler_get_entry_count() + 1);

    cJSON *j_type = cJSON_GetObjectItem(json, "type");
    if (j_type && cJSON_IsString(j_type) && j_type->valuestring[0] != '\0') {
        strncpy(entry.type, j_type->valuestring, sizeof(entry.type) - 1);
    } else {
        strncpy(entry.type, "alarm", sizeof(entry.type) - 1);
    }

    entry.hour = j_hour->valueint;
    entry.minute = j_min->valueint;
    if (entry.hour < 0) entry.hour = 0;
    if (entry.hour > 23) entry.hour = 23;
    if (entry.minute < 0) entry.minute = 0;
    if (entry.minute > 59) entry.minute = 59;

    cJSON *j_rd = cJSON_GetObjectItem(json, "repeat_days");
    if (!j_rd) j_rd = cJSON_GetObjectItem(json, "repeat_days_indices");
    if (j_rd && cJSON_IsString(j_rd)) {
        parse_repeat_days_str_local(j_rd->valuestring, entry.repeat_days);
    } else if (j_rd && cJSON_IsArray(j_rd)) {
        int n = cJSON_GetArraySize(j_rd);
        cJSON *first = (n > 0) ? cJSON_GetArrayItem(j_rd, 0) : NULL;
        if (n == 7 && first && cJSON_IsBool(first)) {
            for (int d = 0; d < 7; d++) {
                cJSON *el = cJSON_GetArrayItem(j_rd, d);
                entry.repeat_days[d] = (el != NULL) && cJSON_IsTrue(el);
            }
        } else {
            for (int i = 0; i < n; i++) {
                cJSON *el = cJSON_GetArrayItem(j_rd, i);
                if (el && cJSON_IsNumber(el) && el->valueint >= 0 && el->valueint < 7) {
                    entry.repeat_days[el->valueint] = true;
                } else if (el && cJSON_IsString(el)) {
                    parse_repeat_days_str_local(el->valuestring, entry.repeat_days);
                }
            }
        }
    }

    cJSON *j_otd = cJSON_GetObjectItem(json, "one_time_date");
    if (j_otd && cJSON_IsString(j_otd)) {
        strncpy(entry.one_time_date, j_otd->valuestring, sizeof(entry.one_time_date) - 1);
    }

    cJSON *j_en = cJSON_GetObjectItem(json, "enabled");
    entry.enabled = (j_en != NULL) ? cJSON_IsTrue(j_en) : true;

    cJSON *j_tgt = cJSON_GetObjectItem(json, "target_device_id");
    if (j_tgt && cJSON_IsString(j_tgt)) {
        strncpy(entry.target_device_id, j_tgt->valuestring, sizeof(entry.target_device_id) - 1);
    }

    cJSON *j_act = cJSON_GetObjectItem(json, "action_command");
    if (j_act && cJSON_IsString(j_act)) {
        strncpy(entry.action_command, j_act->valuestring, sizeof(entry.action_command) - 1);
    }

    cJSON *j_title = cJSON_GetObjectItem(json, "title");
    if (j_title && cJSON_IsString(j_title)) {
        strncpy(entry.title, j_title->valuestring, sizeof(entry.title) - 1);
    }

    cJSON *j_buz = cJSON_GetObjectItem(json, "buzzer");
    entry.buzzer = (j_buz != NULL) ? cJSON_IsTrue(j_buz) : (strcmp(entry.type, "automation") != 0);

    cJSON_Delete(json);

    bool ok = scheduler_add_entry(&entry);
    set_common_headers(req, "application/json");
    if (ok) {
        return httpd_resp_send(req, "{\"status\":\"ok\",\"message\":\"Schedule saved\"}", HTTPD_RESP_USE_STRLEN);
    }
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to save schedule");
    return ESP_FAIL;
}

static esp_err_t delete_schedules_handler(httpd_req_t *req)
{
    if (!check_api_auth(req)) return ESP_OK;
    char query[64] = {0};
    int query_len = httpd_req_get_url_query_len(req);

    if (query_len > 0 && query_len < (int)sizeof(query)) {
        if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
            char param[16] = {0};
            if (httpd_query_key_value(query, "id", param, sizeof(param)) == ESP_OK) {
                int entry_id = atoi(param);
                bool ok = scheduler_delete_entry(entry_id);
                if (ok) {
                    set_common_headers(req, "application/json");
                    return httpd_resp_send(req, "{\"status\":\"ok\",\"message\":\"Schedule deleted\"}", HTTPD_RESP_USE_STRLEN);
                }
                httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Schedule not found");
                return ESP_FAIL;
            }
        }
    }

    scheduler_clear_entries();
    set_common_headers(req, "application/json");
    return httpd_resp_send(req, "{\"status\":\"ok\",\"message\":\"All schedules cleared\"}", HTTPD_RESP_USE_STRLEN);
}

static esp_err_t post_rules_test_handler(httpd_req_t *req)
{
    if (!check_api_auth(req)) return ESP_OK;
    cJSON *json = recv_json_body(req, 512);
    if (!json) return ESP_FAIL;

    cJSON *j_reset = cJSON_GetObjectItem(json, "reset");
    cJSON *j_dev = cJSON_GetObjectItem(json, "device_id");
    cJSON *j_val = cJSON_GetObjectItem(json, "value");

    if (j_reset && cJSON_IsTrue(j_reset)) {
        rule_engine_reset_lockouts();
        if (!j_dev) {
            cJSON_Delete(json);
            set_common_headers(req, "application/json");
            return httpd_resp_send(req, "{\"status\":\"ok\",\"reset\":true}", HTTPD_RESP_USE_STRLEN);
        }
    }

    if (!j_dev || !cJSON_IsString(j_dev) || !j_val || !cJSON_IsNumber(j_val)) {
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing device_id or value");
        return ESP_FAIL;
    }

    sensor_telemetry_t telem;
    memset(&telem, 0, sizeof(telem));
    strncpy(telem.device_id, j_dev->valuestring, sizeof(telem.device_id) - 1);
    telem.value = (float)j_val->valuedouble;

    cJSON *j_hum = cJSON_GetObjectItem(json, "humidity");
    if (j_hum && cJSON_IsNumber(j_hum)) {
        telem.humidity = (float)j_hum->valuedouble;
    }

    cJSON *j_valid = cJSON_GetObjectItem(json, "valid");
    telem.valid = (j_valid != NULL) ? cJSON_IsTrue(j_valid) : true;

    time_t now = time(NULL);
    telem.timestamp = (now > 0) ? (uint32_t)now : (uint32_t)(esp_timer_get_time() / 1000000ULL);
    cJSON_Delete(json);

    int trig = rule_engine_evaluate_telemetry(&telem);

    cJSON *resp = cJSON_CreateObject();
    if (!resp) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    cJSON_AddStringToObject(resp, "status", "ok");
    cJSON_AddStringToObject(resp, "device_id", telem.device_id);
    cJSON_AddNumberToObject(resp, "value", telem.value);
    cJSON_AddBoolToObject(resp, "valid", telem.valid);
    cJSON_AddNumberToObject(resp, "triggered_count", trig);
    cJSON_AddBoolToObject(resp, "safe_default", !telem.valid);
    return send_cjson_response(req, resp);
}

static esp_err_t post_alarm_handler(httpd_req_t *req)
{
    if (!check_api_auth(req)) return ESP_OK;
    cJSON *json = recv_json_body(req, 512);
    if (!json) return ESP_FAIL;

    cJSON *j_act = cJSON_GetObjectItem(json, "action");
    if (!j_act || !cJSON_IsString(j_act)) {
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing action");
        return ESP_FAIL;
    }

    if (strcmp(j_act->valuestring, "trigger") == 0) {
        cJSON *j_title = cJSON_GetObjectItem(json, "title");
        cJSON *j_buz = cJSON_GetObjectItem(json, "buzzer");
        const char *title = (j_title && cJSON_IsString(j_title) && j_title->valuestring[0] != '\0')
                                ? j_title->valuestring
                                : "Manual Alarm";
        bool buzzer = (j_buz != NULL) ? cJSON_IsTrue(j_buz) : true;
        scheduler_trigger_alarm(title, buzzer);
        cJSON_Delete(json);
        set_common_headers(req, "application/json");
        return httpd_resp_send(req, "{\"status\":\"ok\",\"action\":\"trigger\",\"alarm_active\":true}", HTTPD_RESP_USE_STRLEN);
    }

    if (strcmp(j_act->valuestring, "dismiss") == 0) {
        cJSON_Delete(json);
        display_stop_alarm();
        set_common_headers(req, "application/json");
        return httpd_resp_send(req, "{\"status\":\"ok\",\"action\":\"dismiss\",\"alarm_active\":false}", HTTPD_RESP_USE_STRLEN);
    }

    if (strcmp(j_act->valuestring, "snooze") == 0) {
        cJSON *j_min = cJSON_GetObjectItem(json, "minutes");
        int minutes = (j_min && cJSON_IsNumber(j_min) && j_min->valueint > 0) ? j_min->valueint : 9;
        if (minutes > 255) minutes = 255;
        cJSON_Delete(json);
        display_snooze_alarm((uint8_t)minutes);
        set_common_headers(req, "application/json");
        return httpd_resp_send(req, "{\"status\":\"ok\",\"action\":\"snooze\"}", HTTPD_RESP_USE_STRLEN);
    }

    cJSON_Delete(json);
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Unknown alarm action");
    return ESP_FAIL;
}

static esp_err_t post_time_handler(httpd_req_t *req)
{
    if (!check_api_auth(req)) return ESP_OK;
    cJSON *json = recv_json_body(req, 256);
    if (!json) return ESP_FAIL;

    cJSON *j_dt = cJSON_GetObjectItem(json, "datetime");
    cJSON *j_act = cJSON_GetObjectItem(json, "action");

    if (j_dt && cJSON_IsString(j_dt)) {
        int y = 0, m = 0, d = 0, hh = 0, mm = 0, ss = 0;
        if (sscanf(j_dt->valuestring, "%d-%d-%d %d:%d:%d", &y, &m, &d, &hh, &mm, &ss) != 6 ||
            y < 2000 || m < 1 || m > 12 || d < 1 || d > 31 ||
            hh < 0 || hh > 23 || mm < 0 || mm > 59 || ss < 0 || ss > 59) {
            cJSON_Delete(json);
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid datetime format");
            return ESP_FAIL;
        }
        cJSON_Delete(json);

        struct tm ti;
        memset(&ti, 0, sizeof(ti));
        ti.tm_year = y - 1900;
        ti.tm_mon = m - 1;
        ti.tm_mday = d;
        ti.tm_hour = hh;
        ti.tm_min = mm;
        ti.tm_sec = ss;
        ti.tm_isdst = -1;

        if (!app_rtc_set_time(&ti)) {
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }

        char tbuf[32] = {0};
        app_rtc_get_formatted_time(tbuf, sizeof(tbuf));
        cJSON *resp = cJSON_CreateObject();
        if (!resp) {
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        cJSON_AddStringToObject(resp, "status", "ok");
        cJSON_AddStringToObject(resp, "time", tbuf);
        return send_cjson_response(req, resp);
    }

    if (j_act && cJSON_IsString(j_act) && strcmp(j_act->valuestring, "sync") == 0) {
        cJSON_Delete(json);
        app_rtc_sync_from_ntp_and_save();
        set_common_headers(req, "application/json");
        return httpd_resp_send(req, "{\"status\":\"ok\",\"action\":\"sync\"}", HTTPD_RESP_USE_STRLEN);
    }

    cJSON_Delete(json);
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing datetime or action");
    return ESP_FAIL;
}

static esp_err_t post_mqtt_control_handler(httpd_req_t *req)
{
    if (!check_api_auth(req)) return ESP_OK;
    cJSON *json = recv_json_body(req, 256);
    if (!json) return ESP_FAIL;

    cJSON *j_act = cJSON_GetObjectItem(json, "action");
    if (!j_act || !cJSON_IsString(j_act)) {
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing action");
        return ESP_FAIL;
    }

    if (strcmp(j_act->valuestring, "disconnect") == 0) {
        cJSON_Delete(json);
        mqtt_adapter_disconnect();
        set_common_headers(req, "application/json");
        return httpd_resp_send(req, "{\"status\":\"ok\",\"mqtt_connected\":false}", HTTPD_RESP_USE_STRLEN);
    }

    if (strcmp(j_act->valuestring, "reconnect") == 0) {
        cJSON_Delete(json);
        mqtt_adapter_reconnect();
        cJSON *resp = cJSON_CreateObject();
        if (!resp) {
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        cJSON_AddStringToObject(resp, "status", "ok");
        cJSON_AddBoolToObject(resp, "mqtt_connected", mqtt_adapter_is_connected());
        return send_cjson_response(req, resp);
    }

    if (strcmp(j_act->valuestring, "backfill") == 0) {
        cJSON_Delete(json);
        sd_logger_trigger_backfill();
        uint32_t total = 0, unacked = 0;
        sd_logger_get_sync_queue_stats(&total, &unacked);
        cJSON *resp = cJSON_CreateObject();
        if (!resp) {
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        cJSON_AddStringToObject(resp, "status", "ok");
        cJSON_AddNumberToObject(resp, "sync_queue_unacked", (double)unacked);
        return send_cjson_response(req, resp);
    }

    cJSON_Delete(json);
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Unknown mqtt control action");
    return ESP_FAIL;
}

static esp_err_t get_scan_handler(httpd_req_t *req)
{
    char query[64] = {0};
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        if (strstr(query, "refresh=1") || strstr(query, "force=1")) {
            wifi_provisioning_rescan();
        }
    }
    char *json_buf = (char *)malloc(1536);
    if (!json_buf) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    int len = wifi_provisioning_scan_to_json(json_buf, 1536);
    set_common_headers(req, "application/json");
    esp_err_t ret = httpd_resp_send(req, json_buf, len);
    free(json_buf);
    return ret;
}

static void url_decode(char *dst, const char *src, size_t max_dst)
{
    size_t d = 0;
    while (*src && d < max_dst - 1) {
        if (*src == '+') {
            dst[d++] = ' ';
            src++;
        } else if (*src == '%' && src[1] && src[2]) {
            char hex[3] = {src[1], src[2], 0};
            dst[d++] = (char)strtol(hex, NULL, 16);
            src += 3;
        } else {
            dst[d++] = *src++;
        }
    }
    dst[d] = '\0';
}

static esp_err_t post_save_handler(httpd_req_t *req)
{
    // BẢO MẬT: Cho phép SoftAP (192.168.4.1) hoặc thiết bị chưa cấu hình/chưa có pass SoftAP; mạng LAN phải có API Key
    const gateway_config_t *cfg = config_manager_get();
    bool allow_open = is_softap_client(req) || !wifi_provisioning_is_configured() ||
                      !cfg || cfg->softap_password[0] == '\0';
    if (!allow_open && !check_api_auth(req)) {
        return ESP_OK;
    }

    char buf[512];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    char ssid[33] = {0};
    char password[65] = {0};
    char softap_pass[65] = {0};

    cJSON *json = cJSON_Parse(buf);
    if (json) {
        cJSON *s = cJSON_GetObjectItem(json, "ssid");
        cJSON *p = cJSON_GetObjectItem(json, "password");
        cJSON *ap = cJSON_GetObjectItem(json, "softap_pass");
        if (s && cJSON_IsString(s)) strncpy(ssid, s->valuestring, sizeof(ssid) - 1);
        if (p && cJSON_IsString(p)) strncpy(password, p->valuestring, sizeof(password) - 1);
        if (ap && cJSON_IsString(ap)) strncpy(softap_pass, ap->valuestring, sizeof(softap_pass) - 1);
        cJSON_Delete(json);
    } else {
        char raw_ssid[64] = {0};
        char raw_pass[128] = {0};
        char raw_ap_pass[128] = {0};

        char *p_ssid = strstr(buf, "ssid=");
        char *p_pass = strstr(buf, "password=");
        char *p_ap_pass = strstr(buf, "softap_pass=");

        if (p_ssid) {
            p_ssid += 5;
            char *end = strchr(p_ssid, '&');
            int len = end ? (end - p_ssid) : strlen(p_ssid);
            if (len > sizeof(raw_ssid) - 1) len = sizeof(raw_ssid) - 1;
            strncpy(raw_ssid, p_ssid, len);
            url_decode(ssid, raw_ssid, sizeof(ssid));
        }
        if (p_pass) {
            p_pass += 9;
            char *end = strchr(p_pass, '&');
            int len = end ? (end - p_pass) : strlen(p_pass);
            if (len > sizeof(raw_pass) - 1) len = sizeof(raw_pass) - 1;
            strncpy(raw_pass, p_pass, len);
            url_decode(password, raw_pass, sizeof(password));
        }
        if (p_ap_pass) {
            p_ap_pass += 12;
            char *end = strchr(p_ap_pass, '&');
            int len = end ? (end - p_ap_pass) : strlen(p_ap_pass);
            if (len > sizeof(raw_ap_pass) - 1) len = sizeof(raw_ap_pass) - 1;
            strncpy(raw_ap_pass, p_ap_pass, len);
            url_decode(softap_pass, raw_ap_pass, sizeof(softap_pass));
        }
    }

    ESP_LOGI(TAG, "Received Wi-Fi setup: SSID='%s', SoftAP pass len=%zu", ssid, strlen(softap_pass));
    if (strlen(ssid) > 0) {
        config_manager_add_or_update_wifi(ssid, password);
    }
    if (strlen(softap_pass) >= 8) {
        config_manager_set_softap_password(softap_pass);
        ESP_LOGI(TAG, "Configured new SoftAP password (length: %zu)", strlen(softap_pass));
        wifi_provisioning_apply_softap_config();
    } else if (strlen(softap_pass) > 0) {
        ESP_LOGW(TAG, "SoftAP password too short (< 8 chars), keeping current password.");
    }
    config_manager_save();

    set_common_headers(req, "text/html; charset=utf-8");
    httpd_resp_send(req,
        "<!DOCTYPE html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<style>body{background:#0d1117;color:#c9d1d9;font-family:sans-serif;display:flex;justify-content:center;align-items:center;height:100vh;margin:0;}"
        ".card{background:#161b22;padding:28px;border-radius:12px;text-align:center;border:1px solid #30363d;max-width:380px;box-shadow:0 8px 24px rgba(0,0,0,0.4);}</style></head>"
        "<body><div class='card'>"
        "<h2 style='color:#3fb950;margin-bottom:12px;'>✓ Đã Lưu Thành Công!</h2>"
        "<p style='font-size:14px;line-height:1.6;'>Gateway đang kết nối vào Wi-Fi nhà bạn và kích hoạt mật khẩu bảo vệ SoftAP.</p>"
        "<p style='font-size:12px;color:#8b949e;margin-top:16px;'>Màn hình OLED đã chuyển sang hiển thị Đồng hồ. Bạn có thể đóng trang này.</p>"
        "</div></body></html>", HTTPD_RESP_USE_STRLEN);

    display_set_screen(DISPLAY_SCREEN_CLOCK_TEMP);
    event_bus_post(EVT_WIFI_CREDENTIALS_RECEIVED, NULL, 0, false);
    if (strlen(ssid) > 0) {
        vTaskDelay(pdMS_TO_TICKS(500));
        wifi_provisioning_connect(ssid, password);
    }
    return ESP_OK;
}

static esp_err_t post_softap_set_password_handler(httpd_req_t *req)
{
    if (!check_api_auth(req)) return ESP_OK;
    char buf[128];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    char password[65] = {0};
    cJSON *json = cJSON_Parse(buf);
    if (json) {
        cJSON *p = cJSON_GetObjectItem(json, "password");
        if (p && cJSON_IsString(p)) {
            strncpy(password, p->valuestring, sizeof(password) - 1);
        }
        cJSON_Delete(json);
    } else {
        char *p_pass = strstr(buf, "password=");
        if (p_pass) {
            p_pass += 9;
            char *end = strchr(p_pass, '&');
            int len = end ? (end - p_pass) : strlen(p_pass);
            if (len > 64) len = 64;
            char raw_pass[128] = {0};
            strncpy(raw_pass, p_pass, len);
            url_decode(password, raw_pass, sizeof(password));
        }
    }

    if (strlen(password) < 8) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Password must be at least 8 characters");
        return ESP_FAIL;
    }

    config_manager_set_softap_password(password);
    ESP_LOGI(TAG, "Configured new SoftAP password (length: %zu)", strlen(password));
    wifi_provisioning_apply_softap_config();

    // Reset màn hình OLED về màn đồng hồ ngay sau khi cài mật khẩu thành công
    display_set_screen(DISPLAY_SCREEN_CLOCK_TEMP);
    event_bus_post(EVT_WIFI_CREDENTIALS_RECEIVED, NULL, 0, false);

    set_common_headers(req, "application/json");
    return httpd_resp_send(req, "{\"status\":\"ok\"}", HTTPD_RESP_USE_STRLEN);
}

static esp_err_t get_wifi_saved_handler(httpd_req_t *req)
{
    if (!is_softap_client(req) && !check_api_auth(req)) return ESP_OK;
    const gateway_config_t *cfg = config_manager_get();
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "active_ssid", cfg ? cfg->wifi_ssid : "");
    cJSON_AddBoolToObject(root, "connected", wifi_provisioning_is_connected());
    cJSON_AddStringToObject(root, "ip", wifi_provisioning_get_ip());

    cJSON *arr = cJSON_CreateArray();
    uint8_t count = config_manager_get_saved_wifi_count();
    for (uint8_t i = 0; i < count; i++) {
        char s[32] = {0}, p[64] = {0};
        if (config_manager_get_saved_wifi(i, s, sizeof(s), p, sizeof(p))) {
            cJSON *item = cJSON_CreateObject();
            cJSON_AddStringToObject(item, "ssid", s);
            cJSON_AddBoolToObject(item, "active", (cfg && strcmp(s, cfg->wifi_ssid) == 0));
            cJSON_AddItemToArray(arr, item);
        }
    }
    cJSON_AddItemToObject(root, "saved", arr);

    char *resp_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    set_common_headers(req, "application/json");
    esp_err_t ret = httpd_resp_send(req, resp_str, strlen(resp_str));
    free(resp_str);
    return ret;
}

static esp_err_t post_wifi_save_handler(httpd_req_t *req)
{
    if (!is_softap_client(req) && !check_api_auth(req)) return ESP_OK;
    char buf[256];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    char ssid[33] = {0};
    char password[65] = {0};
    bool connect_now = true;

    cJSON *json = cJSON_Parse(buf);
    if (json) {
        cJSON *s = cJSON_GetObjectItem(json, "ssid");
        cJSON *p = cJSON_GetObjectItem(json, "password");
        cJSON *c = cJSON_GetObjectItem(json, "connect");
        if (s && cJSON_IsString(s)) strncpy(ssid, s->valuestring, sizeof(ssid) - 1);
        if (p && cJSON_IsString(p)) strncpy(password, p->valuestring, sizeof(password) - 1);
        if (c && cJSON_IsBool(c)) connect_now = cJSON_IsTrue(c);
        cJSON_Delete(json);
    } else {
        char *p_ssid = strstr(buf, "ssid=");
        char *p_pass = strstr(buf, "password=");
        if (p_ssid) {
            p_ssid += 5;
            char *end = strchr(p_ssid, '&');
            int len = end ? (end - p_ssid) : strlen(p_ssid);
            if (len > 32) len = 32;
            char raw_ssid[33] = {0};
            strncpy(raw_ssid, p_ssid, len);
            url_decode(ssid, raw_ssid, sizeof(ssid));
        }
        if (p_pass) {
            p_pass += 9;
            char *end = strchr(p_pass, '&');
            int len = end ? (end - p_pass) : strlen(p_pass);
            if (len > 64) len = 64;
            char raw_pass[128] = {0};
            strncpy(raw_pass, p_pass, len);
            url_decode(password, raw_pass, sizeof(password));
        }
    }

    if (strlen(ssid) == 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing SSID");
        return ESP_FAIL;
    }

    config_manager_add_or_update_wifi(ssid, password);
    ESP_LOGI(TAG, "Saved Wi-Fi '%s', connect_now=%d", ssid, connect_now);

    set_common_headers(req, "application/json");
    httpd_resp_send(req, "{\"status\":\"ok\"}", HTTPD_RESP_USE_STRLEN);

    display_set_screen(DISPLAY_SCREEN_CLOCK_TEMP);
    event_bus_post(EVT_WIFI_CREDENTIALS_RECEIVED, NULL, 0, false);

    if (connect_now) {
        vTaskDelay(pdMS_TO_TICKS(100));
        wifi_provisioning_connect(ssid, password);
    }
    return ESP_OK;
}

static esp_err_t post_wifi_delete_handler(httpd_req_t *req)
{
    if (!is_softap_client(req) && !check_api_auth(req)) return ESP_OK;
    char buf[128];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    char ssid[33] = {0};
    cJSON *json = cJSON_Parse(buf);
    if (json) {
        cJSON *s = cJSON_GetObjectItem(json, "ssid");
        if (s && cJSON_IsString(s)) strncpy(ssid, s->valuestring, sizeof(ssid) - 1);
        cJSON_Delete(json);
    } else {
        char *p_ssid = strstr(buf, "ssid=");
        if (p_ssid) {
            p_ssid += 5;
            char *end = strchr(p_ssid, '&');
            int len = end ? (end - p_ssid) : strlen(p_ssid);
            if (len > 32) len = 32;
            char raw_ssid[33] = {0};
            strncpy(raw_ssid, p_ssid, len);
            url_decode(ssid, raw_ssid, sizeof(ssid));
        }
    }

    if (strlen(ssid) == 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing SSID");
        return ESP_FAIL;
    }

    config_manager_delete_wifi(ssid);
    set_common_headers(req, "application/json");
    return httpd_resp_send(req, "{\"status\":\"ok\"}", HTTPD_RESP_USE_STRLEN);
}

static esp_err_t post_wifi_connect_handler(httpd_req_t *req)
{
    if (!is_softap_client(req) && !check_api_auth(req)) return ESP_OK;
    char buf[128];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    char ssid[33] = {0};
    cJSON *json = cJSON_Parse(buf);
    if (json) {
        cJSON *s = cJSON_GetObjectItem(json, "ssid");
        if (s && cJSON_IsString(s)) strncpy(ssid, s->valuestring, sizeof(ssid) - 1);
        cJSON_Delete(json);
    } else {
        char *p_ssid = strstr(buf, "ssid=");
        if (p_ssid) {
            p_ssid += 5;
            char *end = strchr(p_ssid, '&');
            int len = end ? (end - p_ssid) : strlen(p_ssid);
            if (len > 32) len = 32;
            char raw_ssid[33] = {0};
            strncpy(raw_ssid, p_ssid, len);
            url_decode(ssid, raw_ssid, sizeof(ssid));
        }
    }

    if (strlen(ssid) == 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing SSID");
        return ESP_FAIL;
    }

    char pass[65] = {0};
    uint8_t count = config_manager_get_saved_wifi_count();
    for (uint8_t i = 0; i < count; i++) {
        char s[32] = {0}, p[64] = {0};
        if (config_manager_get_saved_wifi(i, s, sizeof(s), p, sizeof(p)) && strcmp(s, ssid) == 0) {
            strncpy(pass, p, sizeof(pass) - 1);
            break;
        }
    }

    config_manager_set_wifi(ssid, pass);
    set_common_headers(req, "application/json");
    httpd_resp_send(req, "{\"status\":\"ok\"}", HTTPD_RESP_USE_STRLEN);

    event_bus_post(EVT_WIFI_CREDENTIALS_RECEIVED, NULL, 0, false);
    vTaskDelay(pdMS_TO_TICKS(100));
    wifi_provisioning_connect(ssid, pass);
    return ESP_OK;
}

static void delayed_reboot_task(void *pv)
{
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
}

static esp_err_t post_system_reboot_handler(httpd_req_t *req)
{
    if (!check_api_auth(req)) return ESP_OK;
    set_common_headers(req, "application/json");
    httpd_resp_send(req, "{\"status\":\"ok\"}", HTTPD_RESP_USE_STRLEN);
    xTaskCreate(delayed_reboot_task, "reboot_task", 2048, NULL, 5, NULL);
    return ESP_OK;
}

static esp_err_t post_system_factory_reset_handler(httpd_req_t *req)
{
    if (!check_api_auth(req)) return ESP_OK;
    set_common_headers(req, "application/json");
    httpd_resp_send(req, "{\"status\":\"ok\"}", HTTPD_RESP_USE_STRLEN);
    config_manager_factory_reset();
    xTaskCreate(delayed_reboot_task, "reboot_task", 2048, NULL, 5, NULL);
    return ESP_OK;
}

static esp_err_t post_system_ota_handler(httpd_req_t *req)
{
    if (!check_api_auth(req)) return ESP_OK;
    if (req->content_len <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty OTA payload");
        return ESP_FAIL;
    }

    const esp_partition_t *update_part = esp_ota_get_next_update_partition(NULL);
    if (!update_part) {
        ESP_LOGE(TAG, "No OTA update partition found");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    esp_ota_handle_t ota_handle = 0;
    esp_err_t err = esp_ota_begin(update_part, OTA_SIZE_UNKNOWN, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed: 0x%x", err);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    char *buf = (char *)malloc(1024);
    if (!buf) {
        esp_ota_abort(ota_handle);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    int remaining = req->content_len;
    while (remaining > 0) {
        int to_read = (remaining > 1024) ? 1024 : remaining;
        int r = httpd_req_recv(req, buf, to_read);
        if (r == HTTPD_SOCK_ERR_TIMEOUT) {
            continue;
        }
        if (r <= 0) {
            ESP_LOGE(TAG, "OTA recv error: %d", r);
            free(buf);
            esp_ota_abort(ota_handle);
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        err = esp_ota_write(ota_handle, buf, (size_t)r);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_write failed: 0x%x", err);
            free(buf);
            esp_ota_abort(ota_handle);
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        remaining -= r;
    }
    free(buf);

    err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed: 0x%x", err);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    err = esp_ota_set_boot_partition(update_part);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: 0x%x", err);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "OTA update succeeded (%d bytes), rebooting...", req->content_len);
    set_common_headers(req, "application/json");
    httpd_resp_send(req, "{\"status\":\"ok\",\"message\":\"OTA successful, rebooting\"}", HTTPD_RESP_USE_STRLEN);
    xTaskCreate(delayed_reboot_task, "reboot_task", 2048, NULL, 5, NULL);
    return ESP_OK;
}

static esp_err_t http_captive_redirect_handler(httpd_req_t *req)
{
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    httpd_resp_set_hdr(req, "Connection", "close");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    httpd_resp_set_hdr(req, "Connection", "close");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

// ==========================================
// SERVER INITIALIZATION
// ==========================================

esp_err_t web_server_start(void)
{
    if (s_server != NULL) {
        ESP_LOGW(TAG, "Web server already running");
        return ESP_OK;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 30;
    config.max_open_sockets = 5;
    config.stack_size = 6144;
    config.lru_purge_enable = true;

    esp_err_t ret = httpd_start(&s_server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: 0x%x", ret);
        return ret;
    }

    // 1. Web UI & Captive Portal Routes
    httpd_uri_t uri_root = { .uri = "/", .method = HTTP_GET, .handler = get_dashboard_handler };
    httpd_register_uri_handler(s_server, &uri_root);

    // Standard Captive Portal redirects (Apple, Android, Windows)
    httpd_uri_t uri_hotspot = { .uri = "/hotspot-detect.html", .method = HTTP_GET, .handler = http_captive_redirect_handler };
    httpd_register_uri_handler(s_server, &uri_hotspot);

    httpd_uri_t uri_204 = { .uri = "/generate_204", .method = HTTP_GET, .handler = http_captive_redirect_handler };
    httpd_register_uri_handler(s_server, &uri_204);

    httpd_uri_t uri_gen204 = { .uri = "/gen_204", .method = HTTP_GET, .handler = http_captive_redirect_handler };
    httpd_register_uri_handler(s_server, &uri_gen204);

    httpd_uri_t uri_connecttest = { .uri = "/connecttest.txt", .method = HTTP_GET, .handler = http_captive_redirect_handler };
    httpd_register_uri_handler(s_server, &uri_connecttest);

    httpd_uri_t uri_ncsi = { .uri = "/ncsi.txt", .method = HTTP_GET, .handler = http_captive_redirect_handler };
    httpd_register_uri_handler(s_server, &uri_ncsi);

    // 404 Catch-All Redirect to Root
    httpd_register_err_handler(s_server, HTTPD_404_NOT_FOUND, http_404_error_handler);

    // 2. REST API Routes
    httpd_uri_t uri_status = { .uri = "/api/status", .method = HTTP_GET, .handler = get_status_handler };
    httpd_register_uri_handler(s_server, &uri_status);

    httpd_uri_t uri_telemetry = { .uri = "/api/telemetry", .method = HTTP_GET, .handler = get_telemetry_handler };
    httpd_register_uri_handler(s_server, &uri_telemetry);

    httpd_uri_t uri_relay = { .uri = "/api/relay", .method = HTTP_POST, .handler = post_relay_handler };
    httpd_register_uri_handler(s_server, &uri_relay);

    httpd_uri_t uri_rules_get = { .uri = "/api/rules", .method = HTTP_GET, .handler = get_rules_handler };
    httpd_register_uri_handler(s_server, &uri_rules_get);

    httpd_uri_t uri_rules_post = { .uri = "/api/rules", .method = HTTP_POST, .handler = post_rules_handler };
    httpd_register_uri_handler(s_server, &uri_rules_post);

    httpd_uri_t uri_rules_del = { .uri = "/api/rules", .method = HTTP_DELETE, .handler = delete_rules_handler };
    httpd_register_uri_handler(s_server, &uri_rules_del);

    httpd_uri_t uri_rules_test = { .uri = "/api/rules/test", .method = HTTP_POST, .handler = post_rules_test_handler };
    httpd_register_uri_handler(s_server, &uri_rules_test);

    httpd_uri_t uri_sched_get = { .uri = "/api/schedules", .method = HTTP_GET, .handler = get_schedules_handler };
    httpd_register_uri_handler(s_server, &uri_sched_get);

    httpd_uri_t uri_sched_post = { .uri = "/api/schedules", .method = HTTP_POST, .handler = post_schedules_handler };
    httpd_register_uri_handler(s_server, &uri_sched_post);

    httpd_uri_t uri_sched_del = { .uri = "/api/schedules", .method = HTTP_DELETE, .handler = delete_schedules_handler };
    httpd_register_uri_handler(s_server, &uri_sched_del);

    httpd_uri_t uri_alarm = { .uri = "/api/alarm", .method = HTTP_POST, .handler = post_alarm_handler };
    httpd_register_uri_handler(s_server, &uri_alarm);

    httpd_uri_t uri_time = { .uri = "/api/time", .method = HTTP_POST, .handler = post_time_handler };
    httpd_register_uri_handler(s_server, &uri_time);

    httpd_uri_t uri_mqtt_ctrl = { .uri = "/api/mqtt/control", .method = HTTP_POST, .handler = post_mqtt_control_handler };
    httpd_register_uri_handler(s_server, &uri_mqtt_ctrl);

    httpd_uri_t uri_scan = { .uri = "/scan", .method = HTTP_GET, .handler = get_scan_handler };
    httpd_register_uri_handler(s_server, &uri_scan);

    httpd_uri_t uri_save = { .uri = "/save", .method = HTTP_POST, .handler = post_save_handler };
    httpd_register_uri_handler(s_server, &uri_save);

    httpd_uri_t uri_softap_set_pass = { .uri = "/api/softap/set_password", .method = HTTP_POST, .handler = post_softap_set_password_handler };
    httpd_register_uri_handler(s_server, &uri_softap_set_pass);

    httpd_uri_t uri_wifi_saved = { .uri = "/api/wifi/saved", .method = HTTP_GET, .handler = get_wifi_saved_handler };
    httpd_register_uri_handler(s_server, &uri_wifi_saved);

    httpd_uri_t uri_wifi_save = { .uri = "/api/wifi/save", .method = HTTP_POST, .handler = post_wifi_save_handler };
    httpd_register_uri_handler(s_server, &uri_wifi_save);

    httpd_uri_t uri_wifi_del = { .uri = "/api/wifi/delete", .method = HTTP_POST, .handler = post_wifi_delete_handler };
    httpd_register_uri_handler(s_server, &uri_wifi_del);

    httpd_uri_t uri_wifi_conn = { .uri = "/api/wifi/connect", .method = HTTP_POST, .handler = post_wifi_connect_handler };
    httpd_register_uri_handler(s_server, &uri_wifi_conn);

    httpd_uri_t uri_reboot = { .uri = "/api/system/reboot", .method = HTTP_POST, .handler = post_system_reboot_handler };
    httpd_register_uri_handler(s_server, &uri_reboot);

    httpd_uri_t uri_factory_reset = { .uri = "/api/system/factory_reset", .method = HTTP_POST, .handler = post_system_factory_reset_handler };
    httpd_register_uri_handler(s_server, &uri_factory_reset);

    httpd_uri_t uri_ota = { .uri = "/api/system/ota", .method = HTTP_POST, .handler = post_system_ota_handler };
    httpd_register_uri_handler(s_server, &uri_ota);

    ESP_LOGI(TAG, "Gateway Web Server & REST API active on port 80 (Always-on)");
    return ESP_OK;
}

void web_server_stop(void)
{
    if (s_server != NULL) {
        httpd_stop(s_server);
        s_server = NULL;
        ESP_LOGI(TAG, "Web server stopped");
    }
}

bool web_server_is_running(void)
{
    return (s_server != NULL);
}
