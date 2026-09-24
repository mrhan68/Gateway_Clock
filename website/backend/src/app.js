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
app.use(cors({
  origin: true, // Allow all origins in dev; configure for prod
  credentials: true
}));
app.use(express.json({ limit: '10mb' }));
app.use(express.urlencoded({ extended: true, limit: '10mb' }));

// Logging (dev only)
if (config.nodeEnv === 'development') {
  app.use(require('morgan')('dev'));
}

// ─── Health check ───
app.get('/health', (req, res) => {
  res.json({ status: 'ok', timestamp: new Date().toISOString() });
});

// ─── API Routes ───
// Web App routes (require JWT in production)
app.use('/api/v1/gateways', gatewayRoutes);
app.use('/api/v1/gateways', deviceRoutes);
app.use('/api/v1/gateways', scheduleRoutes);
app.use('/api/v1/gateways', ruleRoutes);
app.use('/api/v1/gateways', eventRoutes);
app.use('/api/v1/gateways', telemetryRoutes);

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
