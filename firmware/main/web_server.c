#include "web_server.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_http_server.h"
#include "cJSON.h"
#include "rule_engine.h"
#include "event_bus.h"
#include "rtc.h"
#include "supervisor.h"
#include "wifi_provisioning.h"
#include "config_manager.h"
#include "mqtt_adapter.h"
#include "mock_peripherals.h"

static const char *TAG = "WEB_SERVER";
static httpd_handle_t s_server = NULL;

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
"<button class='tab-btn active' onclick='switchTab(\"tab-dash\", this)'>📊 Giám Sát</button>"
"<button class='tab-btn' onclick='switchTab(\"tab-rules\", this)'>⚡ Quy Tắc Tự Động</button>"
"<button class='tab-btn' onclick='switchTab(\"tab-wifi\", this)'>📶 Cài Đặt Wi-Fi</button>"
"</div>"
"<div id='tab-dash' class='tab-content active'>"
"<div class='grid'>"
"<div class='card'>"
"<div class='card-title'><span>🌡️ Cảm Biến Nhiệt Độ (0x01)</span><span id='temp-status' class='badge green'>OK</span></div>"
"<div><span id='temp-val' class='metric-val'>--</span><span class='metric-unit'>°C</span></div>"
"<div class='metric-sub'>Modbus Register 0x0000 (FC03)</div>"
"</div>"
"<div class='card'>"
"<div class='card-title'><span>💧 Cảm Biến Độ Ẩm (0x01)</span><span id='hum-status' class='badge green'>OK</span></div>"
"<div><span id='hum-val' class='metric-val'>--</span><span class='metric-unit'>%RH</span></div>"
"<div class='metric-sub'>Modbus Register 0x0001 (FC03)</div>"
"</div>"
"<div class='card'>"
"<div class='card-title'><span>⏱️ Thời Gian & Hệ Thống</span></div>"
"<div style='font-size:14px;line-height:1.8;'>"
"<div><strong>Uptime:</strong> <span id='uptime-val'>--</span></div>"
"<div><strong>Địa chỉ IP:</strong> <span id='ip-val'>--</span></div>"
"<div><strong>Giám sát Task:</strong> <span id='task-health' class='badge green'>HEALTHY</span></div>"
"</div>"
"</div>"
"</div>"
"<div class='card' style='margin-bottom:16px;'>"
"<div class='card-title'><span>⚡ Điều Khiển Relay Công Nghiệp (Slave 0x02)</span></div>"
"<div class='relay-row'><div><strong>Relay 1: Quạt Làm Mát (fan_relay_01)</strong><div style='font-size:12px;color:#8b949e;'>Coil 0x0000</div></div><button id='btn-r1' class='btn btn-secondary' onclick='toggleRelay(\"fan_relay_01\", 0)'>...</button></div>"
"<div class='relay-row'><div><strong>Relay 2: Máy Sưởi (heater_relay_01)</strong><div style='font-size:12px;color:#8b949e;'>Coil 0x0001</div></div><button id='btn-r2' class='btn btn-secondary' onclick='toggleRelay(\"heater_relay_01\", 1)'>...</button></div>"
"<div class='relay-row'><div><strong>Relay 3: Máy Hút Ẩm (dehumidifier_01)</strong><div style='font-size:12px;color:#8b949e;'>Coil 0x0002</div></div><button id='btn-r3' class='btn btn-secondary' onclick='toggleRelay(\"dehumidifier_01\", 2)'>...</button></div>"
"<div class='relay-row'><div><strong>Relay 4: Thiết Bị Phụ (relay_ch4)</strong><div style='font-size:12px;color:#8b949e;'>Coil 0x0003</div></div><button id='btn-r4' class='btn btn-secondary' onclick='toggleRelay(\"relay_ch4\", 3)'>...</button></div>"
"</div>"
"</div>"
"<div id='tab-rules' class='tab-content'>"
"<div class='card' style='margin-bottom:16px;'>"
"<div class='card-title'><span>Danh Sách Quy Tắc Tự Động Hóa (Rule Engine)</span><button class='btn btn-danger' style='padding:4px 10px;' onclick='clearAllRules()'>Xóa Hết</button></div>"
"<div style='overflow-x:auto;'>"
"<table><thead><tr><th>ID</th><th>Cảm Biến</th><th>Điều Kiện</th><th>Ngưỡng</th><th>Thiết Bị Đích</th><th>Lệnh</th><th>Trạng Thái</th><th>Thao Tác</th></tr></thead>"
"<tbody id='rules-table-body'><tr><td colspan='8' style='text-align:center;'>Đang tải quy tắc...</td></tr></tbody></table>"
"</div>"
"</div>"
"<div class='card'>"
"<div class='card-title'><span>Thêm Quy Tắc Tự Động Mới</span></div>"
"<form id='add-rule-form' onsubmit='submitNewRule(event)'>"
"<div class='row-inputs'>"
"<div><label>ID Quy Tắc</label><input type='number' id='new-rule-id' required min='1' max='100' placeholder='VD: 4'></div>"
"<div><label>Nguồn Cảm Biến</label><select id='new-rule-sensor'><option value='sensor_temp_01'>sensor_temp_01 (Nhiệt độ)</option><option value='sensor_hum_01'>sensor_hum_01 (Độ ẩm)</option></select></div>"
"<div><label>Toán Tử So Sánh</label><select id='new-rule-cond'><option value='>'>Lớn hơn (&gt;)</option><option value='<'>Nhỏ hơn (&lt;)</option></select></div>"
"<div><label>Ngưỡng Kích Hoạt</label><input type='number' step='0.1' id='new-rule-thresh' required placeholder='VD: 32.5'></div>"
"</div>"
"<div class='row-inputs'>"
"<div><label>Thiết Bị Chấp Hành</label><select id='new-rule-target'><option value='fan_relay_01'>fan_relay_01 (Quạt)</option><option value='heater_relay_01'>heater_relay_01 (Sưởi)</option><option value='dehumidifier_01'>dehumidifier_01 (Hút ẩm)</option><option value='relay_ch4'>relay_ch4 (Relay 4)</option></select></div>"
"<div><label>Lệnh Điều Khiển</label><select id='new-rule-cmd'><option value='ON'>Bật (ON)</option><option value='OFF'>Tắt (OFF)</option><option value='START'>Khởi động (START)</option><option value='TOGGLE'>Đảo trạng thái (TOGGLE)</option></select></div>"
"</div>"
"<button type='submit' class='btn btn-primary' style='margin-top:8px;'>+ Thêm Quy Tắc Vào Hệ Thống</button>"
"</form>"
"</div>"
"</div>"
"<div id='tab-wifi' class='tab-content'>"
"<div class='card' style='max-width:480px;margin:0 auto;'>"
"<div class='card-title'><span>Cấu Hình Wi-Fi Gateway</span><button class='btn btn-secondary' style='padding:4px 10px;' onclick='fetchWifiScan()'>&#x21bb; Quét Mạng</button></div>"
"<div id='wifi-scan-status' style='font-size:12px;color:#8b949e;margin-bottom:8px;'>Sẵn sàng quét sóng Wi-Fi...</div>"
"<form onsubmit='submitWifiConfig(event)'>"
"<label>Chọn Mạng Wi-Fi (SSID)</label>"
"<select id='wifi-ssid-select' onchange='onSsidChange(this.value)'><option value='' disabled selected>-- Bấm quét lại để tìm Wi-Fi --</option></select>"
"<div id='wifi-manual-box' style='display:none;'><label>Tên Wi-Fi Tùy Chọn</label><input type='text' id='wifi-manual-ssid' placeholder='Nhập tên mạng ẩn'></div>"
"<label>Mật Khẩu</label><input type='password' id='wifi-password' placeholder='Nhập mật khẩu Wi-Fi'>"
"<button type='submit' class='btn btn-primary'>Lưu & Kết Nối</button>"
"</form>"
"</div>"
"</div>"
"</div>"
"<script>"
"var s_relays = [0, 0, 0, 0];"
"function switchTab(id, btn) {"
"  document.querySelectorAll('.tab-content').forEach(el => el.classList.remove('active'));"
"  document.querySelectorAll('.tab-btn').forEach(el => el.classList.remove('active'));"
"  document.getElementById(id).classList.add('active');"
"  btn.classList.add('active');"
"  if (id === 'tab-rules') fetchRules();"
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
"  fetch('/api/relay', {"
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
"      tbody.innerHTML = '<tr><td colspan=\"8\" style=\"text-align:center;color:#8b949e;\">Chưa có quy tắc nào</td></tr>';"
"      return;"
"    }"
"    rules.forEach(r => {"
"      var tr = document.createElement('tr');"
"      tr.innerHTML = '<td><strong>#' + r.id + '</strong></td>' +"
"        '<td>' + r.device_id + '</td>' +"
"        '<td>' + r.condition + '</td>' +"
"        '<td>' + r.threshold + '</td>' +"
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
"    target: document.getElementById('new-rule-target').value,"
"    action: document.getElementById('new-rule-cmd').value,"
"    enabled: true"
"  };"
"  fetch('/api/rules', {"
"    method: 'POST',"
"    headers: {'Content-Type': 'application/json'},"
"    body: JSON.stringify(payload)"
"  }).then(r => r.json()).then(d => {"
"    alert('Đã lưu quy tắc thành công!');"
"    fetchRules();"
"    document.getElementById('add-rule-form').reset();"
"  }).catch(e => alert('Lỗi: ' + e));"
"}"
"function deleteRule(id) {"
"  if (!confirm('Bạn có chắc muốn xóa Quy tắc #' + id + '?')) return;"
"  fetch('/api/rules?id=' + id, { method: 'DELETE' }).then(r => r.json()).then(() => {"
"    fetchRules();"
"  }).catch(e => alert('Lỗi xóa: ' + e));"
"}"
"function clearAllRules() {"
"  if (!confirm('Xóa TẤT CẢ quy tắc tự động?')) return;"
"  fetch('/api/rules', { method: 'DELETE' }).then(r => r.json()).then(() => {"
"    fetchRules();"
"  }).catch(e => alert('Lỗi: ' + e));"
"}"
"function fetchWifiScan() {"
"  var st = document.getElementById('wifi-scan-status');"
"  var sel = document.getElementById('wifi-ssid-select');"
"  st.innerText = 'Đang quét sóng Wi-Fi...';"
"  sel.innerHTML = '<option disabled selected>-- Đang quét... --</option>';"
"  fetch('/scan').then(r => r.json()).then(list => {"
"    sel.innerHTML = '<option value=\"\" disabled selected>-- Chọn mạng Wi-Fi --</option>';"
"    st.innerText = 'Tìm thấy ' + list.length + ' mạng Wi-Fi:';"
"    list.forEach(item => {"
"      var opt = document.createElement('option');"
"      opt.value = item.ssid;"
"      opt.text = item.ssid + ' (' + item.rssi + ' dBm)' + (item.auth>0?' [Khóa]':'');"
"      sel.appendChild(opt);"
"    });"
"    var man = document.createElement('option'); man.value = '__manual__'; man.text = '+ [Nhập mạng ẩn / thủ công]'; sel.appendChild(man);"
"  }).catch(e => { st.innerText = 'Lỗi quét sóng Wi-Fi'; });"
"}"
"function onSsidChange(val) {"
"  document.getElementById('wifi-manual-box').style.display = (val === '__manual__') ? 'block' : 'none';"
"}"
"function submitWifiConfig(e) {"
"  e.preventDefault();"
"  var selVal = document.getElementById('wifi-ssid-select').value;"
"  var ssid = (selVal === '__manual__') ? document.getElementById('wifi-manual-ssid').value : selVal;"
"  var pass = document.getElementById('wifi-password').value;"
"  if (!ssid) { alert('Vui lòng chọn hoặc nhập tên Wi-Fi!'); return; }"
"  var body = 'ssid=' + encodeURIComponent(ssid) + '&password=' + encodeURIComponent(pass);"
"  fetch('/save', {"
"    method: 'POST',"
"    headers: {'Content-Type': 'application/x-www-form-urlencoded'},"
"    body: body"
"  }).then(r => {"
"    alert('Đã lưu Wi-Fi! Gateway đang kết nối...');"
"  }).catch(e => alert('Lỗi: ' + e));"
"}"
"fetchStatus(); fetchTelemetry();"
"setInterval(fetchStatus, 3000);"
"setInterval(fetchTelemetry, 2000);"
"</script></body></html>";

