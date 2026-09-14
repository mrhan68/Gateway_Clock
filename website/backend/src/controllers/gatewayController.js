/**
 * Controllers for Gateway management.
 */
const { PrismaClient } = require('@prisma/client');
const prisma = new PrismaClient();
const { createError } = require('../middleware/errorHandler');

/**
 * GET /gateways/{gateway_id}
 */
async function getGateway(req, res, next) {
  try {
    const { gateway_id } = req.params;
    const gateway = await prisma.gateway.findUnique({
      where: { id: gateway_id },
      include: {
        _count: {
          select: { devices: true, schedules: true, automationRules: true }
        }
      }
    });

    if (!gateway) {
      throw createError('GATEWAY_NOT_FOUND',
        `Gateway '${gateway_id}' not found`);
    }

    const { _count, ...gatewayData } = gateway;
    res.json({ ...gatewayData, deviceCount: _count.devices });
  } catch (err) {
    next(err);
  }
}

/**
 * PUT /gateways/{gateway_id}
 */
async function updateGateway(req, res, next) {
  try {
    const { gateway_id } = req.params;
    const { name, timezone, firmware_version } = req.body;

    // Check existence first
    const existing = await prisma.gateway.findUnique({
      where: { id: gateway_id },
      select: { id: true }
    });
    if (!existing) {
      throw createError('GATEWAY_NOT_FOUND',
        `Gateway '${gateway_id}' not found`);
    }

    const updated = await prisma.gateway.update({
      where: { id: gateway_id },
      data: {
        name: name ?? undefined,
        timezone: timezone ?? undefined,
        firmwareVersion: firmware_version ?? undefined
      }
    });

    res.json(updated);
  } catch (err) {
    next(err);
  }
}

/**
 * POST /gateways  — Register a new gateway
 */
async function createGateway(req, res, next) {
  try {
    const { id, name, timezone } = req.body;

    if (!id || !name) {
      return res.status(400).json({
        error: {
          code: 'VALIDATION_ERROR',
          message: 'Fields id and name are required',
          status: 400
        }
      });
    }

    const gateway = await prisma.gateway.create({
      data: {
        id,
        name,
        timezone: timezone || null
      }
    });

    res.status(201).json({
      id: gateway.id,
      name: gateway.name,
      message: 'Gateway registered successfully'
    });
  } catch (err) {
    if (err.code === 'P2002') {
      return res.status(409).json({
        error: {
          code: 'DEVICE_ID_CONFLICT',
          message: `Gateway '${req.body.id}' already exists`,
          status: 409
        }
      });
    }
    next(err);
  }
}

/**
 * GET /gateways  — List gateways (if user is set)
 */
async function listGateways(req, res, next) {
  try {
    const gateways = await prisma.gateway.findMany({
      orderBy: { createdAt: 'desc' },
      select: {
        id: true,
        name: true,
        status: true,
        lastSeen: true,
        firmwareVersion: true,
        timezone: true,
        createdAt: true
      }
    });
    res.json(gateways);
  } catch (err) {
    next(err);
  }
}

module.exports = {
  getGateway,
  updateGateway,
  createGateway,
  listGateways
};
