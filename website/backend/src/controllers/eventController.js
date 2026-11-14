/**
 * Controllers for Event Log queries (Web App facing).
 */
const prisma = require('../config/prisma');
const { createError } = require('../middleware/errorHandler');

/**
 * GET /gateways/{gateway_id}/events
 *
 * List event logs for a gateway with optional filtering.
 */
async function listEvents(req, res, next) {
  try {
    const { gateway_id } = req.params;
    const { type, from, to, limit = 50, offset = 0 } = req.query;

    // Verify gateway
    const gateway = await prisma.gateway.findUnique({
      where: { id: gateway_id },
      select: { id: true }
    });
    if (!gateway) {
      throw createError('GATEWAY_NOT_FOUND',
        `Gateway '${gateway_id}' not found`);
    }

    const where = { gatewayId: gateway_id };
    if (type) where.eventType = type;
    if (from || to) {
      where.occurredAt = {};
      if (from) where.occurredAt.gte = new Date(from);
      if (to) where.occurredAt.lte = new Date(to);
    }

    const events = await prisma.eventLog.findMany({
      where,
      orderBy: { occurredAt: 'desc' },
      skip: parseInt(offset),
      take: parseInt(limit)
    });

    const formatted = events.map(e => {
      let payload = null;
      if (e.payload) {
        try {
          payload = JSON.parse(e.payload);
        } catch {
          payload = e.payload;
        }
      }
      return {
        id: e.id,
        gateway_id: e.gatewayId,
        event_type: e.eventType,
        device_id: e.deviceId,
        payload,
        occurred_at: e.occurredAt,
        synced_at: e.syncedAt
      };
    });

    res.json(formatted);
  } catch (err) {
    next(err);
  }
}

module.exports = {
  listEvents
};