// ==========================================
// HTTP HANDLERS
// ==========================================

static esp_err_t get_dashboard_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
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

    cJSON_AddNumberToObject(root, "uptime_s", (double)uptime_s);
    cJSON_AddNumberToObject(root, "free_heap", free_heap);
    cJSON_AddStringToObject(root, "ip", wifi_provisioning_get_ip());
    cJSON_AddBoolToObject(root, "wifi_connected", wifi_provisioning_is_connected());
    cJSON_AddBoolToObject(root, "mqtt_connected", mqtt_adapter_is_connected());
    cJSON_AddNumberToObject(root, "active_mask", active);
    cJSON_AddNumberToObject(root, "alive_mask", alive);
    cJSON_AddStringToObject(root, "time", time_str);

    char *resp_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    esp_err_t ret = httpd_resp_send(req, resp_str, strlen(resp_str));
    free(resp_str);
    return ret;
}

static esp_err_t get_telemetry_handler(httpd_req_t *req)
{
    float temp = 0.0f, hum = 0.0f;
    bool valid = true;
    mock_rs485_get_latest(&temp, &hum, &valid);

    uint8_t coils[4] = {0};
    mock_rs485_get_relays(coils);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "temp", temp);
    cJSON_AddNumberToObject(root, "hum", hum);
    cJSON_AddBoolToObject(root, "valid", valid);

    cJSON *relays = cJSON_CreateArray();
    for (int i = 0; i < 4; i++) {
        cJSON_AddItemToArray(relays, cJSON_CreateNumber(coils[i]));
    }
    cJSON_AddItemToObject(root, "relays", relays);

    char *resp_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    esp_err_t ret = httpd_resp_send(req, resp_str, strlen(resp_str));
    free(resp_str);
    return ret;
}

