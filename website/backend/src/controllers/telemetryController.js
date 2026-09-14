/**
 * Controllers for Telemetry queries (Web App facing).
 */
const { PrismaClient } = require('@prisma/client');
const prisma = new PrismaClient();
const { createError } = require('../middleware/errorHandler');

/**
 * GET /gateways/{gateway_id}/devices/{device_id}/telemetry
 *
 * Query historical telemetry for a device.
 */
async function getTelemetry(req, res, next) {
  try {
    const { gateway_id, device_id } = req.params;
    const { from, to, limit = 100, offset = 0 } = req.query;

    // Verify device exists
    const device = await prisma.device.findFirst({
      where: { gatewayId: gateway_id, deviceId: device_id },
      select: { id: true }
    });
    if (!device) {
      throw createError('DEVICE_NOT_FOUND',
        `Device '${device_id}' not found in gateway '${gateway_id}'`);
    }

    const where = {
      deviceId: device_id,
      gatewayId: gateway_id
    };
    if (from || to) {
      where.recordedAt = {};
      if (from) where.recordedAt.gte = new Date(from);
      if (to) where.recordedAt.lte = new Date(to);
    }

    const logs = await prisma.telemetryLog.findMany({
      where,
      orderBy: { recordedAt: 'desc' },
      skip: parseInt(offset),
      take: parseInt(limit)
    });

    const formatted = logs.map(l => ({
      id: l.id,
      device_id: l.deviceId,
      value: l.value,
      unit: l.unit,
      recorded_at: l.recordedAt
    }));

    res.json(formatted);
  } catch (err) {
    next(err);
  }
}

module.exports = {
  getTelemetry
};
