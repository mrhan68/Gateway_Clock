const mqtt = require('mqtt');
const prisma = require('../config/prisma');
const crypto = require('crypto');
const {
  ALIAS_TO_RELAY,
  RELAY_TO_ALIAS,
  DEFAULT_ACTUATOR_NAMES,
  resolveRelayTargets,
  recordPendingCommand,
  consumePendingCommand
} = require('./relayAliases');

let mqttClient = null;
let isRunning = false;
const seededGateways = new Set();

const DAY_NAMES = ['SUN', 'MON', 'TUE', 'WED', 'THU', 'FRI', 'SAT'];

const DEFAULT_FSD_RULES = [
  { sensorDeviceId: 'sensor_temp_01', conditionType: 'gt', threshold: 30.0, hysteresis: 1.5, cooldownS: 60, targetDeviceId: 'fan_01', actionCommand: 'ON', enabled: true },
  { sensorDeviceId: 'sensor_temp_01', conditionType: 'lt', threshold: 22.0, hysteresis: 1.5, cooldownS: 60, targetDeviceId: 'heater_01', actionCommand: 'ON', enabled: true },
  { sensorDeviceId: 'sensor_hum_01', conditionType: 'gt', threshold: 75.0, hysteresis: 2.0, cooldownS: 60, targetDeviceId: 'dehumidifier_01', actionCommand: 'ON', enabled: true }
];

function defaultHysteresisForSensor(sensorDeviceId) {
  return String(sensorDeviceId || '').toLowerCase().includes('hum') ? 2.0 : 1.5;
}

function parseFiniteNumber(val) {
  if (val === undefined || val === null || val === '') return undefined;
  const num = typeof val === 'number' ? val : parseFloat(val);
  return Number.isFinite(num) ? num : undefined;
}

async function runInTransaction(fn) {
  if (typeof prisma.$transaction === 'function') {
    return prisma.$transaction(async (tx) => fn(tx));
  }
  return fn(prisma);
}

const DEFAULT_FSD_SCHEDULES = [
  { type: 'alarm', hour: 7, minute: 0, repeatDays: 'SUN,MON,TUE,WED,THU,FRI,SAT', oneTimeDate: null, enabled: true, targetDeviceId: null, actionCommand: null, title: 'Good Morning Alarm', buzzer: true },
  { type: 'automation', hour: 18, minute: 0, repeatDays: 'MON,TUE,WED,THU,FRI', oneTimeDate: null, enabled: true, targetDeviceId: 'aux_01', actionCommand: 'OFF', title: 'Office Auto AC Off', buzzer: false }
];

function resetSeededGateways() {
  seededGateways.clear();
}

function repeatDaysToIndices(repeatDays) {
  if (!repeatDays) return [];
  if (Array.isArray(repeatDays)) {
    if (repeatDays.length === 7 && repeatDays.every((v) => typeof v === 'boolean')) {
      return repeatDays.reduce((acc, enabled, idx) => {
        if (enabled) acc.push(idx);
        return acc;
      }, []);
    }
    return repeatDays
      .map((d) => (typeof d === 'number' ? d : DAY_NAMES.indexOf(String(d).trim().toUpperCase())))
      .filter((idx) => Number.isInteger(idx) && idx >= 0 && idx <= 6);
  }
  return String(repeatDays)
    .split(',')
    .map((d) => DAY_NAMES.indexOf(d.trim().toUpperCase()))
    .filter((idx) => idx >= 0);
}

function normalizeRepeatDaysString(repeatDays, fallbackIndicesOrMask) {
  const fromBoolOrMask = (arr) => {
    const names = [];
    for (let i = 0; i < 7 && i < arr.length; i++) {
      if (arr[i] === true || arr[i] === 1) names.push(DAY_NAMES[i]);
    }
    return names.length > 0 ? names.join(',') : null;
  };

  if (typeof repeatDays === 'string') {
    const tokens = repeatDays
      .split(',')
      .map((d) => d.trim().toUpperCase())
      .filter((d) => DAY_NAMES.includes(d));
    if (tokens.length > 0) return tokens.join(',');
  } else if (Array.isArray(repeatDays)) {
    if (repeatDays.length === 7 && repeatDays.every((v) => typeof v === 'boolean')) {
      return fromBoolOrMask(repeatDays);
    }
    const tokens = repeatDays
      .map((d) => (typeof d === 'number' ? DAY_NAMES[d] : String(d).trim().toUpperCase()))
      .filter((d) => DAY_NAMES.includes(d));
    if (tokens.length > 0) return tokens.join(',');
  }

  if (Array.isArray(fallbackIndicesOrMask)) {
    if (fallbackIndicesOrMask.length === 7 && fallbackIndicesOrMask.every((v) => v === 0 || v === 1 || typeof v === 'boolean')) {
      return fromBoolOrMask(fallbackIndicesOrMask);
    }
    const tokens = fallbackIndicesOrMask
      .map((d) => (typeof d === 'number' ? DAY_NAMES[d] : String(d).trim().toUpperCase()))
      .filter((d) => DAY_NAMES.includes(d));
    if (tokens.length > 0) return tokens.join(',');
  }

  return null;
}