static esp_err_t post_relay_handler(httpd_req_t *req)
{
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
            strncpy(req_act->target_device_id, target->valuestring, sizeof(req_act->target_device_id) - 1);
            req_act->target_device_id[sizeof(req_act->target_device_id) - 1] = '\0';
            strncpy(req_act->action_command, command->valuestring, sizeof(req_act->action_command) - 1);
            req_act->action_command[sizeof(req_act->action_command) - 1] = '\0';
            req_act->source_rule_id = 0; // Web UI override

            event_bus_post(EVT_RULE_ACTION_REQUEST, req_act, sizeof(rule_action_request_t), false);
            ESP_LOGI(TAG, "Web UI dispatched action to '%s': '%s'", req_act->target_device_id, req_act->action_command);
        }
    }

    cJSON_Delete(json);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, "{\"status\":\"ok\"}", HTTPD_RESP_USE_STRLEN);
}

static esp_err_t get_rules_handler(httpd_req_t *req)
{
    automation_rule_t rules[MAX_RULES];
    int count = rule_engine_get_rules(rules, MAX_RULES);

    cJSON *arr = cJSON_CreateArray();
    for (int i = 0; i < count; i++) {
        cJSON *r = cJSON_CreateObject();
        cJSON_AddNumberToObject(r, "id", rules[i].id);
        cJSON_AddStringToObject(r, "device_id", rules[i].device_id);
        cJSON_AddStringToObject(r, "condition", (rules[i].condition_type == RULE_CONDITION_SENSOR_GT) ? ">" : "<");
        cJSON_AddNumberToObject(r, "threshold", rules[i].threshold);
        cJSON_AddStringToObject(r, "target", rules[i].target_device_id);
        cJSON_AddStringToObject(r, "action", rules[i].action_command);
        cJSON_AddBoolToObject(r, "enabled", rules[i].enabled);
        cJSON_AddItemToArray(arr, r);
    }

    char *resp_str = cJSON_PrintUnformatted(arr);
    cJSON_Delete(arr);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    esp_err_t ret = httpd_resp_send(req, resp_str, strlen(resp_str));
    free(resp_str);
    return ret;
}

