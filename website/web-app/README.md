# Smart Clock Web App (Frontend)

React + Vite + TailwindCSS frontend for the Smart Clock IoT Gateway system.

## Prerequisites

- **Backend**: The web app requires the backend server running (see `/backend/README.md`)
- **Node.js** v18+ recommended
- **npm** or **yarn**

## Development

```bash
# Install dependencies
npm install

# Local development (proxy to backend at localhost:3000)
npm run dev
# Open http://localhost:5173

# Build for production
npm run build

# Preview production build
npm run preview
```

### Environment Variables

Create a `.env.local` file for local development:

```env
# Backend API URL (leave empty to use default Vite proxy)
VITE_API_URL=

# For production deployment, set this to your Render backend URL:
# VITE_API_URL=https://smartclock-backend.onrender.com
```

### Proxy Configuration

During development (`npm run dev`), Vite proxies:
- `/api/*` → `http://localhost:3000/api/*`
- `/health` → `http://localhost:3000/health`

This lets you run the web app and backend independently on `localhost:5173` and `localhost:3000` respectively.

## Deployment (GitHub Pages)

### Option A: Vite build with gh-pages

```bash
npm run build

# Deploy to GitHub Pages
# Option 1: Using gh-pages npm package
npm install -D gh-pages
# Add to package.json scripts: "deploy": "gh-pages -d dist"
npm run deploy

# Option 2: Manual — push dist/ to gh-pages branch
git checkout --orphan gh-pages
git --work-tree=dist add --all
git --work-tree=dist commit -m "Deploy"
git push origin gh-pages --force
```

### Option B: GitHub Actions (recommended)

Create `.github/workflows/deploy.yml`:

```yaml
name: Deploy to GitHub Pages
on:
  push:
    branches: [main]

jobs:
  build-and-deploy:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-node@v4
        with: { node-version: 18 }
      - run: npm install
      - run: npm run build
      - uses: peaceiris/actions-gh-pages@v3
        with:
          github_token: ${{ secrets.GITHUB_TOKEN }}
          publish_dir: ./dist
```

### Production Config

Set the backend API URL in your production environment:

```env
# .env.production or GitHub Actions secrets
VITE_API_URL=https://your-backend.onrender.com
```

## Features

- **Dashboard**: Gateway status, device summary cards, recent events
- **Devices**: Full CRUD + inline device command sending via MQTT relay
- **Schedules**: Alarm, automation, and personal event scheduling with repeat patterns
- **Rules**: IF-THEN automation rule management (sensor condition → actuator command)
- **Events**: Filterable event timeline (by type, time range) with pagination
- **Settings**: Gateway configuration (JWT authentication coming soon)

## Architecture

```
web-app/
├── src/
│   ├── api/
│   │   └── client.js     # Axios instance + all API methods
│   ├── components/
│   │   ├── Layout.jsx          # Sidebar nav + gateway selector
│   │   ├── StatusBadge.jsx     # Online/offline indicators
│   │   ├── ConfirmDialog.jsx   # Delete confirmations
│   │   └── ErrorBoundary.jsx   # React error boundary
│   ├── pages/
│   │   ├── Dashboard.jsx      # Overview + summary
│   │   ├── Devices.jsx        # Device CRUD + commands
│   │   ├── Schedules.jsx      # Schedule CRUD + toggle
│   │   ├── Rules.jsx          # Automation rule CRUD
│   │   ├── Events.jsx         # Filterable event timeline
│   │   └── Settings.jsx       # Gateway config
│   ├── App.jsx               # Routes
│   └── main.jsx              # Entry point
├── index.html
├── vite.config.js             # Proxy config
├── tailwind.config.js
└── postcss.config.js
```

## Notes

- **Authentication**: The Web App endpoints are designed for JWT bearer auth in production. The backend currently runs without auth enforcement (is should-have per FSD — you can add JWT middleware when ready).
- **MQTT**: Device commands sent via REST (`POST /devices/{id}/command`) are relayed to the gateway via MQTT by the backend. The web app doesn't connect to MQTT directly.
- **Gateway Selection**: The sidebar features a gateway selector. The selected gateway ID is stored in `localStorage` (`activeGateway`).