function mapRelayToFriendlyAlias(deviceId) {
  if (!deviceId) return null;
  const trimmed = String(deviceId).trim();
  if (!trimmed) return null;
  if (trimmed === 'office_ac_relay' || trimmed === 'relay_ch4') return 'aux_01';
  return RELAY_TO_ALIAS[trimmed] || trimmed;
}

function normalizeConditionType(cond) {
  const c = String(cond || '').trim().toLowerCase();
  if (c === '>' || c === 'gt') return 'gt';
  if (c === '<' || c === 'lt') return 'lt';
  if (c === '=' || c === '==' || c === 'eq') return 'eq';
  return 'gt';
}

async function countRecordsSafe(model, gatewayId) {
  if (!model) return 0;
  if (typeof model.findMany === 'function') {
    const rows = await model.findMany({ where: { gatewayId } });
    return Array.isArray(rows) ? rows.length : 0;
  }
  if (typeof model.count === 'function') {
    return await model.count({ where: { gatewayId } });
  }
  return 0;
}

async function ensureDefaultSchedulesAndRules(gatewayId) {
  if (!gatewayId || seededGateways.has(gatewayId)) return;

  if (prisma.automationRule && typeof prisma.automationRule.create === 'function') {
    const ruleCount = await countRecordsSafe(prisma.automationRule, gatewayId);
    if (ruleCount === 0) {
      for (const rule of DEFAULT_FSD_RULES) {
        await prisma.automationRule.create({ data: { gatewayId, ...rule } });
      }
    }
  }

  if (prisma.schedule && typeof prisma.schedule.create === 'function') {
    const schedCount = await countRecordsSafe(prisma.schedule, gatewayId);
    if (schedCount === 0) {
      for (const sched of DEFAULT_FSD_SCHEDULES) {
        await prisma.schedule.create({ data: { gatewayId, ...sched } });
      }
    }
  }

  seededGateways.add(gatewayId);
}

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
    mqttClient.subscribe('gateway/+/alarm', { qos: 1 });
    mqttClient.subscribe('gateway/+/schedule/sync', { qos: 1 });
    mqttClient.subscribe('gateway/+/rules/sync', { qos: 1 });
  });

  let mqttMsgQueue = Promise.resolve();
  mqttClient.on('message', (topic, message) => {
    const payload = message.toString();
    mqttMsgQueue = mqttMsgQueue
      .then(async () => {
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
          } else if (action === 'alarm') {
            await handleAlarmEvent(gatewayId, payload);
          } else if (action === 'schedule' && parts.length > 3 && parts[3] === 'sync') {
            await handleScheduleSyncFromGateway(gatewayId, payload);
          } else if (action === 'rules' && parts.length > 3 && parts[3] === 'sync') {
            await handleRulesSyncFromGateway(gatewayId, payload);
          } else if (action === 'device' && parts.length >= 5) {
            const deviceId = parts[3];
            if (parts[4] === 'command_ack') {
              await handleCommandAck(gatewayId, deviceId, payload);
            } else if (parts[4] === 'action') {
              await handleDeviceAction(gatewayId, deviceId, payload);
            }
          }
        }
      })
      .catch((err) => {
        console.error('[MQTT] Unhandled message queue error:', err.message);
      });
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
  try {
    let data = null;
    try {
      data = parseJsonPayload(payload);
    } catch (_err) {
      data = null;
    }

    const records = Array.isArray(data)
      ? data
      : (data && Array.isArray(data.records)
        ? data.records
        : (data && Array.isArray(data.batch)
          ? data.batch
          : (data && typeof data === 'object' ? [data] : [])));

    const now = new Date();
    let insertedCount = 0;
    let deduplicatedCount = 0;
    let firstDeviceId = null;
    let firstRecordedAt = now;

    for (const rec of records) {
      if (!rec || typeof rec !== 'object') continue;
      const devId = String(rec.device_id || rec.deviceId || '').trim();
      if (!devId) continue;

      const devLower = devId.toLowerCase();
      const val = rec.val !== undefined ? rec.val : rec.value;
      const tempVal = parseFiniteNumber(
        rec.temperature !== undefined
          ? rec.temperature
          : (rec.temp !== undefined ? rec.temp : (!devLower.includes('hum') ? val : undefined))
      );
      const humVal = parseFiniteNumber(
        rec.humidity !== undefined
          ? rec.humidity
          : (rec.hum !== undefined ? rec.hum : (devLower.includes('hum') ? val : undefined))
      );

      if (tempVal === undefined && humVal === undefined) continue;

      const rawTs = rec.ts !== undefined
        ? rec.ts
        : (rec.timestamp !== undefined ? rec.timestamp : rec.recorded_at);
      const recordedAt = parseTimestampOrNow(rawTs, now);

      if (!firstDeviceId) {
        firstDeviceId = devId;
        firstRecordedAt = recordedAt;
      }

      const entries = [];
      if (tempVal !== undefined) {
        entries.push({ gatewayId, deviceId: devId, value: tempVal, unit: '°C', recordedAt });
      }
      if (humVal !== undefined) {
        entries.push({ gatewayId, deviceId: devId, value: humVal, unit: '%', recordedAt });
      }

      for (const entry of entries) {
        let duplicate = null;
        if (prisma.telemetryLog && typeof prisma.telemetryLog.findFirst === 'function') {
          duplicate = await prisma.telemetryLog.findFirst({
            where: {
              gatewayId,
              deviceId: devId,
              unit: entry.unit,
              recordedAt
            }
          });
        }

        if (duplicate) {
          deduplicatedCount++;
        } else if (prisma.telemetryLog && typeof prisma.telemetryLog.create === 'function') {
          await prisma.telemetryLog.create({ data: entry });
          insertedCount++;
        }
      }

      const primaryVal = devLower.includes('hum')
        ? (humVal !== undefined ? humVal : tempVal)
        : (tempVal !== undefined ? tempVal : humVal);

      if (prisma.device && typeof prisma.device.findFirst === 'function') {
        const existingDev = await prisma.device.findFirst({
          where: { gatewayId, deviceId: devId }
        });
        if (!existingDev && typeof prisma.device.create === 'function') {
          try {
            await prisma.device.create({
              data: {
                deviceId: devId,
                gatewayId,
                name: rec.device_name || 'Cảm Biến BTHome Môi Trường',
                deviceType: 'sensor',
                protocol: 'ble',
                macAddress: rec.mac_address || null,
                capabilities: JSON.stringify(['temperature', 'humidity']),
                status: 'online',
                lastSeen: recordedAt,
                lastValue: primaryVal
              }
            });
          } catch (err) {
            if (!err || err.code !== 'P2002') throw err;
          }
        } else if (
          existingDev &&
          typeof prisma.device.update === 'function' &&
          (!existingDev.lastSeen || recordedAt >= new Date(existingDev.lastSeen))
        ) {
          await prisma.device.update({
            where: { id: existingDev.id },
            data: {
              status: 'online',
              lastSeen: recordedAt,
              lastValue: primaryVal
            }
          });
        }
      }
    }

    if (prisma.eventLog && typeof prisma.eventLog.create === 'function') {
      await prisma.eventLog.create({
        data: {
          gatewayId,
          eventType: 'telemetry_batch_backfill',
          deviceId: firstDeviceId ? firstDeviceId.slice(0, 32) : null,
          payload: JSON.stringify({
            total_received: records.length,
            inserted_count: insertedCount,
            deduplicated_count: deduplicatedCount,
            raw: typeof payload === 'string' ? payload : data
          }),
          occurredAt: firstRecordedAt
        }
      });
    }
  } catch (err) {
    console.error('[MQTT] Failed to process telemetry batch:', err.message);
  }
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