static esp_err_t post_rules_handler(httpd_req_t *req)
{
    char buf[256];
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
    strncpy(rule.target_device_id, target->valuestring, sizeof(rule.target_device_id) - 1);
    strncpy(rule.action_command, act->valuestring, sizeof(rule.action_command) - 1);
    rule.enabled = (en != NULL) ? cJSON_IsTrue(en) : true;

    cJSON_Delete(json);

    bool ok = rule_engine_add_rule(&rule);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    if (ok) {
        return httpd_resp_send(req, "{\"status\":\"ok\",\"message\":\"Rule saved\"}", HTTPD_RESP_USE_STRLEN);
    } else {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to save rule");
    }
}

static esp_err_t delete_rules_handler(httpd_req_t *req)
{
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
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, "{\"status\":\"ok\",\"message\":\"All rules cleared\"}", HTTPD_RESP_USE_STRLEN);
}

static esp_err_t get_scan_handler(httpd_req_t *req)
{
    char json_buf[1536];
    int len = wifi_provisioning_scan_to_json(json_buf, sizeof(json_buf));
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, json_buf, len);
}

static esp_err_t post_save_handler(httpd_req_t *req)
{
    char buf[256];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    char ssid[33] = {0};
    char password[65] = {0};

    char *p_ssid = strstr(buf, "ssid=");
    char *p_pass = strstr(buf, "password=");

    if (p_ssid) {
        p_ssid += 5;
        char *end = strchr(p_ssid, '&');
        int len = end ? (end - p_ssid) : strlen(p_ssid);
        if (len > 32) len = 32;
        strncpy(ssid, p_ssid, len);
    }
    if (p_pass) {
        p_pass += 9;
        char *end = strchr(p_pass, '&');
        int len = end ? (end - p_pass) : strlen(p_pass);
        if (len > 64) len = 64;
        strncpy(password, p_pass, len);
    }

    ESP_LOGI(TAG, "Received Wi-Fi configuration: SSID='%s'", ssid);
    config_manager_set_wifi(ssid, password);
    config_manager_save();

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, "{\"status\":\"ok\",\"message\":\"Wi-Fi credentials saved\"}", HTTPD_RESP_USE_STRLEN);

    event_bus_post(EVT_WIFI_CREDENTIALS_RECEIVED, NULL, 0, false);
    vTaskDelay(pdMS_TO_TICKS(500));
    wifi_provisioning_connect(ssid, password);
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
    config.max_uri_handlers = 16;
    config.stack_size = 8192;
    config.lru_purge_enable = true;

    esp_err_t ret = httpd_start(&s_server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: 0x%x", ret);
        return ret;
    }

    // 1. Web UI Routes
    httpd_uri_t uri_root = { .uri = "/", .method = HTTP_GET, .handler = get_dashboard_handler };
    httpd_register_uri_handler(s_server, &uri_root);

    httpd_uri_t uri_hotspot = { .uri = "/hotspot-detect.html", .method = HTTP_GET, .handler = get_dashboard_handler };
    httpd_register_uri_handler(s_server, &uri_hotspot);

    httpd_uri_t uri_204 = { .uri = "/generate_204", .method = HTTP_GET, .handler = get_dashboard_handler };
    httpd_register_uri_handler(s_server, &uri_204);

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

    httpd_uri_t uri_scan = { .uri = "/scan", .method = HTTP_GET, .handler = get_scan_handler };
    httpd_register_uri_handler(s_server, &uri_scan);

    httpd_uri_t uri_save = { .uri = "/save", .method = HTTP_POST, .handler = post_save_handler };
    httpd_register_uri_handler(s_server, &uri_save);

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
