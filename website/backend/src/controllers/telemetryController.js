/**
 * Controllers for Telemetry queries (Web App facing).
 */
const prisma = require('../config/prisma');
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

/**
 * POST /gateways/{gateway_id}/telemetry/backfill
 *
 * Batch backfill telemetry records from Gateway (offline store-and-forward recovery).
 * Per FSD §4.11.3 & §9.1: Deduplicates based on (deviceId, gatewayId, recordedAt).
 */
async function backfillTelemetry(req, res, next) {
  try {
    const { gateway_id } = req.params;
    const records = Array.isArray(req.body)
      ? req.body
      : (req.body.records || req.body.batch || []);

    // Verify gateway exists
    const gateway = await prisma.gateway.findUnique({
      where: { id: gateway_id },
      select: { id: true }
    });
    if (!gateway) {
      throw createError('GATEWAY_NOT_FOUND', `Gateway '${gateway_id}' not found`);
    }

    let insertedCount = 0;
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
          gatewayId: gateway_id,
          deviceId: devId,
          recordedAt
        }
      });

      if (!existing) {
        await prisma.telemetryLog.create({
          data: {
            gatewayId: gateway_id,
            deviceId: devId,
            value: typeof val === 'number' ? val : parseFloat(val),
            unit: rec.unit || null,
            recordedAt
          }
        });
        insertedCount++;
      }
    }

    res.status(200).json({
      success: true,
      total_received: records.length,
      inserted_count: insertedCount,
      deduplicated_count: records.length - insertedCount,
      message: `Backfill processed: ${insertedCount} inserted, ${records.length - insertedCount} deduplicated`
    });
  } catch (err) {
    next(err);
  }
}

module.exports = {
  getTelemetry,
  backfillTelemetry
};

