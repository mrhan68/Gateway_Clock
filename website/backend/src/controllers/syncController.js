/**
 * Controllers for Gateway Sync endpoints.
 *
 * These endpoints are called by the ESP32 Gateway to:
 * 1. Pull schedule + rules config
 * 2. Pull device list config
 * 3. Push event log entries
 *
 * Per FSD §4.3.5: Backend is the source of truth for schedule.json and device_list.json.
 * The Gateway only reads these — it does NOT write config back.
 */
const { PrismaClient } = require('@prisma/client');
const prisma = new PrismaClient();
const { createError } = require('../middleware/errorHandler');

/**
 * Map DB enum to the string used in the Gateway JSON config.
 * DB: automation|alarm|personal_event → Gateway JSON: automation|alarm|personal_event
 */
function mapScheduleType(dbType) {
  return dbType.toLowerCase(); // already lowercase in DB
}

/**
 * Map DB enum to the condition string used in Gateway JSON config.
 * DB: gt|lt|eq → Gateway JSON: "SENSOR_GT"|"SENSOR_LT"|"SENSOR_EQ"
 */
function mapConditionType(dbType) {
  const map = { gt: 'SENSOR_GT', lt: 'SENSOR_LT', eq: 'SENSOR_EQ' };
  return map[dbType] || dbType;
}

/**
 * GET /gateways/{gateway_id}/sync/schedule
 *
 * Pull the latest schedule, automation rules, and device list.
 * Returns JSON in the format the Gateway expects (schedule.json + rules merged).
 */
async function syncSchedule(req, res, next) {
  try {
    const { gateway_id } = req.params;

    // Verify gateway exists (also updates last_seen)
    const gatewayExists = await prisma.gateway.findUnique({
      where: { id: gateway_id },
      select: { id: true }
    });
    if (!gatewayExists) {
      throw createError('GATEWAY_NOT_FOUND',
        `Gateway '${gateway_id}' not found`);
    }

    // Fetch schedules
    const schedules = await prisma.schedule.findMany({
      where: { gatewayId: gateway_id },
      orderBy: [{ hour: 'asc' }, { minute: 'asc' }]
    });

    // Fetch automation rules
    const rules = await prisma.automationRule.findMany({
      where: { gatewayId: gateway_id },
      orderBy: { createdAt: 'desc' }
    });

    // Format schedules for Gateway (per FSD §7.3 JSON schema)
    const scheduleJson = schedules.map(s => {
      let repeatDays = null;
      if (s.repeatDays) {
        // Parse "MON,TUE,WED..." into [1,2,3,4,5] (0=Sun)
        const dayMap = {
          SUN: 0, MON: 1, TUE: 2, WED: 3, THU: 4, FRI: 5, SAT: 6
        };
        repeatDays = s.repeatDays.split(',').map(d => dayMap[d]);
      }

      const entry = {
        id: s.id,
        type: mapScheduleType(s.type),
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
      return entry;
    });

    // Format rules for Gateway (per FSD §7.5)
    const ruleJson = rules.map(r => ({
      id: r.id,
      sensor_device_id: r.sensorDeviceId,
      condition_type: mapConditionType(r.conditionType),
      threshold: r.threshold,
      target_device_id: r.targetDeviceId,
      action_command: r.actionCommand,
      enabled: r.enabled
    }));

    const lastUpdated = schedules.length > 0 || rules.length > 0
      ? new Date().toISOString()
      : (await prisma.gateway.findUnique({
        where: { id: gateway_id },
        select: { updatedAt: true }
      })).updatedAt.toISOString();

    res.json({
      schedules: scheduleJson,
      rules: ruleJson,
      last_updated: lastUpdated
    });

  } catch (err) {
    next(err);
  }
}

/**
 * GET /gateways/{gateway_id}/sync/devices
 *
 * Pull the latest device list for this gateway.
 */
async function syncDevices(req, res, next) {
  try {
    const { gateway_id } = req.params;

    // Verify gateway
    const gatewayExists = await prisma.gateway.findUnique({
      where: { id: gateway_id },
      select: { id: true }
    });
    if (!gatewayExists) {
      throw createError('GATEWAY_NOT_FOUND',
        `Gateway '${gateway_id}' not found`);
    }

    const devices = await prisma.device.findMany({
      where: { gatewayId: gateway_id },
      orderBy: { createdAt: 'desc' }
    });

    // Format for Gateway (per FSD §7.4 JSON schema)
    const deviceList = devices.map(d => ({
      device_id: d.deviceId,
      name: d.name,
      protocol: d.protocol,
      mac_address: d.macAddress,
      mqtt_topic: d.mqttTopic,
      device_type: d.deviceType,
      capabilities: d.capabilities ? JSON.parse(d.capabilities) : null
    }));

    const lastUpdated = devices.length > 0
      ? new Date().toISOString()
      : new Date().toISOString();

    res.json({
      devices: deviceList,
      last_updated: lastUpdated
    });

  } catch (err) {
    next(err);
  }
}

/**
 * POST /gateways/{gateway_id}/sync/events
 *
 * Receive event log entries from the Gateway.
 * Gateway pushes its accumulated event_log.json to the Backend.
 */
async function syncEvents(req, res, next) {
  try {
    const { gateway_id } = req.params;
    const { events } = req.body;

    if (!Array.isArray(events)) {
      return res.status(400).json({
        error: {
          code: 'VALIDATION_ERROR',
          message: 'Field "events" must be an array',
          status: 400
        }
      });
    }

    // Verify gateway
    const gatewayExists = await prisma.gateway.findUnique({
      where: { id: gateway_id },
      select: { id: true }
    });
    if (!gatewayExists) {
      throw createError('GATEWAY_NOT_FOUND',
        `Gateway '${gateway_id}' not found`);
    }

    let syncedCount = 0;
    const now = new Date();

    for (const event of events) {
      // Map Gateway event format to DB
      // Gateway sends: { type, ts, ...details }
      // DB expects: { eventType, deviceId, payload, occurredAt }
      try {
        const occurredAt = event.ts
          ? new Date(event.ts * 1000) // Convert Unix timestamp
          : now;

        const eventType = event.type;
        const deviceId = event.device_id || event.target || null;
        const payload = JSON.stringify({
          type: eventType,
          rule_id: event.rule_id || undefined,
          schedule_id: event.schedule_id || undefined,
          target: event.target || undefined,
          command: event.command || undefined
        });

        await prisma.eventLog.create({
          data: {
            gatewayId: gateway_id,
            eventType,
            deviceId,
            payload,
            occurredAt,
            syncedAt: now
          }
        });

        syncedCount++;
      } catch (eventErr) {
        console.error('[SYNC] Failed to process event:', eventErr.message);
        // Continue processing other events even if one fails
      }
    }

    res.json({
      synced_count: syncedCount,
      message: `Successfully synced ${syncedCount} of ${events.length} events`
    });

  } catch (err) {
    next(err);
  }
}

module.exports = {
  syncSchedule,
  syncDevices,
  syncEvents
};
