/**
 * Smart Clock Gateway Backend — Express Application
 *
 * This server provides:
 * 1. REST API for the Web App (manages gateways, devices, schedules, rules, events)
 * 2. Sync API for the ESP32 Gateway (pull config, push events)
 * 3. Embedded MQTT broker (aedes) for real-time device communication
 *
 * Deployment: Render (Docker)
 * Database: Aiven PostgreSQL
 */
const express = require('express');
const cookieParser = require('cookie-parser');
const { authenticate } = require('./middleware/auth');
const cors = require('cors');
const http = require('http');
const config = require('./config');
const { errorHandler } = require('./middleware/errorHandler');
const mqttBroker = require('./mqtt/broker');

// ─── Routes ───
const gatewayRoutes = require('./routes/gateways');
const deviceRoutes = require('./routes/devices');
const scheduleRoutes = require('./routes/schedules');
const ruleRoutes = require('./routes/rules');
const eventRoutes = require('./routes/events');
const telemetryRoutes = require('./routes/telemetry');

const app = express();

// ─── Middleware ───
const allowedOriginEnv = process.env.ALLOWED_ORIGIN;
let corsOrigin = true; // Mặc định: phản chiếu request origin (hỗ trợ GitHub Pages https://mrhan68.github.io và Localhost)

if (allowedOriginEnv && allowedOriginEnv !== '*') {
  const allowedList = allowedOriginEnv.split(',').map(s => s.trim());
  corsOrigin = (origin, callback) => {
    if (!origin || allowedList.includes(origin) || origin.endsWith('.github.io') || /^https?:\/\/(localhost|127\.0\.0\.1)(:\d+)?$/.test(origin)) {
      callback(null, true);
    } else {
      callback(null, false);
    }
  };
}

app.use(cors({
  origin: corsOrigin,
  credentials: true
}));
app.use(express.json({ limit: '10mb' }));
app.use(express.urlencoded({ extended: true, limit: '10mb' }));
app.use(cookieParser());

// Auth routes
app.use('/auth', require('./routes/auth'));

// Logging (dev only)
if (config.nodeEnv === 'development') {
  app.use(require('morgan')('dev'));
}

// ─── Health & Root check ───
app.get('/', (req, res) => {
  res.json({
    service: 'Smart Clock Gateway Backend API',
    status: 'online',
    version: '1.6.0',
    health: '/health',
    gateways: '/api/v1/gateways',
    timestamp: new Date().toISOString()
  });
});

app.get('/health', (req, res) => {
  res.json({ status: 'ok', timestamp: new Date().toISOString() });
});

// ─── API Routes ───
// Web App routes (require JWT in production)
app.use('/api/v1/gateways', authenticate, gatewayRoutes);
app.use('/api/v1/gateways', authenticate, deviceRoutes);
app.use('/api/v1/gateways', authenticate, scheduleRoutes);
app.use('/api/v1/gateways', authenticate, ruleRoutes);
app.use('/api/v1/gateways', authenticate, eventRoutes);
app.use('/api/v1/gateways', authenticate, telemetryRoutes);

// ─── Error handler (must be last) ───
app.use(errorHandler);

// ─── Start server ───
async function startServer() {
  try {
    // Verify DB connection
    const { PrismaClient } = require('@prisma/client');
    const prisma = new PrismaClient();
    await prisma.$connect();
    console.log('[DB] Connected to PostgreSQL');

    // Start HTTP server
    const server = http.createServer(app);
    server.listen(config.port, () => {
      console.log(`[HTTP] REST API listening on port ${config.port}`);
      console.log(`[HTTP] Health check: http://localhost:${config.port}/health`);
    });

    // Start MQTT broker
    await mqttBroker.startBroker(config.mqttPort);

    // Graceful shutdown
    process.on('SIGTERM', async () => {
      console.log('[SERVER] SIGTERM received, shutting down...');
      await mqttBroker.stopBroker();
      await prisma.$disconnect();
      server.close(() => process.exit(0));
    });

    process.on('SIGINT', async () => {
      console.log('[SERVER] SIGINT received, shutting down...');
      await mqttBroker.stopBroker();
      await prisma.$disconnect();
      server.close(() => process.exit(0));
    });

  } catch (err) {
    console.error('[STARTUP] Failed to start server:', err);
    process.exit(1);
  }
}

// Only start if run directly (not in tests)
if (require.main === module) {
  startServer();
}

module.exports = app;
