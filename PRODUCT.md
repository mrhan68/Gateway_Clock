# Product

<!-- impeccable:product-schema 1 -->

## Platform

web

## Users

Industrial plant operators, embedded systems engineers, and smart facility managers who monitor telemetry (temperature, humidity, sensor data) and control field hardware actuators in real-time.

## Product Purpose

A unified operations dashboard and edge management interface for the ESP32-S3 Smart Clock & Industrial Edge IoT Gateway. Provides autonomous edge status visibility, real-time telemetry streaming, direct 4-channel relay actuation, autonomous rule engine supervision, and offline RTC schedule synchronization.

## Positioning

Unlike cloud-dependent IoT platforms, this interface operates in tandem with edge-native intelligence: relays and schedules continue firing on physical ESP32-S3 hardware via FreeRTOS even during complete internet or cloud disconnects. The web UI reflects true edge state with high-density telemetry, millisecond tactile actuation, and audit logging.

## Operating Context

Used on desktop control monitors and mobile maintenance tablets in factory floors, server rooms, and agricultural setups. Operates in low-light/ambient environments where high-contrast dark surfaces, non-distracting status LEDs, and numeric clarity are essential.

## Capabilities and Constraints

- **Real-Time Telemetry**: BLE BTHome V2 and RS485 Modbus sensor streaming.
- **Hardware Actuators**: 4 GPIO relay channels (GPIO 15..18) with immediate visual feedback and confirmation.
- **Edge Rule Engine**: Condition-action evaluation on-device with multi-metric routing.
- **RTC Schedules**: Alarms, automation tasks, and calendar events stored in LittleFS Flash.
- **Security & Network**: JWT token auth, rate limiting (20 req/15 min), MQTTS TLS 8883 communication.
- **Visual Palette**: Strict industrial dark mode (`#09090b` / `#121215`), high-contrast state LEDs (Emerald for online/active, Amber for warning/threshold, Rose for offline/error, Sky/Cyan for telemetry/sync). No purple/violet AI slop or uncalibrated gradients.
- **Typography & Numerals**: High-legibility system UI stack with JetBrains Mono / tabular numbers (`tabular-nums`) for measurements, clocks, and counters.

## Product Principles

1. **Edge Truth First**: Never display synthetic or assumed state; always reflect true gateway acknowledgment or explicit disconnect warning.
2. **Speed & Tactility**: Relay toggles and sensor filters must react immediately with clear loading and active states.
3. **Information Density without Clutter**: Maximize actionable telemetry metrics per viewport using tight mathematical rhythm and clean data grids.
4. **Resilience & Continuity**: Clear graceful degradation during network dropouts; provide immediate endpoint configuration options.
