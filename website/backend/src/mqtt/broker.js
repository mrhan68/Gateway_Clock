/**
 * MQTT Broker (aedes) — embedded in-process.
 *
 * Runs alongside the Express HTTP server on a separate port.
 * Handles:
 *  - Gateway LWT (online/offline status tracking)
 *  - Gateway → Backend telemetry ingestion (stored in DB)
 *  - Backend → Gateway device commands (published from REST calls)
 */
const { Aedes } = require('aedes');
const net = require('net');
const { PrismaClient } = require('@prisma/client');
const prisma = new PrismaClient();

const aedes = new Aedes();
let server;
let isRunning = false;

/**
 * Start the MQTT broker on the given port.
 * @param {number} port - TCP port for MQTT (typically 1883)
 * @returns {Promise<void>}
 */
async function startBroker(port = 1883) {
  if (isRunning) return;

  // aedes.listen() sets up internal state (persistence, heartbeat, $SYS topics)
  await aedes.listen();

  server = net.createServer((socket) => {
    aedes.handle(socket);
  });
  server.listen(port, () => {
    isRunning = true;
    console.log(`[MQTT] Broker listening on port ${port}`);
  });

  // ─── Connection events ───
  aedes.on('clientReady', (client) => {
    console.log(`[MQTT] Client connected: ${client.id}`);
  });

  aedes.on('clientDisconnect', (client) => {
    console.log(`[MQTT] Client disconnected: ${client.id}`);
  });

  aedes.on('subscribe', (subscriptions, client) => {
    console.log(`[MQTT] Client ${client.id} subscribed to: ${subscriptions.map(s => s.topic).join(', ')}`);
  });

  aedes.on('unsubscribe', (subscriptions, client) => {
    console.log(`[MQTT] Client ${client.id} unsubscribed from: ${subscriptions.join(', ')}`);
  });

  aedes.on('publish', async (packet) => {
    const topic = packet.topic;
    const payload = packet.payload.toString();

    // Parse topic structure: gateway/{id}/...
    const parts = topic.split('/');
    if (parts[0] !== 'gateway') return;

    const gatewayId = parts[1];

    // ─── Telemetry from gateway → device ───
    if (parts[2] === 'device' && parts[4] === 'telemetry') {
      const deviceId = parts[3];
      await handleTelemetry(gatewayId, deviceId, payload);
      return;
    }

    // ─── Gateway status (LWT / heartbeat) ───
    if (parts[2] === 'status' && parts.length === 3) {
      await handleGatewayStatus(gatewayId, payload);
      return;
    }

    // ─── Device commands (from gateway to devices) ───
    if (parts[2] === 'device' && parts[4] === 'command') {
      // Commands are received from Web App via REST → this broker → gateway
      // We don't need to do anything here (already handled by relay)
      // This is just for logging/debugging
      const deviceId = parts[3];
      console.log(`[MQTT] Command for ${deviceId} on gateway ${gatewayId}: ${payload}`);
      return;
    }
  });

  // Handle broker errors gracefully
  aedes.on('connection_error', (client, err) => {
    console.error(`[MQTT] Connection error for client ${client?.id}:`, err.message);
  });
}

/**
 * Handle telemetry published by gateway.
 * Topic: gateway/{id}/device/{deviceId}/telemetry
 */
async function handleTelemetry(gatewayId, deviceId, payload) {
  try {
    const data = JSON.parse(payload);
    console.log(`[MQTT] Telemetry: gateway=${gatewayId}, device=${deviceId}, value=${data.value}`);

    // Update device last seen + last value
    await prisma.device.updateMany({
      where: { gatewayId, deviceId },
      data: {
        lastSeen: new Date(),
        lastValue: data.value,
        status: 'online'
      }
    });

    // Store telemetry log entry
    if (data.value !== undefined && data.valid !== false) {
      await prisma.telemetryLog.create({
        data: {
          deviceId: deviceId,
          gatewayId: gatewayId,
          value: data.value,
          unit: data.unit || null,
          recordedAt: data.timestamp
            ? new Date(data.timestamp * 1000)
            : new Date()
        }
      });
    }
  } catch (err) {
    console.error('[MQTT] Failed to process telemetry:', err.message);
  }
}

/**
 * Handle gateway status updates (from LWT or heartbeat).
 * Topic: gateway/{id}/status
 */
async function handleGatewayStatus(gatewayId, payload) {
  try {
    const status = payload.trim() === 'online' ? 'online' : 'offline';
    const now = new Date();

    await prisma.gateway.update({
      where: { id: gatewayId },
      data: {
        status: status,
        lastSeen: status === 'online' ? now : undefined
      }
    });

    console.log(`[MQTT] Gateway ${gatewayId} status: ${status}`);
  } catch (err) {
    console.error('[MQTT] Failed to update gateway status:', err.message);
  }
}

/**
 * Publish a device command from REST API to the MQTT broker.
 * This is called by the relay module when Web App sends a command via REST.
 *
 * @param {string} gatewayId - Gateway ID
 * @param {string} deviceId - Target device ID
 * @param {string} command - Command string (e.g. "ON", "OFF")
 * @returns {Promise<boolean>} - true if published successfully
 */
function publishDeviceCommand(gatewayId, deviceId, command) {
  const topic = `gateway/${gatewayId}/device/${deviceId}/command`;
  const payload = JSON.stringify({ command, source: 'web_app' });

  const packet = {
    topic,
    payload,
    qos: 1,
    retain: false
  };

  return new Promise((resolve) => {
    aedes.publish(packet, (err) => {
      if (err) {
        console.error(`[MQTT] Failed to publish command to ${topic}:`, err.message);
        resolve(false);
      } else {
        console.log(`[MQTT] Published command to ${topic}: ${payload}`);
        resolve(true);
      }
    });
  });
}

/**
 * Check if a gateway is currently online.
 * @param {string} gatewayId
 * @returns {Promise<boolean>}
 */
async function isGatewayOnline(gatewayId) {
  const gw = await prisma.gateway.findUnique({
    where: { id: gatewayId },
    select: { status: true }
  });
  return gw ? gw.status === 'online' : false;
}

/**
 * Gracefully shut down the broker.
 */
async function stopBroker() {
  if (!isRunning) return;
  if (server) server.close();
  await aedes.close();
  isRunning = false;
  console.log('[MQTT] Broker stopped');
}

module.exports = {
  startBroker,
  stopBroker,
  publishDeviceCommand,
  isGatewayOnline,
  aedes
};