async function handleAlarmEvent(gatewayId, payload) {
  try {
    const data = parseJsonPayload(payload);
    await prisma.eventLog.create({
      data: {
        gatewayId,
        eventType: 'alarm_triggered',
        deviceId: null,
        payload: JSON.stringify({
          ...data,
          message: `Alarm triggered: ${data.title || 'Alarm'} at ${data.time || ''}`.trim()
        }),
        occurredAt: new Date()
      }
    });
  } catch (err) {
    console.error('[MQTT] Failed to process alarm event:', err.message);
  }
}

async function handleScheduleSyncFromGateway(gatewayId, payload) {
  try {
    const parsed = parseJsonPayload(payload);
    const items = Array.isArray(parsed)
      ? parsed
      : (parsed && Array.isArray(parsed.schedules) ? parsed.schedules : null);
    if (!items) return;

    seededGateways.add(gatewayId);

    await runInTransaction(async (tx) => {
      const scheduleModel = (tx && tx.schedule) || prisma.schedule;
      if (scheduleModel && typeof scheduleModel.deleteMany === 'function') {
        await scheduleModel.deleteMany({ where: { gatewayId } });
      }

      if (scheduleModel && typeof scheduleModel.create === 'function') {
        for (const item of items) {
          const typeRaw = String(item.type || 'alarm').trim().toLowerCase();
          const type = ['automation', 'alarm', 'personal_event'].includes(typeRaw) ? typeRaw : 'alarm';
          const oneTimeDateRaw = item.one_time_date && String(item.one_time_date).trim()
            ? new Date(item.one_time_date)
            : null;
          const oneTimeDate = oneTimeDateRaw && !Number.isNaN(oneTimeDateRaw.getTime()) ? oneTimeDateRaw : null;
          const repeatDays = oneTimeDate
            ? null
            : normalizeRepeatDaysString(item.repeat_days, item.repeat_days_indices || item.repeat_days_mask);
          const targetDeviceId = mapRelayToFriendlyAlias(item.target_device_id);
          const actionCommand = item.action_command ? String(item.action_command).trim() : null;

          await scheduleModel.create({
            data: {
              gatewayId,
              type,
              hour: Number.isInteger(item.hour) ? item.hour : (parseInt(item.hour, 10) || 0),
              minute: Number.isInteger(item.minute) ? item.minute : (parseInt(item.minute, 10) || 0),
              repeatDays,
              oneTimeDate,
              enabled: item.enabled !== undefined ? Boolean(item.enabled) : true,
              targetDeviceId,
              actionCommand: actionCommand || null,
              title: item.title ? String(item.title).slice(0, 64) : null,
              buzzer: Boolean(item.buzzer)
            }
          });
        }
      }
    });

    await publishScheduleConfig(gatewayId, { skipSeed: true });
  } catch (err) {
    console.error('[MQTT] Failed to sync schedules from gateway:', err.message);
  }
}

