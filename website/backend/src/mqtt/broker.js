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

    // ─── Command ACK from gateway → backend (FSD §9.1) ───
    if (parts[2] === 'device' && parts[4] === 'command' && parts[5] === 'ack') {
      const deviceId = parts[3];
      await handleCommandAck(gatewayId, deviceId, payload);
      return;
    }

    // ─── Offline telemetry batch backfill (FSD §4.11.3 & §9.1) ───
    if (parts[2] === 'telemetry' && parts[3] === 'batch') {
      await handleTelemetryBatch(gatewayId, payload);
      return;
    }

    // ─── Realtime event log stream (FSD §9.1) ───
    if (parts[2] === 'events' && parts.length === 3) {
      await handleEvents(gatewayId, payload);
      return;
    }

    // ─── Gateway status (LWT / heartbeat) ───
    if (parts[2] === 'status' && parts.length === 3) {
      await handleGatewayStatus(gatewayId, payload);
      return;
    }

    // ─── Device commands (from gateway to devices) ───
    if (parts[2] === 'device' && parts[4] === 'command') {
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
 * Handle command ACK published by gateway (FSD §9.2).
 * Topic: gateway/{id}/device/{deviceId}/command/ack
 */
async function handleCommandAck(gatewayId, deviceId, payload) {
  try {
    const data = JSON.parse(payload);
    console.log(`[MQTT] Command ACK for ${deviceId} on gateway ${gatewayId}:`, data);

    const occurredAt = data.timestamp ? new Date(data.timestamp * 1000) : new Date();

    await prisma.eventLog.create({
      data: {
        gatewayId,
        eventType: 'command_ack',
        deviceId,
        payload: JSON.stringify(data),
        occurredAt,
        syncedAt: new Date()
      }
    });

    // If command was successful and toggled device state, update last seen
    await prisma.device.updateMany({
      where: { gatewayId, deviceId },
      data: {
        lastSeen: new Date(),
        status: 'online'
      }
    });
  } catch (err) {
    console.error('[MQTT] Failed to process command ACK:', err.message);
  }
}

/**
 * Handle batch telemetry backfill published by gateway (FSD §4.11.3 & §9.1).
 * Topic: gateway/{id}/telemetry/batch
 */
async function handleTelemetryBatch(gatewayId, payload) {
  try {
    const parsed = JSON.parse(payload);
    const records = Array.isArray(parsed) ? parsed : (parsed.batch || parsed.records || []);
    console.log(`[MQTT] Ingesting batch telemetry for gateway ${gatewayId}: ${records.length} records`);

    let count = 0;
    for (const rec of records) {
      if (!rec.device_id && !rec.deviceId) continue;
      const devId = rec.device_id || rec.deviceId;
      const val = rec.value !== undefined ? rec.value : 0;
      const recordedAt = rec.timestamp
        ? new Date(rec.timestamp * 1000)
        : (rec.recorded_at ? new Date(rec.recorded_at) : new Date());

      // Deduplication: check if already recorded
      const existing = await prisma.telemetryLog.findFirst({
        where: {
          gatewayId,
          deviceId: devId,
          recordedAt
        }
      });

      if (!existing) {
        await prisma.telemetryLog.create({
          data: {
            gatewayId,
            deviceId: devId,
            value: val,
            unit: rec.unit || null,
            recordedAt
          }
        });
        count++;
      }
    }
    console.log(`[MQTT] Batch telemetry stored: ${count} new records ingested (deduplicated)`);
  } catch (err) {
    console.error('[MQTT] Failed to process telemetry batch:', err.message);
  }
}

/**
 * Handle real-time event logs published by gateway (FSD §9.1).
 * Topic: gateway/{id}/events
 */
async function handleEvents(gatewayId, payload) {
  try {
    const data = JSON.parse(payload);
    console.log(`[MQTT] Realtime Event for gateway ${gatewayId}:`, data);

    const occurredAt = data.ts ? new Date(data.ts * 1000) : (data.timestamp ? new Date(data.timestamp * 1000) : new Date());

    await prisma.eventLog.create({
      data: {
        gatewayId,
        eventType: data.type || data.eventType || 'unknown',
        deviceId: data.device_id || data.deviceId || data.target || null,
        payload: JSON.stringify(data),
        occurredAt,
        syncedAt: new Date()
      }
    });
  } catch (err) {
    console.error('[MQTT] Failed to process realtime event:', err.message);
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

    await prisma.gateway.updateMany({
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
 * Publish a device command from REST API to the MQTT broker (FSD §9.2).
 * Topic: gateway/{gatewayId}/device/{deviceId}/command
 */
function publishDeviceCommand({ gatewayId, deviceId, command, commandId, ttlSeconds = 60 }) {
  const crypto = require('crypto');
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
 * Push updated schedule and rules config to the Gateway via retained MQTT topic (FSD §4.8.3, §9.1).
 * Topic: gateway/{gatewayId}/config/schedule (QoS 1, retain: true)
 */
async function publishScheduleConfig(gatewayId) {
  try {
    const schedules = await prisma.schedule.findMany({
      where: { gatewayId },
      orderBy: [{ hour: 'asc' }, { minute: 'asc' }]
    });

    const rules = await prisma.automationRule.findMany({
      where: { gatewayId },
      orderBy: { createdAt: 'desc' }
    });

    const dayMap = { SUN: 0, MON: 1, TUE: 2, WED: 3, THU: 4, FRI: 5, SAT: 6 };
    const condMap = { gt: 'SENSOR_GT', lt: 'SENSOR_LT', eq: 'SENSOR_EQ' };

    const scheduleJson = schedules.map(s => {
      let repeatDays = null;
      if (s.repeatDays) {
        repeatDays = s.repeatDays.split(',').map(d => dayMap[d.trim()] ?? d);
      }
      return {
        id: s.id,
        type: s.type.toLowerCase(),
        hour: s.hour,
        minute: s.minute,
        repeat: repeatDays,
        date: s.oneTimeDate ? s.oneTimeDate.toISOString().split('T')[0] : null,
        enabled: s.enabled,
        target_device_id: s.targetDeviceId,
        action_command: s.actionCommand,
        title: s.title,
        buzzer: s.buzzer
      };
    });

    const ruleJson = rules.map(r => ({
      id: r.id,
      sensor_device_id: r.sensorDeviceId,
      condition_type: condMap[r.conditionType] || r.conditionType,
      threshold: r.threshold,
      target_device_id: r.targetDeviceId,
      action_command: r.actionCommand,
      enabled: r.enabled
    }));

    const topic = `gateway/${gatewayId}/config/schedule`;
    const payload = JSON.stringify({
      schedules: scheduleJson,
      rules: ruleJson,
      last_updated: new Date().toISOString()
    });

    const packet = {
      topic,
      payload,
      qos: 1,
      retain: true
    };

    return new Promise((resolve) => {
      aedes.publish(packet, (err) => {
        if (err) {
          console.error(`[MQTT] Failed to publish config to ${topic}:`, err.message);
          resolve(false);
        } else {
          console.log(`[MQTT] Published retained config to ${topic} (${schedules.length} schedules, ${rules.length} rules)`);
          resolve(true);
        }
      });
    });
  } catch (err) {
    console.error(`[MQTT] Error preparing schedule config push for gateway ${gatewayId}:`, err.message);
    return false;
  }
}

module.exports = {
  startBroker,
  stopBroker,
  publishDeviceCommand,
  publishScheduleConfig,
  isGatewayOnline,
  aedes
};
