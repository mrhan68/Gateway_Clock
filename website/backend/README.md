# Smart Clock Gateway Backend

Express-based backend API + embedded MQTT broker for the Smart Clock IoT Gateway system.

## Architecture

```
┌──────────────┐     REST API     ┌──────────┐     MQTT      ┌─────────┐
│  Web App     │◄────────────────►│ Backend  │◄─────────────►│ Gateway │
│  (React)     │                    │ (Node.js)│               │ (ESP32) │
└──────────────┘                    └──────────┘               └─────────┘
                                      │   ▲
                                      │   │
                                      ▼   │
                                   ┌──────────┐
                                   │ Aiven    │
                                   │ PostgreSQL│
                                   └──────────┘
```

- **REST API**: Port 3000 (HTTP) — CRUD for gateways, devices, schedules, rules, events, telemetry
- **MQTT Broker**: Port 1883 (TCP) — device command + telemetry relay
- **Database**: PostgreSQL on Aiven

## Getting Started

### Local development

```bash
# Install dependencies
npm install

# Copy env and edit
cp .env.example .env.local
# Edit .env.local with your PostgreSQL credentials

# Run migrations (creates tables)
npx prisma db push

# Start development server (includes hot reload)
npm run dev
# or: npx nodemon src/app.js
```

The server listens on:
- HTTP REST API: `http://localhost:3000/api/v1`
- MQTT broker: `mqtt://localhost:1883`
- Health check: `http://localhost:3000/health`

### Docker development

```bash
cp .env.example .env
npm run dev
# or use docker-compose:
docker-compose up --build
```

## API Endpoints

### Gateway Management
| Method | Path | Description |
|--------|------|-------------|
| GET | `/api/v1/gateways` | List all gateways |
| POST | `/api/v1/gateways` | Register a new gateway |
| GET | `/api/v1/gateways/{id}` | Get gateway info |
| PUT | `/api/v1/gateways/{id}` | Update gateway |

### Device Management
| Method | Path | Description |
|--------|------|-------------|
| GET | `/api/v1/gateways/{id}/devices` | List devices |
| POST | `/api/v1/gateways/{id}/devices` | Add device |
| PUT | `/api/v1/gateways/{id}/devices/{device_id}` | Update device |
| DELETE | `/api/v1/gateways/{id}/devices/{device_id}` | Delete device |
| POST | `/api/v1/gateways/{id}/devices/{device_id}/command` | Send device command |

### Schedules
| Method | Path | Description |
|--------|------|-------------|
| GET | `/api/v1/gateways/{id}/schedules` | List schedules (filter: `?type=automation`) |
| POST | `/api/v1/gateways/{id}/schedules` | Create schedule |
| PUT | `/api/v1/gateways/{id}/schedules/{id}` | Update schedule |
| DELETE | `/api/v1/gateways/{id}/schedules/{id}` | Delete schedule |
| PATCH | `/api/v1/gateways/{id}/schedules/{id}/toggle` | Toggle enable/disable |

### Automation Rules
| Method | Path | Description |
|--------|------|-------------|
| GET | `/api/v1/gateways/{id}/rules` | List rules |
| POST | `/api/v1/gateways/{id}/rules` | Create rule |
| PUT | `/api/v1/gateways/{id}/rules/{id}` | Update rule |
| DELETE | `/api/v1/gateways/{id}/rules/{id}` | Delete rule |

### Sync (Gateway-facing, public)
| Method | Path | Description |
|--------|------|-------------|
| GET | `/api/v1/gateways/{id}/sync/schedule` | Pull schedule + rules |
| GET | `/api/v1/gateways/{id}/sync/devices` | Pull device list |
| POST | `/api/v1/gateways/{id}/sync/events` | Push event log |

### Events
| Method | Path | Description |
|--------|------|-------------|
| GET | `/api/v1/gateways/{id}/events` | List events (filter: `?type=...&from=...&to=...&limit=...&offset=...`) |

### Telemetry
| Method | Path | Description |
|--------|------|-------------|
| GET | `/api/v1/gateways/{id}/devices/{device_id}/telemetry` | Historical telemetry |

## MQTT Topic Convention

```
gateway/{gateway_id}/status                    ← Gateway status (online/offline, retained, LWT)
gateway/{gateway_id}/device/{device_id}/telemetry   ← Gateway → Backend (device telemetry)
gateway/{gateway_id}/device/{device_id}/command     ← Backend → Gateway (device commands)
```

## Deployment (Render)

### Deploy backend

1. Create a PostgreSQL database on [Aiven](https://aiven.io) (free tier)
2. Copy the connection string from Aiven
3. Create a new web service on [Render](https://render.com)
4. Set these environment variables in Render:
   - `DATABASE_URL` — from Aiven
   - `JWT_SECRET` — any random string
   - `PORT` — `3000`
5. Set the build command: `npm install && npx prisma generate && npx prisma db push`
6. Set the start command: `npm start`

### Deploy Mosquitto separately (optional)

If you prefer an external Mosquitto instead of the in-process broker:
1. Use Render's "Private Service" with `eclipse-mosquitto` Docker image
2. Set `MQTT_PORT` env var to the private service port
3. Configure the backend to connect to the external broker instead of `startBroker()`

## Error Response Format

All errors return:
```json
{
  "error": {
    "code": "DEVICE_NOT_FOUND",
    "message": "Device with id 'sensor_99' not found in gateway 'gateway_001'",
    "status": 404
  }
}
```

## Testing

```bash
# Unit + integration tests
npm test

# Run in watch mode
npm run test:watch
```

## Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `PORT` | `3000` | HTTP REST API port |
| `MQTT_PORT` | `1883` | MQTT broker port |
| `DATABASE_URL` | — | PostgreSQL connection string (Aiven) |
| `JWT_SECRET` | — | Secret for JWT signing (web app auth) |
| `GATEWAY_KEY` | — | API key for gateway authentication (sync endpoints) |
| `NODE_ENV` | `development` | Node environment |