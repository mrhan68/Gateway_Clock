const mqtt = require('mqtt');
const { PrismaClient } = require('@prisma/client');
const crypto = require('crypto');
const {
  DEFAULT_ACTUATOR_NAMES,
  resolveRelayTargets,
  recordPendingCommand,
  consumePendingCommand
} = require('./relayAliases');

const prisma = new PrismaClient();
let mqttClient = null;
let isRunning = false;

function parseJsonPayload(payload) {
  return typeof payload === 'string' ? JSON.parse(payload) : payload;
}

function parseTimestampOrNow(ts, fallbackNow) {
  if (typeof ts === 'number' && ts > 1e9) {
    return new Date(ts < 1e12 ? ts * 1000 : ts);
  }
  if (typeof ts === 'string' && ts.trim()) {
    const parsed = new Date(ts);
    if (!Number.isNaN(parsed.getTime())) return parsed;
  }
  return fallbackNow;
}

async function upsertActuatorRecord(gatewayId, id, updateData, now, numericVal) {
  const res = await prisma.device.updateMany({
    where: { gatewayId, deviceId: id },
    data: updateData
  });
  if (res && res.count === 0) {
    try {
      await prisma.device.create({
        data: {
          gatewayId,
          deviceId: id,
          name: DEFAULT_ACTUATOR_NAMES[id] || id,
          deviceType: 'actuator',
          protocol: 'mqtt',
          status: 'online',
          lastSeen: now,
          ...(numericVal !== undefined ? { lastValue: numericVal } : {})
        }
      });
    } catch (err) {
      if (err && err.code === 'P2002') {
        await prisma.device.updateMany({
          where: { gatewayId, deviceId: id },
          data: updateData
        });
      } else {
        throw err;
      }
    }
  }
}

async function syncActuatorState(gatewayId, deviceId, arg3, arg4) {
  let now;
  let numericVal;
  if (arg3 instanceof Date) {
    now = arg3;
    numericVal = typeof arg4 === 'number'
      ? arg4
      : (String(arg4 || '').trim().toUpperCase() === 'ON'
        ? 1
        : String(arg4 || '').trim().toUpperCase() === 'OFF'
          ? 0
          : undefined);
  } else {
    now = arg4 instanceof Date ? arg4 : new Date();
    const normState = typeof arg3 === 'string' ? arg3.trim().toUpperCase() : arg3;
    numericVal = (normState === 'ON' || normState === 1 || normState === true)
      ? 1
      : (normState === 'OFF' || normState === 0 || normState === false)
        ? 0
        : undefined;
  }

  const { primary, optionalLegacy } = resolveRelayTargets(deviceId);
  const updateData = {
    status: 'online',
    lastSeen: now
  };
  if (numericVal !== undefined) {
    updateData.lastValue = numericVal;
  }

  await Promise.all([
    ...primary.map((id) => upsertActuatorRecord(gatewayId, id, updateData, now, numericVal)),
    ...optionalLegacy.map((id) =>
      prisma.device.updateMany({
        where: { gatewayId, deviceId: id },
        data: updateData
      })
    )
  ]);
}

async function startBroker() {
  if (isRunning) return;
  const brokerUri = process.env.MQTT_URI || 'mqtts://localhost:8883';
  const username = process.env.MQTT_USER || '';
  const password = process.env.MQTT_PASS || '';

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
    mqttClient.subscribe('gateway/+/device/+/action', { qos: 1 });
    mqttClient.subscribe('gateway/+/telemetry/batch', { qos: 1 });
    mqttClient.subscribe('gateway/+/events', { qos: 1 });
    mqttClient.subscribe('gateway/+/status', { qos: 1 });
  });

  mqttClient.on('message', async (topic, message) => {
    const payload = message.toString();
    const parts = topic.split('/');
    if (parts.length >= 3 && parts[0] === 'gateway') {
      const gatewayId = parts[1];
      const action = parts[2];

      if (action === 'telemetry') {
        if (parts.length > 3 && parts[3] === 'batch') await handleTelemetryBatch(gatewayId, payload);
        else await handleTelemetry(gatewayId, payload);
      } else if (action === 'events') {
        await handleEvents(gatewayId, payload);
      } else if (action === 'status') {
        await handleGatewayStatus(gatewayId, payload);
      } else if (action === 'device' && parts.length >= 5) {
        const deviceId = parts[3];
        if (parts[4] === 'command_ack') {
          await handleCommandAck(gatewayId, deviceId, payload);
        } else if (parts[4] === 'action') {
          await handleDeviceAction(gatewayId, deviceId, payload);
        }
      }
    }
  });

  mqttClient.on('error', (err) => {
    console.error('[MQTT] Connection error:', err.message);
  });
}

