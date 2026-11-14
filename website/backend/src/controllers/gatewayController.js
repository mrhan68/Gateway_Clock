/**
 * Controllers for Gateway management.
 */
const prisma = require('../config/prisma');
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
    res.json({ ...gatewayData, deviceCount: _count ? (_count.devices ?? 0) : 0 });
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

    // Check existence and ownership
    const existing = await prisma.gateway.findUnique({
      where: { id: gateway_id },
      select: { id: true, userId: true }
    });
    if (!existing) {
      throw createError('GATEWAY_NOT_FOUND',
        `Gateway '${gateway_id}' not found`);
    }
    if (req.user?.id && existing.userId && existing.userId !== req.user.id) {
      throw createError('FORBIDDEN', 'Access to this gateway is denied');
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
        timezone: timezone || null,
        userId: req.user?.id || null
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
    const where = req.user?.id ? { OR: [{ userId: req.user.id }, { userId: null }] } : {};
    const gateways = await prisma.gateway.findMany({
      where,
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

/**
 * POST /gateways/register-claim (FSD §8.2)
 * Gateway registers its hardware MAC and claim code upon initial boot.
 */
async function registerClaim(req, res, next) {
  try {
    let { mac_address, claim_code, firmware_version, gateway_id } = req.body;
    const now = new Date();
    const expiresAt = new Date(now.getTime() + 24 * 60 * 60 * 1000); // TTL 24 hours

    const cleanMac = mac_address ? mac_address.replace(/[^a-zA-Z0-9]/g, '').toLowerCase() : 'unknown';
    const targetId = gateway_id || `gw_${cleanMac.slice(-8)}`;

    if (!claim_code) {
      claim_code = `GW-${Math.random().toString(36).substring(2, 8).toUpperCase()}`;
    }

    let gateway = await prisma.gateway.findFirst({
      where: {
        OR: [
          ...(mac_address ? [{ macAddress: mac_address }] : []),
          { id: targetId }
        ]
      }
    });

    if (gateway) {
      if (gateway.status === 'active' || gateway.status === 'online') {
        return res.status(200).json({
          status: gateway.status.toUpperCase(),
          gateway_id: gateway.id,
          message: 'Gateway already claimed and active'
        });
      }

      gateway = await prisma.gateway.update({
        where: { id: gateway.id },
        data: {
          claimCode: claim_code,
          claimExpiresAt: expiresAt,
          firmwareVersion: firmware_version || gateway.firmwareVersion,
          status: 'unclaimed',
          macAddress: mac_address,
          lastSeen: now
        }
      });
    } else {
      gateway = await prisma.gateway.create({
        data: {
          id: targetId,
          name: `Gateway ${cleanMac.slice(-4).toUpperCase()}`,
          macAddress: mac_address,
          claimCode: claim_code,
          claimExpiresAt: expiresAt,
          firmwareVersion: firmware_version || '1.0.0',
          status: 'unclaimed',
          lastSeen: now
        }
      });
    }

    res.status(200).json({
      status: 'UNCLAIMED',
      gateway_id: gateway.id,
      claim_code: gateway.claimCode,
      expires_at: gateway.claimExpiresAt.toISOString(),
      message: 'Gateway claim registered successfully'
    });
  } catch (err) {
    next(err);
  }
}

/**
 * POST /gateways/claim (FSD §8.2)
 * User enters claim code in Web App to pair gateway to account.
 */
async function claimGateway(req, res, next) {
  try {
    const { claim_code, name } = req.body;
    const now = new Date();

    const gateway = await prisma.gateway.findFirst({
      where: {
        claimCode: claim_code,
        status: 'unclaimed',
        claimExpiresAt: { gte: now }
      }
    });

    if (!gateway) {
      return res.status(404).json({
        error: {
          code: 'CLAIM_CODE_INVALID',
          message: 'Claim code is invalid or has expired (TTL 24h)',
          status: 404
        }
      });
    }

    const updated = await prisma.gateway.update({
      where: { id: gateway.id },
      data: {
        name: name || gateway.name,
        userId: req.user?.id || gateway.userId,
        status: 'active',
        claimCode: null,
        claimExpiresAt: null,
        lastSeen: now
      }
    });

    res.status(200).json({
      gateway_id: updated.id,
      name: updated.name,
      message: 'Gateway claimed successfully'
    });
  } catch (err) {
    next(err);
  }
}

module.exports = {
  getGateway,
  updateGateway,
  createGateway,
  listGateways,
  registerClaim,
  claimGateway
};