async function handleRulesSyncFromGateway(gatewayId, payload) {
  try {
    const parsed = parseJsonPayload(payload);
    const items = Array.isArray(parsed)
      ? parsed
      : (parsed && Array.isArray(parsed.rules) ? parsed.rules : null);
    if (!items) return;

    seededGateways.add(gatewayId);

    await runInTransaction(async (tx) => {
      const ruleModel = (tx && tx.automationRule) || prisma.automationRule;
      if (ruleModel && typeof ruleModel.deleteMany === 'function') {
        await ruleModel.deleteMany({ where: { gatewayId } });
      }

      if (ruleModel && typeof ruleModel.create === 'function') {
        for (const item of items) {
          const sensorDeviceId = String(item.sensor_device_id || 'sensor_temp_01').trim();
          const targetDeviceId = mapRelayToFriendlyAlias(item.target_device_id) || item.target_device_id;
          const hyst = parseFiniteNumber(item.hysteresis);
          const cooldownRaw = item.cooldown_s !== undefined ? item.cooldown_s : item.cooldownS;
          const cooldownParsed = parseFiniteNumber(cooldownRaw);
          await ruleModel.create({
            data: {
              gatewayId,
              sensorDeviceId,
              conditionType: normalizeConditionType(item.condition_type || item.condition),
              threshold: Number(item.threshold ?? 0),
              hysteresis: hyst !== undefined && hyst >= 0 ? hyst : defaultHysteresisForSensor(sensorDeviceId),
              cooldownS: cooldownParsed !== undefined && cooldownParsed >= 0 ? Math.trunc(cooldownParsed) : 60,
              targetDeviceId,
              actionCommand: String(item.action_command || item.action || 'ON').trim().toUpperCase(),
              enabled: item.enabled !== undefined ? Boolean(item.enabled) : true
            }
          });
        }
      }
    });

    await publishScheduleConfig(gatewayId, { skipSeed: true });
  } catch (err) {
    console.error('[MQTT] Failed to sync rules from gateway:', err.message);
  }
}