async function handleTelemetry(gatewayId, payload) {
  try {
    const data = parseJsonPayload(payload);
    const now = new Date();
    const gwUpdateOp = prisma.gateway.updateMany({
      where: { id: gatewayId },
      data: {
        lastSeen: now,
        status: 'online'
      }
    });

    const devId = data.device_id;
    const temp = data.temperature !== undefined ? data.temperature : data.temp;
    const hum = data.humidity !== undefined ? data.humidity : data.hum;

    if (!devId || (temp === undefined && hum === undefined)) {
      await gwUpdateOp;
      return;
    }

    const primaryRaw = devId.toLowerCase().includes('hum')
      ? (hum !== undefined ? hum : temp)
      : (temp !== undefined ? temp : hum);
    const primaryVal = typeof primaryRaw === 'number' ? primaryRaw : parseFloat(primaryRaw);

    const deviceOp = (async () => {
      const existing = await prisma.device.findFirst({
        where: { gatewayId, deviceId: devId }
      });
      if (!existing) {
        return prisma.device.create({
          data: {
            deviceId: devId,
            gatewayId,
            name: data.device_name || 'Cảm Biến BTHome Môi Trường',
            deviceType: 'sensor',
            protocol: 'ble',
            macAddress: data.mac_address || null,
            capabilities: JSON.stringify(['temperature', 'humidity']),
            status: 'online',
            lastSeen: now,
            lastValue: primaryVal
          }
        });
      }
      return prisma.device.update({
        where: { id: existing.id },
        data: {
          status: 'online',
          lastSeen: now,
          lastValue: primaryVal
        }
      });
    })();

    const logs = [];
    if (temp !== undefined) {
      logs.push({
        gatewayId,
        deviceId: devId,
        value: typeof temp === 'number' ? temp : parseFloat(temp),
        unit: '°C',
        recordedAt: now
      });
    }
    if (hum !== undefined) {
      logs.push({
        gatewayId,
        deviceId: devId,
        value: typeof hum === 'number' ? hum : parseFloat(hum),
        unit: '%',
        recordedAt: now
      });
    }

    const dbOps = [gwUpdateOp, deviceOp];
    if (logs.length > 0) {
      if (typeof prisma.telemetryLog.createMany === 'function') {
        dbOps.push(prisma.telemetryLog.createMany({ data: logs }));
      } else {
        dbOps.push(...logs.map((entry) => prisma.telemetryLog.create({ data: entry })));
      }
    }

    await Promise.all(dbOps);
  } catch (err) {
    console.error('[MQTT] Failed to process telemetry:', err.message);
  }
}

async function handleCommandAck(gatewayId, deviceId, payload) {
  try {
    const data = parseJsonPayload(payload);
    const now = new Date();
    const msgId = data.msg_id || data.command_id || null;
    const pending = msgId ? consumePendingCommand(msgId) : null;

    let state = data.state
      ? String(data.state).trim().toUpperCase()
      : (pending && pending.state ? pending.state : null);

    if (state !== 'ON' && state !== 'OFF' && typeof data.message === 'string') {
      if (/\bturned ON\b/i.test(data.message)) state = 'ON';
      else if (/\bturned OFF\b/i.test(data.message)) state = 'OFF';
    }

    const success = Boolean(data.success);
    const errorCode = data.error_code ?? 0;
    const eventType = success
      ? 'command_ack'
      : (errorCode === 3
        ? 'safety_interlock'
        : errorCode === 4
          ? 'compressor_dwell_lock'
          : 'command_rejected');

    const parts = [`Device ${deviceId}`];
    if (state) parts.push(`state=${state}`);
    if (msgId) parts.push(`msg_id=${msgId}`);
    if (!success) {
      parts.push(`error_code=${errorCode}`);
      if (data.error || data.message) parts.push(`(${data.error || data.message})`);
    } else if (data.message) {
      parts.push(`(${data.message})`);
    }
    const messageText = `Command ${success ? 'ACK' : 'REJECTED'}: ${parts.join(' ')}`;

    const eventPayload = {
      ...data,
      device_id: deviceId,
      ...(state ? { state } : {}),
      ...(msgId ? { msg_id: msgId } : {}),
      error_code: errorCode,
      success,
      message: messageText
    };

    const eventOp = prisma.eventLog.create({
      data: {
        gatewayId,
        eventType,
        deviceId,
        payload: JSON.stringify(eventPayload),
        occurredAt: now
      }
    });

    if (success) {
      const numericVal = state === 'ON' ? 1 : state === 'OFF' ? 0 : undefined;
      await Promise.all([eventOp, syncActuatorState(gatewayId, deviceId, now, numericVal)]);
    } else {
      await eventOp;
    }
  } catch (err) {
    console.error('[MQTT] Failed to process command_ack:', err.message);
  }
}

