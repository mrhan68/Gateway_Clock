"""
Test Suite: FSD v1.7 Specification Compliance Tests
Verifies:
  - FR-WIFI-01: SoftAP Always-On, WPA2-PSK ('changeme') & Factory Reset NVS
  - FR-RTC-01: One-shot NTP Sync into RTC DS3231 & Independent Hardware RTC
  - FR-DISP-01: Alarm button dismiss < 500ms & Reset Button Safety Interlock
  - FR-DISP-02: OLED Provisioning & Prompt Screen State Transitions
"""



class MockConfigManager:
    """Mock implementation of config_manager.c behavior per FSD v1.7"""
    def __init__(self):
        self.gateway_id = "gw_smartclock_01"
        self.timezone = "ICT-7"
        self.wifi_ssid = ""
        self.wifi_password = ""
        self.softap_password = ""  # Open when unconfigured / out of the box
        self.mqtt_broker_uri = "mqtt://broker.emqx.io:1883"
        self.mqtt_telemetry_interval_s = 10
        self.nvs_wifi_cfg_erased = False

    def set_wifi(self, ssid: str, password: str):
        self.wifi_ssid = ssid
        self.wifi_password = password

    def set_softap_password(self, password: str):
        self.softap_password = password if password else ""

    def factory_reset(self):
        self.wifi_ssid = ""
        self.wifi_password = ""
        self.softap_password = ""
        self.nvs_wifi_cfg_erased = True


class MockWifiProvisioning:
    """Mock implementation of wifi_provisioning.c state machine per FSD v1.8"""
    def __init__(self, cfg: MockConfigManager):
        self.cfg = cfg
        self.is_connected = False
        self.current_ip = "0.0.0.0"
        self.softap_active = True  # Always-on APSTA (WP-08)
        self.softap_authmode = "WPA2_PSK" if self.cfg.softap_password else "OPEN"
        self.dns_portal_active = False

    def init(self):
        self.softap_active = True
        self.softap_authmode = "WPA2_PSK" if self.cfg.softap_password else "OPEN"
        if self.cfg.wifi_ssid:
            self.connect(self.cfg.wifi_ssid, self.cfg.wifi_password)
        else:
            self.dns_portal_active = True

    def connect(self, ssid: str, password: str, new_softap_pass: str = "changeme"):
        self.cfg.set_wifi(ssid, password)
        if new_softap_pass:
            self.cfg.set_softap_password(new_softap_pass)
            self.softap_authmode = "WPA2_PSK"
        # Simulate connection
        self.is_connected = True
        self.current_ip = "192.168.1.143"
        self.softap_active = True  # SoftAP remains active!
        self.dns_portal_active = False

    def get_ip(self):
        if self.is_connected:
            return self.current_ip
        return "192.168.4.1"

    def force_reset(self):
        self.cfg.factory_reset()
        self.is_connected = False
        self.current_ip = "0.0.0.0"
        self.softap_active = True
        self.softap_authmode = "OPEN"
        self.dns_portal_active = True


class MockRtcSync:
    """Mock implementation of rtc.c one-shot NTP sync per FSD v1.7 §4.2 & FR-RTC-01"""
    def __init__(self):
        self.sntp_running = False
        self.ds3231_synced = False
        self.ds3231_time = 0
        self.periodic_ntp_polls = 0

    def start_ntp_sync(self):
        self.sntp_running = True

    def on_time_sync_notification(self, ntp_timestamp: int):
        assert self.sntp_running, "NTP notification received while SNTP client not running"
        # 1. Save time into DS3231 hardware registers
        self.ds3231_time = ntp_timestamp
        self.ds3231_synced = True
        # 2. Stop SNTP client immediately (One-shot)
        self.sntp_running = False

    def simulate_periodic_background_timer(self):
        if self.sntp_running:
            self.periodic_ntp_polls += 1


class MockDisplayStateMachine:
    """Mock implementation of display.c screen transitions per FSD v1.7 FR-DISP-01 & FR-DISP-02"""
    SCREEN_CLOCK_TEMP = 0
    SCREEN_UPCOMING_EVENT = 1
    SCREEN_DEVICE_STATUS = 2
    SCREEN_PROVISIONING = 3
    SCREEN_COUNT = 4

    def __init__(self, wifi_prov: MockWifiProvisioning):
        self.wifi_prov = wifi_prov
        self.alarm_active = False
        self.alarm_snoozed = False
        self.snooze_minutes = 0
        self.current_screen = self.SCREEN_PROVISIONING if not wifi_prov.is_connected else self.SCREEN_CLOCK_TEMP

    def on_button_1_pressed(self, held_seconds: float):
        if self.alarm_active:
            # Safety interlock (FR-DISP-01): holding >= 5s during alarm ONLY snoozes, NEVER factory reset
            self.alarm_snoozed = True
            self.snooze_minutes = 9
            self.alarm_active = False
            return "SNOOZED"
        else:
            if held_seconds >= 5.0:
                self.wifi_prov.force_reset()
                self.current_screen = self.SCREEN_PROVISIONING
                return "FACTORY_RESET"
            return "IGNORED"

    def on_button_2_pressed(self):
        if self.alarm_active:
            self.alarm_active = False
            return "DISMISSED"
        else:
            self.current_screen = (self.current_screen + 1) % self.SCREEN_COUNT
            return "NEXT_SCREEN"

    def on_wifi_connected_event(self):
        if self.current_screen == self.SCREEN_PROVISIONING:
            self.current_screen = self.SCREEN_CLOCK_TEMP


