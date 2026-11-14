/**
 * Controllers for Automation Rule management.
 */
const { PrismaClient } = require('@prisma/client');
const prisma = new PrismaClient();
const { createError } = require('../middleware/errorHandler');
const broker = require('../mqtt/broker');
const { DEFAULT_ACTUATOR_NAMES } = require('../mqtt/relayAliases');

const STANDARD_SENSOR_DEFAULTS = {
  sensor_temp_01: { name: 'Cảm biến Nhiệt độ (BTHome)', deviceType: 'sensor', protocol: 'ble' },
  sensor_hum_01: { name: 'Cảm biến Độ ẩm (BTHome)', deviceType: 'sensor', protocol: 'ble' }
};

const STANDARD_ACTUATOR_IDS = new Set([
  'fan_01',
  'heater_01',
  'dehumidifier_01',
  'aux_01',
  'relay_1',
  'relay_2',
  'relay_3',
  'relay_4'
]);

async function ensureKnownDevice(gatewayId, deviceId, role) {
  const existing = await prisma.device.findFirst({
    where: { gatewayId, deviceId }
  });
  if (existing) return existing;

  let deviceData = null;
  if (role === 'sensor' && STANDARD_SENSOR_DEFAULTS[deviceId]) {
    deviceData = {
      gatewayId,
      deviceId,
      ...STANDARD_SENSOR_DEFAULTS[deviceId],
      status: 'online'
    };
  } else if (role === 'target' && STANDARD_ACTUATOR_IDS.has(deviceId)) {
    deviceData = {
      gatewayId,
      deviceId,
      name: DEFAULT_ACTUATOR_NAMES[deviceId] || deviceId,
      deviceType: 'actuator',
      protocol: 'mqtt',
      status: 'online'
    };
  }

  if (!deviceData) {
    throw createError(
      'DEVICE_NOT_FOUND',
      `${role === 'sensor' ? 'Sensor' : 'Target'} device '${deviceId}' not found in gateway '${gatewayId}'`
    );
  }

  try {
    return await prisma.device.create({ data: deviceData });
  } catch (err) {
    if (err && err.code === 'P2002') {
      return await prisma.device.findFirst({ where: { gatewayId, deviceId } });
    }
    throw err;
  }
}

/**
 * GET /gateways/{gateway_id}/rules
 */
async function listRules(req, res, next) {
  try {
    const { gateway_id } = req.params;

    // Verify gateway
    const gateway = await prisma.gateway.findUnique({
      where: { id: gateway_id },
      select: { id: true }
    });
    if (!gateway) {
      throw createError('GATEWAY_NOT_FOUND',
        `Gateway '${gateway_id}' not found`);
    }

    let rules = await prisma.automationRule.findMany({
      where: { gatewayId: gateway_id },
      orderBy: { createdAt: 'desc' }
    });

    if (rules.length === 0 && typeof broker.ensureDefaultSchedulesAndRules === 'function') {
      await broker.ensureDefaultSchedulesAndRules(gateway_id);
      rules = await prisma.automationRule.findMany({
        where: { gatewayId: gateway_id },
        orderBy: { createdAt: 'desc' }
      });
      if (rules.length > 0 && typeof broker.publishScheduleConfig === 'function') {
        broker.publishScheduleConfig(gateway_id, { skipSeed: true }).catch((err) => {
          console.error('[RULE] Failed to publish config to gateway:', err.message);
        });
      }
    }

    const formatted = rules.map(r => ({
      id: r.id,
      gateway_id: r.gatewayId,
      sensor_device_id: r.sensorDeviceId,
      condition_type: r.conditionType,
      threshold: r.threshold,
      target_device_id: r.targetDeviceId,
      action_command: r.actionCommand,
      enabled: r.enabled,
      created_at: r.createdAt
    }));

    res.json(formatted);
  } catch (err) {
    next(err);
  }
}

/**
 * POST /gateways/{gateway_id}/rules
 */
async function createRule(req, res, next) {
  try {
    const { gateway_id } = req.params;
    const {
      sensor_device_id, condition_type, threshold,
      target_device_id, action_command, enabled = true
    } = req.body;

    // Verify gateway
    const gateway = await prisma.gateway.findUnique({
      where: { id: gateway_id },
      select: { id: true }
    });
    if (!gateway) {
      throw createError('GATEWAY_NOT_FOUND',
        `Gateway '${gateway_id}' not found`);
    }

    // Verify or auto-create standard sensor & target devices
    await ensureKnownDevice(gateway_id, sensor_device_id, 'sensor');
    await ensureKnownDevice(gateway_id, target_device_id, 'target');

    const rule = await prisma.automationRule.create({
      data: {
        gatewayId: gateway_id,
        sensorDeviceId: sensor_device_id,
        conditionType: condition_type,
        threshold: parseFloat(threshold),
        targetDeviceId: target_device_id,
        actionCommand: action_command,
        enabled
      }
    });

    // Push updated schedule/rule config to Gateway via retained MQTT topic (FSD §4.8.3, §9.1)
    broker.publishScheduleConfig(gateway_id).catch((err) => {
      console.error('[RULE] Failed to publish config to gateway:', err.message);
    });

    res.status(201).json({
      id: rule.id,
      message: 'Automation rule created successfully'
    });
  } catch (err) {
    next(err);
  }
}

/**
 * PUT /gateways/{gateway_id}/rules/{id}
 */
async function updateRule(req, res, next) {
  try {
    const { gateway_id, id } = req.params;
    const {
      sensor_device_id, condition_type, threshold,
      target_device_id, action_command, enabled
    } = req.body;

    const rule = await prisma.automationRule.update({
      where: {
        id: parseInt(id),
        gatewayId: gateway_id
      },
      data: {
        sensorDeviceId: sensor_device_id ?? undefined,
        conditionType: condition_type ?? undefined,
        threshold: threshold ? parseFloat(threshold) : undefined,
        targetDeviceId: target_device_id ?? undefined,
        actionCommand: action_command ?? undefined,
        enabled: enabled ?? undefined
      }
    });

    // Push updated schedule/rule config to Gateway via retained MQTT topic (FSD §4.8.3, §9.1)
    broker.publishScheduleConfig(gateway_id).catch((err) => {
      console.error('[RULE] Failed to publish config to gateway:', err.message);
    });

    res.json({ id: rule.id, message: 'Rule updated successfully' });
  } catch (err) {
    if (err.code === 'P2025') {
      throw createError('RULE_NOT_FOUND',
        `Rule with id '${req.params.id}' not found`);
    }
    next(err);
  }
}

/**
 * DELETE /gateways/{gateway_id}/rules/{id}
 */
async function deleteRule(req, res, next) {
  try {
    const { gateway_id, id } = req.params;

    await prisma.automationRule.delete({
      where: {
        id: parseInt(id),
        gatewayId: gateway_id
      }
    });

    // Push updated schedule/rule config to Gateway via retained MQTT topic (FSD §4.8.3, §9.1)
    broker.publishScheduleConfig(gateway_id).catch((err) => {
      console.error('[RULE] Failed to publish config to gateway:', err.message);
    });

    res.status(204).send();
  } catch (err) {
    if (err.code === 'P2025') {
      throw createError('RULE_NOT_FOUND',
        `Rule with id '${req.params.id}' not found`);
    }
    next(err);
  }
}

module.exports = {
  listRules,
  createRule,
  updateRule,
  deleteRule
};