async function handleDeviceAction(gatewayId, deviceId, payload) {
  try {
    const data = parseJsonPayload(payload);
    const now = new Date();
    const ruleId = data.rule_id !== undefined ? data.rule_id : data.source_rule;
    const rawAction = data.action !== undefined ? data.action : data.command;
    const action = rawAction !== undefined && rawAction !== null
      ? String(rawAction).trim().toUpperCase()
      : null;
    const status = data.status ? String(data.status).trim().toUpperCase() : 'EXECUTED';
    const occurredAt = parseTimestampOrNow(data.timestamp, now);

    const messageText = `Rule #${ruleId ?? '?'} ${status}: ${deviceId} -> ${action ?? 'UNKNOWN'}`;
    const eventPayload = {
      ...data,
      rule_id: ruleId ?? null,
      device_id: deviceId,
      action,
      status,
      message: messageText
    };

    const eventOp = prisma.eventLog.create({
      data: {
        gatewayId,
        eventType: 'rule_action',
        deviceId,
        payload: JSON.stringify(eventPayload),
        occurredAt
      }
    });

    if ((action === 'ON' || action === 'OFF') && status === 'EXECUTED') {
      const numericVal = action === 'ON' ? 1 : 0;
      await Promise.all([eventOp, syncActuatorState(gatewayId, deviceId, now, numericVal)]);
    } else {
      await eventOp;
    }
  } catch (err) {
    console.error('[MQTT] Failed to process device action:', err.message);
  }
}

async function handleTelemetryBatch(gatewayId, payload) {
  console.log(`[MQTT] Batch telemetry from ${gatewayId}`);
}

async function handleEvents(gatewayId, payload) {
  try {
    const data = parseJsonPayload(payload);
    const now = new Date();
    const targetDeviceId = data.device_id || (data.relay_id !== undefined && data.relay_id !== null ? `relay_${data.relay_id}` : null);
    const occurredAt = parseTimestampOrNow(data.timestamp, now);
    console.log(`[MQTT] Event from ${gatewayId}:`, data);

    const eventOp = prisma.eventLog.create({
      data: {
        gatewayId,
        eventType: data.event || data.type || data.event_type || data.reason || 'relay_event',
        deviceId: targetDeviceId,
        payload: typeof payload === 'string' ? payload : JSON.stringify(data),
        occurredAt
      }
    });

    const isRelayStateChanged =
      data.type === 'relay_state_changed' ||
      data.event_type === 'relay_state_changed' ||
      data.event === 'relay_state_changed';

    if (isRelayStateChanged && targetDeviceId && data.state !== undefined) {
      await Promise.all([
        eventOp,
        syncActuatorState(gatewayId, targetDeviceId, data.state, now)
      ]);
    } else {
      await eventOp;
    }
  } catch (err) {
    console.error('[MQTT] Failed to process event:', err.message);
  }
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
    recordPendingCommand(cmdId, {
      gatewayId,
      deviceId,
      state: String(command || '').trim().toUpperCase(),
      timestamp: Date.now()
    });
    mqttClient.publish(topic, payload, { qos: 1 }, (err) => {
      if (err) consumePendingCommand(cmdId);
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
  isGatewayOnline,
  handleTelemetry,
  handleCommandAck,
  handleDeviceAction,
  handleEvents
};