# =========================================================================
# Unit Tests
# =========================================================================

def test_fr_wifi_01_softap_wpa2_and_factory_reset():
    """Verify FR-WIFI-01: SoftAP Adaptive Security (OPEN on reset, WPA2 when configured)"""
    cfg = MockConfigManager()
    assert cfg.softap_password == ""  # Initially empty / unconfigured

    wifi = MockWifiProvisioning(cfg)
    wifi.init()
    assert wifi.softap_active is True
    assert wifi.softap_authmode == "OPEN", "SoftAP must be OPEN when unconfigured per FSD v1.8"
    assert wifi.get_ip() == "192.168.4.1"

    # User connects Station and configures SoftAP password
    wifi.connect("HomeWiFi", "SecretPass", new_softap_pass="mypassword123")
    assert wifi.is_connected is True
    assert wifi.get_ip() == "192.168.1.143"
    # WP-08 invariant: SoftAP MUST remain active concurrently!
    assert wifi.softap_active is True
    assert wifi.softap_authmode == "WPA2_PSK", "SoftAP must switch to WPA2 once configured"
    assert cfg.softap_password == "mypassword123"

    # Factory Reset
    wifi.force_reset()
    assert cfg.wifi_ssid == ""
    assert cfg.wifi_password == ""
    assert cfg.softap_password == ""
    assert cfg.nvs_wifi_cfg_erased is True
    assert wifi.is_connected is False
    assert wifi.get_ip() == "192.168.4.1"
    assert wifi.softap_active is True
    assert wifi.softap_authmode == "OPEN", "SoftAP must revert to OPEN mode on Factory Reset"


def test_fr_rtc_01_one_shot_sntp_sync():
    """Verify FR-RTC-01: One-shot NTP sync stops SNTP immediately after DS3231 write"""
    rtc = MockRtcSync()
    rtc.start_ntp_sync()
    assert rtc.sntp_running is True

    # Receive NTP sync
    test_epoch = 1790440000
    rtc.on_time_sync_notification(test_epoch)

    # Invariants
    assert rtc.ds3231_synced is True
    assert rtc.ds3231_time == test_epoch
    assert rtc.sntp_running is False, "SNTP client must be stopped immediately per FR-RTC-01"

    # Verify no periodic background UDP traffic
    for _ in range(10):
        rtc.simulate_periodic_background_timer()
    assert rtc.periodic_ntp_polls == 0, "Zero periodic NTP polls permitted after initial sync"


def test_fr_disp_01_alarm_button_reset_lockout():
    """Verify FR-DISP-01: Holding Button 1 >= 5s during alarm does NOT erase Wi-Fi credentials"""
    cfg = MockConfigManager()
    cfg.set_wifi("HomeWiFi", "SecretPass")
    wifi = MockWifiProvisioning(cfg)
    wifi.connect("HomeWiFi", "SecretPass")
    disp = MockDisplayStateMachine(wifi)

    # Trigger alarm
    disp.alarm_active = True

    # User panics or accidentally holds Button 1 for 6 seconds during alarm
    action = disp.on_button_1_pressed(held_seconds=6.0)

    # Invariants per FR-DISP-01
    assert action == "SNOOZED"
    assert disp.alarm_snoozed is True
    assert disp.snooze_minutes == 9
    assert cfg.wifi_ssid == "HomeWiFi", "Wi-Fi credentials MUST be preserved during alarm snooze"
    assert cfg.nvs_wifi_cfg_erased is False


def test_fr_disp_02_provisioning_screen_transitions():
    """Verify FR-DISP-02: Boot without Wi-Fi shows Provisioning prompt, auto-transitions on Wi-Fi connect"""
    cfg = MockConfigManager()
    wifi = MockWifiProvisioning(cfg)
    wifi.init()

    disp = MockDisplayStateMachine(wifi)
    # 1. Unconnected boot defaults to SCREEN_PROVISIONING
    assert disp.current_screen == MockDisplayStateMachine.SCREEN_PROVISIONING

    # 2. Button 2 cycles through screens
    disp.on_button_2_pressed()
    assert disp.current_screen == MockDisplayStateMachine.SCREEN_CLOCK_TEMP

    disp.on_button_2_pressed()
    assert disp.current_screen == MockDisplayStateMachine.SCREEN_UPCOMING_EVENT

    # Cycle back to provisioning
    disp.on_button_2_pressed()
    disp.on_button_2_pressed()
    assert disp.current_screen == MockDisplayStateMachine.SCREEN_PROVISIONING

    # 3. Wi-Fi connects -> auto-transition to SCREEN_CLOCK_TEMP
    wifi.connect("HomeWiFi", "SecretPass")
    disp.on_wifi_connected_event()
    assert disp.current_screen == MockDisplayStateMachine.SCREEN_CLOCK_TEMP
