/**
 * Controllers for Automation Rule management.
 */
const { PrismaClient } = require('@prisma/client');
const prisma = new PrismaClient();
const { createError } = require('../middleware/errorHandler');

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

    const rules = await prisma.automationRule.findMany({
      where: { gatewayId: gateway_id },
      orderBy: { createdAt: 'desc' }
    });

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

    // Verify sensor device exists
    const sensor = await prisma.device.findFirst({
      where: { gatewayId: gateway_id, deviceId: sensor_device_id }
    });
    if (!sensor) {
      throw createError('DEVICE_NOT_FOUND',
        `Sensor device '${sensor_device_id}' not found in gateway '${gateway_id}'`);
    }

    // Verify target device exists
    const target = await prisma.device.findFirst({
      where: { gatewayId: gateway_id, deviceId: target_device_id }
    });
    if (!target) {
      throw createError('DEVICE_NOT_FOUND',
        `Target device '${target_device_id}' not found in gateway '${gateway_id}'`);
    }

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
