/**
 * Controllers for Schedule management.
 *
 * Schedules can be of type: automation, alarm, or personal_event.
 * Per FSD §7.2.4: repeat_days and one_time_date are mutually exclusive.
 */
const { PrismaClient } = require('@prisma/client');
const prisma = new PrismaClient();
const { createError } = require('../middleware/errorHandler');
const broker = require('../mqtt/broker');

/**
 * GET /gateways/{gateway_id}/schedules?type=...
 */
async function listSchedules(req, res, next) {
  try {
    const { gateway_id } = req.params;
    const { type } = req.query;

    // Verify gateway
    const gateway = await prisma.gateway.findUnique({
      where: { id: gateway_id },
      select: { id: true }
    });
    if (!gateway) {
      throw createError('GATEWAY_NOT_FOUND',
        `Gateway '${gateway_id}' not found`);
    }

    const where = {
      gatewayId: gateway_id,
      ...(type ? { type: type.toLowerCase() } : {})
    };

    const schedules = await prisma.schedule.findMany({
      where,
      orderBy: [{ type: 'asc' }, { hour: 'asc' }, { minute: 'asc' }]
    });

    const formatted = schedules.map(s => ({
      id: s.id,
      gateway_id: s.gatewayId,
      type: s.type,
      hour: s.hour,
      minute: s.minute,
      repeat_days: s.repeatDays,
      one_time_date: s.oneTimeDate ? s.oneTimeDate.toISOString().split('T')[0] : null,
      enabled: s.enabled,
      target_device_id: s.targetDeviceId,
      action_command: s.actionCommand,
      title: s.title,
      buzzer: s.buzzer,
      created_at: s.createdAt,
      updated_at: s.updatedAt
    }));

    res.json(formatted);
  } catch (err) {
    next(err);
  }
}

/**
 * POST /gateways/{gateway_id}/schedules
 */
async function createSchedule(req, res, next) {
  try {
    const { gateway_id } = req.params;
    const {
      type, hour, minute, enabled = true,
      repeat_days, one_time_date,
      target_device_id, action_command,
      title, buzzer = false
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

    // Validate: repeat_days and one_time_date are mutually exclusive
    if (repeat_days && one_time_date) {
      return res.status(400).json({
        error: {
          code: 'VALIDATION_ERROR',
          message: "Fields 'repeat_days' and 'one_time_date' are mutually exclusive. Provide only one.",
          status: 400
        }
      });
    }

    // Validate automation requires target + action
    if (type === 'automation' && (!target_device_id || !action_command)) {
      return res.status(400).json({
        error: {
          code: 'VALIDATION_ERROR',
          message: "Automation type requires target_device_id and action_command",
          status: 400
        }
      });
    }

    const schedule = await prisma.schedule.create({
      data: {
        gatewayId: gateway_id,
        type: type.toLowerCase(),
        hour,
        minute,
        enabled,
        repeatDays: repeat_days || null,
        oneTimeDate: one_time_date ? new Date(one_time_date) : null,
        targetDeviceId: target_device_id || null,
        actionCommand: action_command || null,
        title: title || null,
        buzzer
      }
    });

    // Push updated schedule config to Gateway via retained MQTT topic (FSD §4.8.3, §9.1)
    broker.publishScheduleConfig(gateway_id).catch((err) => {
      console.error('[SCHEDULE] Failed to publish config to gateway:', err.message);
    });

    res.status(201).json({
      id: schedule.id,
      message: 'Schedule created successfully'
    });
  } catch (err) {
    next(err);
  }
}

/**
 * PUT /gateways/{gateway_id}/schedules/{id}
 */
async function updateSchedule(req, res, next) {
  try {
    const { gateway_id, id } = req.params;
    const {
      type, hour, minute, enabled,
      repeat_days, one_time_date,
      target_device_id, action_command,
      title, buzzer
    } = req.body;

    const schedule = await prisma.schedule.update({
      where: {
        id: parseInt(id),
        gatewayId: gateway_id
      },
      data: {
        type: type ? type.toLowerCase() : undefined,
        hour: hour ?? undefined,
        minute: minute ?? undefined,
        enabled: enabled ?? undefined,
        repeatDays: repeat_days !== undefined ? (repeat_days || null) : undefined,
        oneTimeDate: one_time_date !== undefined ? (one_time_date ? new Date(one_time_date) : null) : undefined,
        targetDeviceId: target_device_id !== undefined ? (target_device_id || null) : undefined,
        actionCommand: action_command !== undefined ? (action_command || null) : undefined,
        title: title !== undefined ? (title || null) : undefined,
        buzzer: buzzer ?? undefined
      }
    });

    if (!schedule) {
      throw createError('SCHEDULE_NOT_FOUND',
        `Schedule with id '${id}' not found for gateway '${gateway_id}'`);
    }

    // Push updated schedule config to Gateway via retained MQTT topic (FSD §4.8.3, §9.1)
    broker.publishScheduleConfig(gateway_id).catch((err) => {
      console.error('[SCHEDULE] Failed to publish config to gateway:', err.message);
    });

    res.json({ id: schedule.id, message: 'Schedule updated successfully' });
  } catch (err) {
    if (err.code === 'P2025') {
      throw createError('SCHEDULE_NOT_FOUND',
        `Schedule with id '${req.params.id}' not found`);
    }
    next(err);
  }
}

/**
 * DELETE /gateways/{gateway_id}/schedules/{id}
 */
async function deleteSchedule(req, res, next) {
  try {
    const { gateway_id, id } = req.params;

    await prisma.schedule.delete({
      where: {
        id: parseInt(id),
        gatewayId: gateway_id
      }
    });

    // Push updated schedule config to Gateway via retained MQTT topic (FSD §4.8.3, §9.1)
    broker.publishScheduleConfig(gateway_id).catch((err) => {
      console.error('[SCHEDULE] Failed to publish config to gateway:', err.message);
    });

    res.status(204).send();
  } catch (err) {
    if (err.code === 'P2025') {
      throw createError('SCHEDULE_NOT_FOUND',
        `Schedule with id '${req.params.id}' not found`);
    }
    next(err);
  }
}

/**
 * PATCH /gateways/{gateway_id}/schedules/{id}/toggle
 */
async function toggleSchedule(req, res, next) {
  try {
    const { gateway_id, id } = req.params;
    const { enabled } = req.body;

    const schedule = await prisma.schedule.update({
      where: {
        id: parseInt(id),
        gatewayId: gateway_id
      },
      data: { enabled }
    });

    if (!schedule) {
      throw createError('SCHEDULE_NOT_FOUND',
        `Schedule with id '${id}' not found for gateway '${gateway_id}'`);
    }

    // Push updated schedule config to Gateway via retained MQTT topic (FSD §4.8.3, §9.1)
    broker.publishScheduleConfig(gateway_id).catch((err) => {
      console.error('[SCHEDULE] Failed to publish config to gateway:', err.message);
    });

    res.json({
      id: schedule.id,
      enabled: schedule.enabled,
      message: `Schedule ${schedule.enabled ? 'enabled' : 'disabled'}`
    });
  } catch (err) {
    if (err.code === 'P2025') {
      throw createError('SCHEDULE_NOT_FOUND',
        `Schedule with id '${req.params.id}' not found`);
    }
    next(err);
  }
}

module.exports = {
  listSchedules,
  createSchedule,
  updateSchedule,
  deleteSchedule,
  toggleSchedule
};
