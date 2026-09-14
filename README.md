# Smart Clock Gateway System

IoT device management platform for ESP32-based smart clocks with BLE device hubs,
MQTT communication, and web-based configuration.

## System Overview

```
┌──────────────┐   REST API    ┌──────────┐  MQTT   ┌──────────┐
│   Web App    │◄─────────────►│ Backend  │◄───────►│ Gateway  │
│  (React/Vite)│                │(Node.js)│        │ (ESP32)  │
└──────────────┘               └──────────┘        └──────────┘
                                      │
                                      ▼
                               ┌──────────────┐
                               │ Aiven Postgres│
                               └──────────────┘
```

- **Web App**: React + Vite + TailwindCSS — deployed on GitHub Pages
- **Backend**: Express + Prisma + embedded MQTT broker — deployed on Render
- **Database**: PostgreSQL (free tier) — hosted on Aiven
- **Gateway**: ESP32 firmware (FSD documented separately)

## Project Structure

```
smart-clock-gateway/
├── docs/
│   └── api/
│       └── SmartClockAPI.yaml       # OpenAPI 3.0 spec
├── website/
│   ├── backend/                     # Node.js Express API + MQTT broker
│   │   ├── src/
│   │   │   ├── app.js               # Express server
│   │   │   ├── config/              # Env/config
│   │   │   ├── controllers/         # Business logic (7 controllers)
│   │   │   ├── routes/              # API routes (7 route files)
│   │   │   ├── middleware/          # Error handler, validation
│   │   │   └── mqtt/                # aedes broker + relay
│   │   ├── prisma/
│   │   │   └── schema.prisma        # DB models (7 models + 6 enums)
│   │   ├── Dockerfile               # Render deployment
│   │   ├── docker-compose.yml       # Local dev
│   │   └── README.md
│   └── web-app/                     # React frontend
│       ├── src/
│       │   ├── api/                 # Axios client + API methods
│       │   ├── components/          # Shared UI (4 components)
│       │   └── pages/               # 6 pages (Dashboard, Devices, Schedules, Rules, Events, Settings)
│       ├── vite.config.js
│       ├── tailwind.config.js
│       └── README.md
├── fsd-smart-clock-gateway.md       # Functional System Design
├── PROJECT_SUMMARY.md               # Summary reference documentation
├── testing/                         # Test plan + test code
└── tests/
```

## Quick Start (Local Development)

### 1. Backend

```bash
cd website/backend
npm install
cp .env.example .env
# Edit .env with your Aiven PostgreSQL connection string
# Or use docker-compose for local Postgres:
docker-compose up --build
```

The backend starts:
- REST API → `http://localhost:3000/api/v1`
- MQTT broker → `mqtt://localhost:1883`
- Health check → `http://localhost:3000/health`

### 2. Web App

```bash
cd website/web-app
npm install
npm run dev
# Open http://localhost:5173
```

The Vite dev server proxies API requests to `localhost:3000`.

## Deployment

### 1. Database (Aiven — free tier)
Create a PostgreSQL service on [Aiven](https://aiven.io). Copy the connection string.

### 2. Backend (Render — free tier)
Push the `website/backend/` directory to a Render web service:
- Build: `npm install && npx prisma generate && npx prisma db push`
- Start: `npm start`
- Env vars: `DATABASE_URL`, `JWT_SECRET`, `GATEWAY_KEY`

### 3. Web App (GitHub Pages)
Build and deploy to GitHub Pages:
```bash
cd website/web-app
npm install && npm run build
# Deploy dist/ to GitHub Pages
```

Set `VITE_API_URL` to your Render backend URL in production.

## API Documentation

The OpenAPI 3.0 spec is at [`docs/api/SmartClockAPI.yaml`](docs/api/SmartClockAPI.yaml).

Key endpoints:

| Resource | Endpoints | Description |
|----------|-----------|-------------|
| Gateways | `GET/POST/PUT /gateways[/:id]` | Register & manage gateways |
| Devices | CRUD `/gateways/:id/devices[/:device_id]` | Manage devices + send commands |
| Schedules | CRUD `/gateways/:id/schedules[/:id]` | Alarms, automation, personal events |
| Rules | CRUD `/gateways/:id/rules[/:id]` | IF-THEN automation rules |
| Sync | `GET/POST /gateways/:id/sync/*` | Gateway-facing config endpoints |
| Events | `GET /gateways/:id/events` | Queryable event log |
| Telemetry | `GET /gateways/:id/devices/:device_id/telemetry` | Historical telemetry |

## Testing

```bash
# Backend tests
cd backend
npm test

# Web app lint
cd web-app
npm run lint
```

## License

MIT — see [CLAUDE.md](CLAUDE.md) for the AICLP methodology.
