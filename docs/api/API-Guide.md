# Smart Clock Gateway API Guide

> **REST API reference for the Smart Clock IoT Gateway Backend**

This guide covers all REST endpoints, request/response schemas, authentication, and error handling for the Backend server. The OpenAPI specification (machine-readable) lives alongside this file as `SmartClockAPI.yaml`.

## Table of Contents

- [Architecture Overview](#architecture-overview)
- [Authentication](#authentication)
  - [JWT Bearer (Web App)](#jwt-bearer-web-app)
  - [X-Gateway-Key (Gateway)](#x-gateway-key-gateway)
- [Common Patterns](#common-patterns)
  - [Error Responses](#error-responses)
  - [Pagination](#pagination)
  - [Filtering](#filtering)
- [Health Check](#health-check)
- [Gateway Management](#gateway-management)
- [Device Management](#device-management)
- [Schedule Management](#schedule-management)
- [Automation Rules](#automation-rules)
- [Event Logs](#event-logs)
- [Telemetry](#telemetry)
- [Gateway Sync API (Gateway-facing)](#gateway-sync-api-gateway-facing)
- [MQTT Topic Convention](#mqtt-topic-convention)

---

## Architecture Overview

```
┌──────────────┐     REST API     ┌──────────┐     MQTT      ┌─────────┐
│  Web App     │◄────────────────►│ Backend  │◄─────────────►│ Gateway │
│  (React)     │                    │ (Node.js)│               │ (ESP32) │
└──────────────┘                    └──────────┘               └─────────┘
                                      │   ▲
                                      │   │
                                      ▼   │
                                   ┌──────────┐
                                   │ PostgreSQL│
                                   └──────────┘
```

- **REST API**: Port 3000 (HTTP) — CRUD for gateways, devices, schedules, rules, events, telemetry
- **MQTT Broker**: Port 1883 (TCP) — device command + telemetry relay
- **Database**: PostgreSQL (Aiven)

### API Base URL

| Environment | Base URL |
|---|---|
| Local dev | `http://localhost:3000/api/v1` |
| Production | `https://{backend-host}.onrender.com/api/v1` |

---

## Authentication

### JWT Bearer (Web App)

Most Web App ↔ Backend endpoints require a **JWT Bearer token** in the `Authorization` header.

```
Authorization: Bearer <your-jwt-token>
```

> **Note:** JWT authentication is a *should-have* feature — the current implementation does not yet enforce it on all routes. In production, ensure the Web App obtains a JWT (e.g., via a login flow) and includes it on every request below that lists `BearerAuth`.

### X-Gateway-Key (Gateway)

The Sync endpoints (Gateway-facing) authenticate via an **API key** in a custom header. This is public (no JWT required) and is used by the ESP32 Gateway to pull config and push event logs.

```
X-Gateway-Key: <your-gateway-api-key>
```

The key is configured at provisioning time in the Gateway's NVS and stored as the `GATEWAY_KEY` environment variable on the Backend.

### Endpoints Without Authentication

| Method | Path | Purpose |
|---|---|---|
| GET | `/health` | Server health check (no auth) |
| POST | `/api/v1/gateways` | Register a new gateway (no auth — first-time provisioning) |

---

## Common Patterns

### Error Responses

All errors use the following envelope:

```json
{
  "error": {
    "code": "DEVICE_NOT_FOUND",
    "message": "Device with id 'sensor_99' not found in gateway 'gateway_001'",
    "status": 404
  }
}
```

#### Error Codes

| Code | HTTP Status | Meaning |
|---|---|---|
| `VALIDATION_ERROR` | 400 | Invalid request body or parameters |
| `INVALID_DEVICE_PROTOCOL` | 400 | Device protocol is invalid |
| `GATEWAY_NOT_FOUND` | 404 | Gateway ID not found |
| `DEVICE_NOT_FOUND` | 404 | Device not found |
| `SCHEDULE_NOT_FOUND` | 404 | Schedule not found |
| `RULE_NOT_FOUND` | 404 | Automation rule not found |
| `DEVICE_ID_CONFLICT` | 409 | Device ID already exists for this gateway |
| `DEVICE_IN_USE` | 409 | Device is referenced by an automation rule or schedule |
| `GATEWAY_OFFLINE` | 422 | Gateway is currently offline (command cannot be sent) |
| `INTERNAL_ERROR` | 500 | Unexpected server error |

### Pagination

List endpoints that return arrays support `limit` and `offset` query parameters:

| Parameter | Type | Default | Description |
|---|---|---|---|
| `limit` | integer | 50 | Maximum number of items to return |
| `offset` | integer | 0 | Number of items to skip |

Example:

```
GET /api/v1/gateways/gateway_001/events?limit=100&offset=200
```

### Filtering

List endpoints may support additional filters. See the specific endpoint section for details.

---

## Health Check

### GET `/health`

Returns the server's health status and current timestamp. No authentication required.

**Request:**

```bash
curl http://localhost:3000/health
```

**Response — 200 OK**

```json
{
  "status": "ok",
  "timestamp": "2026-09-02T10:30:00.000Z"
}
```

---

## Gateway Management

### GET `/api/v1/gateways`

List all gateways. Requires JWT bearer token.

**Response — 200 OK**

Returns an array of [Gateway objects](#gateway-object).

```json
[
  {
    "id": "gateway_001",
    "name": "Đồng hồ phòng khách",
    "user_id": 1,
    "status": "online",
    "last_seen": "2026-09-02T10:25:00Z",
    "firmware_version": "1.0.0",
    "timezone": "Asia/Ho_Chi_Minh",
    "created_at": "2026-08-14T00:00:00Z"
  }
]
```

### POST `/api/v1/gateways`

Register a new gateway. No authentication required — used during initial provisioning.

**Request Body:**

```json
{
  "id": "gateway_001",
  "name": "Đồng hồ phòng khách",
  "timezone": "Asia/Ho_Chi_Minh",
  "user_id": 1
}
```

| Field | Type | Required | Constraints | Description |
|---|---|---|---|---|
| `id` | string | Yes | 1–32 chars, `^[a-zA-Z0-9_-]+$` | Unique gateway identifier |
| `name` | string | Yes | 1–64 chars | Human-readable name |
| `timezone` | string | No | — | IANA timezone (e.g., `Asia/Ho_Chi_Minh`) |
| `user_id` | integer | No | — | Owner user ID |

**Response — 201 Created**

```json
{
  "id": "gateway_001",
  "name": "Đồng hồ phòng khách",
  "message": "Gateway registered successfully"
}
```

### GET `/api/v1/gateways/{gateway_id}`

Get a single gateway's info. Requires JWT bearer token.

**Response — 200 OK**

Returns a [Gateway object](#gateway-object).

### PUT `/api/v1/gateways/{gateway_id}`

Update a gateway's info. Requires JWT bearer token.

**Request Body:**

```json
{
  "name": "Đồng hồ phòng khách (updated)",
  "timezone": "Asia/Ho_Chi_Minh",
  "firmware_version": "1.0.1"
}
```

| Field | Type | Required | Description |
|---|---|---|---|
| `name` | string | No | New gateway name |
| `timezone` | string | No | New timezone |
| `firmware_version` | string | No | New firmware version |

**Response — 200 OK**

Returns the updated [Gateway object](#gateway-object).

### Gateway Object

```json
{
  "id": "gateway_001",
  "name": "Đồng hồ phòng khách",
  "user_id": 1,
  "status": "online",
  "last_seen": "2026-09-02T10:25:00Z",
  "firmware_version": "1.0.0",
  "timezone": "Asia/Ho_Chi_Minh",
  "created_at": "2026-08-14T00:00:00Z"
}
```

| Field | Type | Description |
|---|---|---|
| `id` | string | Unique gateway identifier |
| `name` | string | Human-readable name |
| `user_id` | integer \| null | Owner user ID |
| `status` | `"online"` \| `"offline"` | Updated via MQTT LWT |
| `last_seen` | string (date-time) | Last time the gateway was seen online |
| `firmware_version` | string | Currently running firmware version |
| `timezone` | string | IANA timezone string |
| `created_at` | string (date-time) | Registration timestamp |

---

## Device Management

All device endpoints are scoped to a gateway: `/api/v1/gateways/{gateway_id}/devices...`

### GET `/api/v1/gateways/{gateway_id}/devices`

List all devices registered to a gateway. Requires JWT bearer token.

**Response — 200 OK**

Returns an array of [Device objects](#device-object).

```json
[
  {
    "id": 1,
    "device_id": "sensor_01",
    "gateway_id": "gateway_001",
    "name": "Cảm biến nhiệt phòng ngủ",
    "device_type": "sensor",
    "protocol": "ble",
    "mac_address": "AA:BB:CC:DD:EE:FF",
    "mqtt_topic": null,
    "capabilities": "[\"temperature\", \"humidity\"]",
    "status": "online",
    "last_seen": "2026-09-02T10:25:00Z",
    "last_value": 24.5,
    "created_at": "2026-08-14T00:00:00Z"
  }
]
```

### POST `/api/v1/gateways/{gateway_id}/devices`

Add a new device to a gateway. Requires JWT bearer token.

**Request Body:**

```json
{
  "device_id": "fan_01",
  "name": "Quạt làm mát",
  "device_type": "actuator",
  "protocol": "mqtt",
  "mqtt_topic": "home/living/fan"
}
```

| Field | Type | Required | Constraints | Description |
|---|---|---|---|---|
| `device_id` | string | Yes | max 32 chars | Unique device identifier within the gateway |
| `name` | string | Yes | max 64 chars | Human-readable device name |
| `device_type` | string | Yes | `sensor` \| `actuator` | Device type |
| `protocol` | string | Yes | `ble` \| `mqtt` | Communication protocol |
| `mac_address` | string | No | pattern `^(?:[0-9A-F]{2}:){5}[0-9A-F]{2}$` | MAC address (for BLE devices) |
| `mqtt_topic` | string | No | max 128 chars | MQTT topic (for MQTT devices) |
| `capabilities` | string | No | JSON string | Device capabilities, e.g. `["temperature", "humidity"]` |

**Response — 201 Created**

```json
{
  "id": 2,
  "device_id": "fan_01",
  "message": "Device registered successfully"
}
```

**Response — 409 Conflict** (if `device_id` already exists)

```json
{
  "error": {
    "code": "DEVICE_ID_CONFLICT",
    "message": "Device with id 'fan_01' already exists in gateway 'gateway_001'",
    "status": 409
  }
}
```

### PUT `/api/v1/gateways/{gateway_id}/devices/{device_id}`

Update an existing device. Requires JWT bearer token.

**Request Body:** Same schema as [POST device](#post-api-v1gatewaysgateway_iddevices).

**Response — 200 OK** — Returns the updated [Device object](#device-object).

### DELETE `/api/v1/gateways/{gateway_id}/devices/{device_id}`

Delete a device. Requires JWT bearer token.

> **Note:** Deleting a device also cascades to delete any automation rules that reference it.

**Response — 204 No Content** — No response body.

**Possible errors:**
- `409 Conflict` — Device is still in use by an automation rule (non-cascade case).

### POST `/api/v1/gateways/{gateway_id}/devices/{device_id}/command`

Send a command to a specific device. The Backend publishes the command to the MQTT broker, which the Gateway picks up and relays to the device.

Requires JWT bearer token.

**Request Body:**

```json
{
  "command": "ON"
}
```

| Field | Type | Required | Description |
|---|---|---|---|
| `command` | string | Yes | The command to send (e.g., `"ON"`, `"OFF"`, `"SET:75"`) |

**Response — 202 Accepted**

```json
{
  "message": "Command 'ON' sent to device 'fan_01' via gateway 'gateway_001'"
}
```

**Response — 404 Not Found** (gateway or device not found)

**Response — 422 Unprocessable Entity** (gateway is offline)

```json
{
  "error": {
    "code": "GATEWAY_OFFLINE",
    "message": "Gateway 'gateway_001' is currently offline",
    "status": 422
  }
}
```

### Device Object

```json
{
  "id": 1,
  "device_id": "sensor_01",
  "gateway_id": "gateway_001",
  "name": "Cảm biến nhiệt phòng ngủ",
  "device_type": "sensor",
  "protocol": "ble",
  "mac_address": "AA:BB:CC:DD:EE:FF",
  "mqtt_topic": null,
  "capabilities": "[\"temperature\", \"humidity\"]",
  "status": "online",
  "last_seen": "2026-09-02T10:25:00Z",
  "last_value": 24.5,
  "created_at": "2026-08-14T00:00:00Z"
}
```

| Field | Type | Description |
|---|---|---|
| `id` | integer | Database ID |
| `device_id` | string | Unique device identifier within gateway |
| `gateway_id` | string | Parent gateway ID |
| `name` | string | Human-readable name |
| `device_type` | `"sensor"` \| `"actuator"` | Device type |
| `protocol` | `"ble"` \| `"mqtt"` | Communication protocol |
| `mac_address` | string \| null | BLE MAC address |
| `mqtt_topic` | string \| null | MQTT topic |
| `capabilities` | string \| null | JSON string of capabilities |
| `status` | `"online"` \| `"offline"` \| `"unknown"` | Last known status |
| `last_seen` | string (date-time) \| null | Last time device was seen |
| `last_value` | number \| null | Last reported sensor value |
| `created_at` | string (date-time) | Registration timestamp |

---

## Schedule Management

All schedule endpoints are scoped to a gateway: `/api/v1/gateways/{gateway_id}/schedules...`

> **Schedule types:**
> - `automation` — IF condition THEN action (local rule-based)
> - `alarm` — Time-based alarm with optional buzzer
> - `personal_event` — Calendar-style personal event

### GET `/api/v1/gateways/{gateway_id}/schedules`

List all schedules. Supports optional `type` filtering. Requires JWT bearer token.

**Query Parameters:**

| Parameter | Type | Required | Description |
|---|---|---|---|
| `type` | string | No | Filter by type: `automation` \| `alarm` \| `personal_event` |

**Response — 200 OK**

Returns an array of [Schedule objects](#schedule-object).

### POST `/api/v1/gateways/{gateway_id}/schedules`

Create a new schedule. Requires JWT bearer token.

**Request Body:**

```json
{
  "type": "alarm",
  "hour": 7,
  "minute": 0,
  "repeat_days": "MON,TUE,WED,THU,FRI",
  "title": "Dậy đi học",
  "buzzer": true
}
```

For automation type:

```json
{
  "type": "automation",
  "hour": 18,
  "minute": 30,
  "target_device_id": "light_01",
  "action_command": "ON"
}
```

| Field | Type | Required | Description |
|---|---|---|---|
| `type` | string | Yes | `automation` \| `alarm` \| `personal_event` |
| `hour` | integer | Yes | 0–23 |
| `minute` | integer | Yes | 0–59 |
| `repeat_days` | string | No | Comma-separated days: `MON,TUE,WED,THU,FRI`. Mutually exclusive with `one_time_date`. |
| `one_time_date` | string (date) | No | Date for one-time event: `2026-08-20`. Mutually exclusive with `repeat_days`. |
| `enabled` | boolean | No | Default: `true` |
| `target_device_id` | string | No\* | Required for `automation` type |
| `action_command` | string | No\* | Required for `automation` type |
| `title` | string | No | Recommended for `alarm`/`personal_event` |
| `buzzer` | boolean | No | Default: `false` |

**Response — 201 Created**

```json
{
  "id": 101,
  "message": "Schedule created successfully"
}
```

### PUT `/api/v1/gateways/{gateway_id}/schedules/{id}`

Update an existing schedule. Requires JWT bearer token.

**Request Body:** Same schema as [POST schedule](#post-api-v1gatewaysgateway_idschedules).

**Response — 200 OK** — Returns the updated [Schedule object](#schedule-object).

### DELETE `/api/v1/gateways/{gateway_id}/schedules/{id}`

Delete a schedule. Requires JWT bearer token.

**Response — 204 No Content** — No response body.

### PATCH `/api/v1/gateways/{gateway_id}/schedules/{id}/toggle`

Toggle a schedule's enabled/disabled state. Requires JWT bearer token.

**Request Body:**

```json
{
  "enabled": false
}
```

**Response — 200 OK** — Returns the updated [Schedule object](#schedule-object).

### Schedule Object

```json
{
  "id": 101,
  "gateway_id": "gateway_001",
  "type": "alarm",
  "hour": 7,
  "minute": 0,
  "repeat_days": "MON,TUE,WED,THU,FRI",
  "one_time_date": null,
  "enabled": true,
  "target_device_id": null,
  "action_command": null,
  "title": "Dậy đi học",
  "buzzer": true,
  "created_at": "2026-08-14T00:00:00Z",
  "updated_at": "2026-08-14T00:00:00Z"
}
```

| Field | Type | Description |
|---|---|---|
| `id` | integer | Schedule ID |
| `gateway_id` | string | Parent gateway ID |
| `type` | `"automation"` \| `"alarm"` \| `"personal_event"` | Schedule type |
| `hour` | integer | 0–23 |
| `minute` | integer | 0–59 |
| `repeat_days` | string \| null | Comma-separated days |
| `one_time_date` | string \| null | ISO date string for one-time events |
| `enabled` | boolean | Whether the schedule is active |
| `target_device_id` | string \| null | Target device (for automation) |
| `action_command` | string \| null | Command to send (for automation) |
| `title` | string \| null | Human-readable title |
| `buzzer` | boolean | Whether to trigger the buzzer |
| `created_at` | string (date-time) | Creation timestamp |
| `updated_at` | string (date-time) | Last update timestamp |

---

## Automation Rules

All rule endpoints are scoped to a gateway: `/api/v1/gateways/{gateway_id}/rules...`

Automation rules implement the **Rule Engine** pattern: when a sensor's value crosses a threshold, a target device receives a command. The rules are evaluated locally on the Gateway (edge computing), but managed from the Web App via the Backend API.

### GET `/api/v1/gateways/{gateway_id}/rules`

List all automation rules for a gateway. Requires JWT bearer token.

**Response — 200 OK**

Returns an array of [AutomationRule objects](#automation-rule-object).

### POST `/api/v1/gateways/{gateway_id}/rules`

Create a new automation rule. Requires JWT bearer token.

**Request Body:**

```json
{
  "sensor_device_id": "sensor_01",
  "condition_type": "gt",
  "threshold": 30.0,
  "target_device_id": "fan_01",
  "action_command": "ON"
}
```

| Field | Type | Required | Description |
|---|---|---|---|
| `sensor_device_id` | string | Yes | Device ID of the sensor |
| `condition_type` | string | Yes | `gt` (greater than) \| `lt` (less than) \| `eq` (equals) |
| `threshold` | number (float) | Yes | Threshold value |
| `target_device_id` | string | Yes | Device ID of the target actuator |
| `action_command` | string | Yes | Command to send to the target |
| `enabled` | boolean | No | Default: `true` |

**Response — 201 Created**

```json
{
  "id": 1,
  "message": "Automation rule created successfully"
}
```

### PUT `/api/v1/gateways/{gateway_id}/rules/{id}`

Update an existing automation rule. Requires JWT bearer token.

**Request Body:** Same schema as [POST rule](#post-api-v1gatewaysgateway_idrules).

**Response — 200 OK** — Returns the updated [AutomationRule object](#automation-rule-object).

### DELETE `/api/v1/gateways/{gateway_id}/rules/{id}`

Delete an automation rule. Requires JWT bearer token.

**Response — 204 No Content** — No response body.

### Automation Rule Object

```json
{
  "id": 1,
  "gateway_id": "gateway_001",
  "sensor_device_id": "sensor_01",
  "condition_type": "gt",
  "threshold": 30.0,
  "target_device_id": "fan_01",
  "action_command": "ON",
  "enabled": true,
  "created_at": "2026-08-14T00:00:00Z"
}
```

| Field | Type | Description |
|---|---|---|
| `id` | integer | Rule ID |
| `gateway_id` | string | Parent gateway ID |
| `sensor_device_id` | string | Device ID of the sensor |
| `condition_type` | `"gt"` \| `"lt"` \| `"eq"` | Comparison operator |
| `threshold` | number (float) | Threshold value |
| `target_device_id` | string | Device ID of the target actuator |
| `action_command` | string | Command to send to the target |
| `enabled` | boolean | Whether the rule is active |
| `created_at` | string (date-time) | Creation timestamp |

---

## Event Logs

All event endpoints are scoped to a gateway: `/api/v1/gateways/{gateway_id}/events`

### GET `/api/v1/gateways/{gateway_id}/events`

Query event log history for a gateway. Requires JWT bearer token.

**Query Parameters:**

| Parameter | Type | Required | Default | Description |
|---|---|---|---|---|
| `type` | string | No | — | Filter by event type (e.g., `rule_executed`, `device_command`) |
| `from` | string (date-time) | No | — | Filter events from this timestamp |
| `to` | string (date-time) | No | — | Filter events up to this timestamp |
| `limit` | integer | No | 50 | Maximum items to return |
| `offset` | integer | No | 0 | Number of items to skip |

**Response — 200 OK**

Returns an array of [EventLog objects](#event-log-object).

```json
[
  {
    "id": 1,
    "gateway_id": "gateway_001",
    "event_type": "rule_executed",
    "device_id": "fan_01",
    "payload": "{\"command\":\"ON\",\"source\":\"automation\"}",
    "occurred_at": "2026-09-02T10:25:00Z",
    "synced_at": "2026-09-02T10:25:01Z"
  }
]
```

### Event Log Object

| Field | Type | Description |
|---|---|---|
| `id` | integer | Event log ID |
| `gateway_id` | string | Parent gateway ID |
| `event_type` | string | Type of event (e.g., `rule_executed`, `device_command`, `schedule_triggered`) |
| `device_id` | string \| null | Associated device ID |
| `payload` | string \| null | JSON string with event details |
| `occurred_at` | string (date-time) | Timestamp from Gateway RTC |
| `synced_at` | string (date-time) \| null | When the event was synced to Backend |

---

## Telemetry

### GET `/api/v1/gateways/{gateway_id}/devices/{device_id}/telemetry`

Query historical telemetry data for a specific device. Requires JWT bearer token.

**Query Parameters:**

| Parameter | Type | Required | Default | Description |
|---|---|---|---|---|
| `from` | string (date-time) | No | — | Filter telemetry from this timestamp |
| `to` | string (date-time) | No | — | Filter telemetry up to this timestamp |
| `limit` | integer | No | 100 | Maximum items to return |
| `offset` | integer | No | 0 | Number of items to skip |

**Response — 200 OK**

Returns an array of [TelemetryLog objects](#telemetry-log-object).

```json
[
  {
    "id": 1,
    "device_id": "sensor_01",
    "value": 24.5,
    "unit": "°C",
    "recorded_at": "2026-09-02T10:25:00Z"
  }
]
```

### Telemetry Log Object

| Field | Type | Description |
|---|---|---|
| `id` | integer | Telemetry log ID |
| `device_id` | string | Device that reported the telemetry |
| `value` | number (float) | Measured value |
| `unit` | string \| null | Unit of measurement (e.g., `°C`, `%`) |
| `recorded_at` | string (date-time) | Timestamp from Gateway RTC |

---

## Gateway Sync API (Gateway-facing)

These endpoints are called by the ESP32 Gateway to pull configuration and push event logs. They use **`X-Gateway-Key`** authentication (no JWT required).

### GET `/api/v1/gateways/{gateway_id}/sync/schedule`

Pull the full configuration (schedules + automation rules) from the Backend. Called by the Gateway on boot or when it receives a config update notification.

**Headers:**

```
X-Gateway-Key: <your-gateway-api-key>
```

**Response — 200 OK**

```json
{
  "schedules": [ /* array of Schedule objects */ ],
  "rules": [ /* array of AutomationRule objects */ ],
  "last_updated": "2026-09-02T10:20:00Z"
}
```

| Field | Type | Description |
|---|---|---|
| `schedules` | array | Array of [Schedule objects](#schedule-object) |
| `rules` | array | Array of [AutomationRule objects](#automation-rule-object) |
| `last_updated` | string (date-time) | Timestamp of the most recent config change |

**Response — 404 Not Found** (gateway not found or key mismatch)

### GET `/api/v1/gateways/{gateway_id}/sync/devices`

Pull the device list from the Backend. Called by the Gateway on boot or when device list changes.

**Headers:**

```
X-Gateway-Key: <your-gateway-api-key>
```

**Response — 200 OK**

```json
{
  "devices": [ /* array of Device objects */ ],
  "last_updated": "2026-09-02T10:15:00Z"
}
```

| Field | Type | Description |
|---|---|---|
| `devices` | array | Array of [Device objects](#device-object) |
| `last_updated` | string (date-time) | Timestamp of the most recent device list change |

**Response — 404 Not Found** (gateway not found or key mismatch)

### POST `/api/v1/gateways/{gateway_id}/sync/events`

Push event log entries from the Gateway to the Backend. Called periodically by the Gateway to sync its local event log.

**Headers:**

```
X-Gateway-Key: <your-gateway-api-key>
```

**Request Body:**

```json
{
  "events": [
    {
      "type": "rule_executed",
      "ts": 1723621234,
      "device_id": "fan_01",
      "rule_id": 1,
      "target": "fan_01",
      "command": "ON"
    },
    {
      "type": "schedule_triggered",
      "ts": 1723624800,
      "schedule_id": 201
    }
  ]
}
```

| Field | Type | Required | Description |
|---|---|---|---|
| `type` | string | Yes | Event type |
| `ts` | integer | Yes | Unix timestamp from Gateway RTC |
| `device_id` | string | No | Associated device ID |
| `rule_id` | integer | No | Associated rule ID |
| `schedule_id` | integer | No | Associated schedule ID |
| `target` | string | No | Target device ID |
| `command` | string | No | Command sent |

**Response — 200 OK**

```json
{
  "synced_count": 2,
  "message": "Events synced successfully"
}
```

**Response — 404 Not Found** (gateway not found or key mismatch)

---

## MQTT Topic Convention

The Backend runs an embedded MQTT broker (aedes) on port 1883. The Gateway connects as an MQTT client. The topic structure is:

```
gateway/{gateway_id}/status                    ← Gateway status (online/offline, retained, LWT)
gateway/{gateway_id}/device/{device_id}/telemetry   ← Gateway → Backend (device telemetry)
gateway/{gateway_id}/device/{device_id}/command     ← Backend → Gateway (device commands)
```

### Publishing Telemetry (Gateway → Backend)

When a device sends telemetry (e.g., a BLE sensor reading), the Gateway publishes to:

```
gateway/{gateway_id}/device/{device_id}/telemetry
```

**Payload:**

```json
{
  "value": 24.5,
  "unit": "°C"
}
```

### Sending Device Commands (Backend → Gateway)

When the Web App sends a command to a device (via the REST API), the Backend publishes to:

```
gateway/{gateway_id}/device/{device_id}/command
```

**Payload:**

```json
{
  "command": "ON"
}
```

### Gateway Status (LWT)

The Gateway sets a **Last Will and Testament** on its MQTT connection. If the Gateway disconnects unexpectedly, the broker publishes:

```
gateway/{gateway_id}/status
```

**Payload:**

```json
{
  "status": "offline"
}
```

This message is **retained** on the broker, so the Backend always sees the latest status when it subscribes.

---

## Appendix: Quick Reference

### All Endpoints

| Method | Path | Auth | Description |
|---|---|---|---|
| GET | `/health` | None | Server health |
| GET | `/api/v1/gateways` | JWT | List gateways |
| POST | `/api/v1/gateways` | None | Register gateway |
| GET | `/api/v1/gateways/{id}` | JWT | Get gateway |
| PUT | `/api/v1/gateways/{id}` | JWT | Update gateway |
| GET | `/api/v1/gateways/{id}/devices` | JWT | List devices |
| POST | `/api/v1/gateways/{id}/devices` | JWT | Add device |
| PUT | `/api/v1/gateways/{id}/devices/{device_id}` | JWT | Update device |
| DELETE | `/api/v1/gateways/{id}/devices/{device_id}` | JWT | Delete device |
| POST | `/api/v1/gateways/{id}/devices/{device_id}/command` | JWT | Send command |
| GET | `/api/v1/gateways/{id}/schedules` | JWT | List schedules |
| POST | `/api/v1/gateways/{id}/schedules` | JWT | Create schedule |
| PUT | `/api/v1/gateways/{id}/schedules/{id}` | JWT | Update schedule |
| DELETE | `/api/v1/gateways/{id}/schedules/{id}` | JWT | Delete schedule |
| PATCH | `/api/v1/gateways/{id}/schedules/{id}/toggle` | JWT | Toggle schedule |
| GET | `/api/v1/gateways/{id}/rules` | JWT | List rules |
| POST | `/api/v1/gateways/{id}/rules` | JWT | Create rule |
| PUT | `/api/v1/gateways/{id}/rules/{id}` | JWT | Update rule |
| DELETE | `/api/v1/gateways/{id}/rules/{id}` | JWT | Delete rule |
| GET | `/api/v1/gateways/{id}/events` | JWT | List events |
| GET | `/api/v1/gateways/{id}/devices/{device_id}/telemetry` | JWT | Get telemetry |
| GET | `/api/v1/gateways/{id}/sync/schedule` | Gateway Key | Pull config |
| GET | `/api/v1/gateways/{id}/sync/devices` | Gateway Key | Pull devices |
| POST | `/api/v1/gateways/{id}/sync/events` | Gateway Key | Push events |

### HTTP Status Code Summary

| Code | Meaning |
|---|---|
| 200 | OK — successful read or update |
| 201 | Created — resource successfully created |
| 202 | Accepted — command published to MQTT |
| 204 | No Content — resource successfully deleted |
| 400 | Bad Request — validation error |
| 404 | Not Found — resource does not exist |
| 409 | Conflict — resource conflict (duplicate ID, in use) |
| 422 | Unprocessable Entity — semantic error (e.g., gateway offline) |
| 500 | Internal Server Error |
