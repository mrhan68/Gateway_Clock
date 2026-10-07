const mqtt = require('mqtt');
const { PrismaClient } = require('@prisma/client');
const crypto = require('crypto');

const prisma = new PrismaClient();
let mqttClient = null;
let isRunning = false;

async function startBroker() {
  if (isRunning) return;
  const brokerUri = process.env.MQTT_URI || 'mqtts://your-emqx-broker.emqxsl.com:8883';
  const username = process.env.MQTT_USER || 'gw_client';
  const password = process.env.MQTT_PASS || 'your_mqtt_password';
  
  console.log(`[MQTT] Connecting to external broker: ${brokerUri}`);
  
  mqttClient = mqtt.connect(brokerUri, {
    username,
    password,
    clientId: 'backend_' + crypto.randomBytes(4).toString('hex')
  });

  mqttClient.on('connect', () => {
    console.log('[MQTT] Connected to external broker');
    isRunning = true;
    mqttClient.subscribe('gateway/+/telemetry', { qos: 1 });
    mqttClient.subscribe('gateway/+/device/+/command_ack', { qos: 1 });
    mqttClient.subscribe('gateway/+/telemetry/batch', { qos: 1 });
    mqttClient.subscribe('gateway/+/events', { qos: 1 });
    mqttClient.subscribe('gateway/+/status', { qos: 1 });
  });

  mqttClient.on('message', (topic, message) => {
    const payload = message.toString();
    const parts = topic.split('/');
    if (parts.length >= 3 && parts[0] === 'gateway') {
      const gatewayId = parts[1];
      const action = parts[2];
      
      if (action === 'telemetry') {
        if (parts.length > 3 && parts[3] === 'batch') handleTelemetryBatch(gatewayId, payload);
        else handleTelemetry(gatewayId, payload);
      } else if (action === 'events') {
        handleEvents(gatewayId, payload);
      } else if (action === 'status') {
        handleGatewayStatus(gatewayId, payload);
      } else if (action === 'device' && parts[4] === 'command_ack') {
        const deviceId = parts[3];
        handleCommandAck(gatewayId, deviceId, payload);
      }
    }
  });

  mqttClient.on('error', (err) => {
    console.error('[MQTT] Connection error:', err.message);
  });
}

async function handleTelemetry(gatewayId, payload) {
  try {
    const data = JSON.parse(payload);
    const uptime = data.uptime_s || 0;
    const ip = data.ip || '';
    const now = new Date();
    await prisma.gateway.updateMany({
      where: { id: gatewayId },
      data: {
        lastSeen: now,
        status: 'online',
        uptimeSeconds: uptime,
        ipAddress: ip
      }
    });
  } catch (err) {
    console.error('[MQTT] Failed to process telemetry:', err.message);
  }
}

async function handleCommandAck(gatewayId, deviceId, payload) {
  console.log(`[MQTT] ACK from gateway ${gatewayId} for device ${deviceId}:`, payload);
}

async function handleTelemetryBatch(gatewayId, payload) {
  console.log(`[MQTT] Batch telemetry from ${gatewayId}`);
}

async function handleEvents(gatewayId, payload) {
  console.log(`[MQTT] Event from ${gatewayId}`);
}

async function handleGatewayStatus(gatewayId, payload) {
  try {
    const status = payload.trim() === 'online' ? 'online' : 'offline';
    await prisma.gateway.updateMany({
      where: { id: gatewayId },
      data: {
        status: status,
        lastSeen: status === 'online' ? new Date() : undefined
      }
    });
  } catch (err) { }
}

function publishDeviceCommand({ gatewayId, deviceId, command, commandId, ttlSeconds = 60 }) {
  const cmdId = commandId || crypto.randomUUID();
  const topic = `gateway/${gatewayId}/device/${deviceId}/command`;
  const payload = JSON.stringify({
    command_id: cmdId,
    target_device_id: deviceId,
    command: command,
    ttl_seconds: ttlSeconds,
    source: 'web_app',
    timestamp: Math.floor(Date.now() / 1000)
  });

  return new Promise((resolve) => {
    if (!mqttClient || !mqttClient.connected) return resolve(false);
    mqttClient.publish(topic, payload, { qos: 1 }, (err) => {
      resolve(!err);
    });
  });
}

async function publishScheduleConfig(gatewayId) {
  return true;
}

async function isGatewayOnline(gatewayId) {
  const gw = await prisma.gateway.findUnique({
    where: { id: gatewayId },
    select: { status: true }
  });
  return gw ? gw.status === 'online' : false;
}

async function stopBroker() {
  if (mqttClient) {
    mqttClient.end();
    isRunning = false;
  }
}

module.exports = {
  startBroker,
  stopBroker,
  publishDeviceCommand,
  publishScheduleConfig,
  isGatewayOnline
};
