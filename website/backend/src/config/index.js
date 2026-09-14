/**
 * Central configuration for the backend.
 */
require('dotenv').config();

module.exports = {
  port: process.env.PORT || 3000,
  mqttPort: process.env.MQTT_PORT || 1883,
  databaseUrl: process.env.DATABASE_URL || 'postgresql://postgres:postgres@localhost:5432/smartclock',
  jwtSecret: process.env.JWT_SECRET || 'dev-secret-change-in-production',
  gatewayKey: process.env.GATEWAY_KEY || 'dev-gateway-key-change-me',
  nodeEnv: process.env.NODE_ENV || 'development'
};