async function publishScheduleConfig(gatewayId, opts = {}) {
  if (!opts.skipSeed) {
    try {
      await ensureDefaultSchedulesAndRules(gatewayId);
    } catch (err) {
      console.error('[MQTT] Seed check failed:', err.message);
    }
  }

  let schedules = [];
  let rules = [];
  let gateway = null;
  let bleDevices = [];
  try {
    if (prisma.schedule && typeof prisma.schedule.findMany === 'function') {
      schedules = await prisma.schedule.findMany({
        where: { gatewayId },
        orderBy: { id: 'asc' }
      });
    }
    if (prisma.automationRule && typeof prisma.automationRule.findMany === 'function') {
      rules = await prisma.automationRule.findMany({
        where: { gatewayId },
        orderBy: { id: 'asc' }
      });
    }
    if (prisma.gateway && typeof prisma.gateway.findUnique === 'function') {
      gateway = await prisma.gateway.findUnique({
        where: { id: gatewayId },
        select: { timezone: true }
      });
    }
    if (prisma.device && typeof prisma.device.findMany === 'function') {
      const foundBle = await prisma.device.findMany({
        where: { gatewayId, protocol: 'ble' },
        orderBy: { id: 'asc' }
      });
      if (Array.isArray(foundBle)) bleDevices = foundBle;
    }
  } catch (err) {
    console.error('[MQTT] Failed to query schedules/rules:', err.message);
    return false;
  }

  const formattedSchedules = (schedules || []).map((s) => ({
    id: s.id,
    type: s.type,
    hour: s.hour,
    minute: s.minute,
    repeat_days: s.repeatDays || '',
    repeat_days_indices: repeatDaysToIndices(s.repeatDays),
    one_time_date: s.oneTimeDate ? new Date(s.oneTimeDate).toISOString().split('T')[0] : '',
    enabled: Boolean(s.enabled),
    target_device_id: s.targetDeviceId ? (ALIAS_TO_RELAY[s.targetDeviceId] || s.targetDeviceId) : '',
    action_command: s.actionCommand || '',
    title: s.title || '',
    buzzer: Boolean(s.buzzer)
  }));

  const formattedRules = (rules || []).map((r) => ({
    id: r.id,
    sensor_device_id: r.sensorDeviceId,
    condition_type: r.conditionType,
    threshold: r.threshold,
    hysteresis: r.hysteresis ?? defaultHysteresisForSensor(r.sensorDeviceId),
    cooldown_s: r.cooldownS ?? r.cooldown_s ?? 60,
    target_device_id: ALIAS_TO_RELAY[r.targetDeviceId] || r.targetDeviceId,
    action_command: r.actionCommand,
    enabled: Boolean(r.enabled)
  }));

  const formattedBleSensors = bleDevices.map((d) => ({
    device_id: d.deviceId,
    mac_address: d.macAddress || ''
  }));

  if (!mqttClient || !mqttClient.connected) return false;

  const topic = `gateway/${gatewayId}/config/schedule`;
  const payload = JSON.stringify({
    schedules: formattedSchedules,
    rules: formattedRules,
    timezone: (gateway && gateway.timezone) || 'ICT-7',
    ble_sensors: formattedBleSensors,
    updated_at: Math.floor(Date.now() / 1000)
  });

  return new Promise((resolve) => {
    mqttClient.publish(topic, payload, { qos: 1, retain: true }, (err) => {
      if (err) {
        console.error('[MQTT] Failed to publish schedule config:', err.message);
        resolve(false);
      } else {
        resolve(true);
      }
    });
  });
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
    if (typeof mqttClient.removeAllListeners === 'function') {
      mqttClient.removeAllListeners('connect');
      mqttClient.removeAllListeners('message');
      mqttClient.removeAllListeners('error');
    }
    mqttClient.end();
    mqttClient = null;
    isRunning = false;
  }
}

module.exports = {
  startBroker,
  stopBroker,
  publishDeviceCommand,
  publishScheduleConfig,
  ensureDefaultSchedulesAndRules,
  resetSeededGateways,
  isGatewayOnline,
  handleTelemetry,
  handleTelemetryBatch,
  handleCommandAck,
  handleDeviceAction,
  handleEvents,
  handleAlarmEvent,
  handleScheduleSyncFromGateway,
  handleRulesSyncFromGateway
};
