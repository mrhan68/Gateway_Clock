/**
 * Controllers for Device management and device commands.
 */
const { PrismaClient } = require('@prisma/client');
const prisma = new PrismaClient();
const { createError } = require('../middleware/errorHandler');
const relay = require('../mqtt/relay');

/**
 * GET /gateways/{gateway_id}/devices
 */
async function listDevices(req, res, next) {
  try {
    const { gateway_id } = req.params;

    // Verify gateway exists
    const gateway = await prisma.gateway.findUnique({
      where: { id: gateway_id },
      select: { id: true }
    });
    if (!gateway) {
      throw createError('GATEWAY_NOT_FOUND',
        `Gateway '${gateway_id}' not found`);
    }

    const devices = await prisma.device.findMany({
      where: { gatewayId: gateway_id },
      orderBy: { createdAt: 'desc' },
      select: {
        id: true,
        deviceId: true,
        name: true,
        deviceType: true,
        protocol: true,
        macAddress: true,
        mqttTopic: true,
        capabilities: true,
        status: true,
        lastSeen: true,
        lastValue: true,
        createdAt: true
      }
    });

    // Format for frontend (camelCase)
    const formatted = devices.map(d => ({
      id: d.id,
      device_id: d.deviceId,
      name: d.name,
      device_type: d.deviceType,
      protocol: d.protocol,
      mac_address: d.macAddress,
      mqtt_topic: d.mqttTopic,
      capabilities: d.capabilities ? JSON.parse(d.capabilities) : [],
      status: d.status,
      last_seen: d.lastSeen,
      last_value: d.lastValue,
      created_at: d.createdAt
    }));

    res.json(formatted);
  } catch (err) {
    next(err);
  }
}

/**
 * POST /gateways/{gateway_id}/devices
 */
async function createDevice(req, res, next) {
  try {
    const { gateway_id } = req.params;
    const { device_id, name, device_type, protocol, mac_address, mqtt_topic, capabilities } = req.body;

    // Verify gateway exists
    const gateway = await prisma.gateway.findUnique({
      where: { id: gateway_id },
      select: { id: true }
    });
    if (!gateway) {
      throw createError('GATEWAY_NOT_FOUND',
        `Gateway '${gateway_id}' not found`);
    }

    // Check if device_id already exists
    const existing = await prisma.device.findFirst({
      where: { gatewayId: gateway_id, deviceId: device_id }
    });
    if (existing) {
      throw createError('DEVICE_ID_CONFLICT',
        `Device '${device_id}' already exists in gateway '${gateway_id}'`);
    }

    // Validate protocol-specific fields
    if (protocol === 'ble' && !mac_address) {
      return res.status(400).json({
        error: {
          code: 'VALIDATION_ERROR',
          message: 'mac_address is required for BLE devices',
          status: 400
        }
      });
    }
    if (protocol === 'mqtt' && !mqtt_topic) {
      return res.status(400).json({
        error: {
          code: 'VALIDATION_ERROR',
          message: 'mqtt_topic is required for MQTT devices',
          status: 400
        }
      });
    }

    const device = await prisma.device.create({
      data: {
        deviceId: device_id,
        gatewayId: gateway_id,
        name,
        deviceType: device_type,
        protocol,
        macAddress: mac_address || null,
        mqttTopic: mqtt_topic || null,
        capabilities: capabilities ? JSON.stringify(capabilities) : null
      }
    });

    res.status(201).json({
      id: device.id,
      device_id: device.deviceId,
      message: 'Device created successfully'
    });
  } catch (err) {
    next(err);
  }
}

/**
 * PUT /gateways/{gateway_id}/devices/{device_id}
 */
async function updateDevice(req, res, next) {
  try {
    const { gateway_id, device_id } = req.params;
    const { name, device_type, protocol, mac_address, mqtt_topic, capabilities } = req.body;

    const device = await prisma.device.update({
      where: {
        gatewayId_deviceId: {
          gatewayId: gateway_id,
          deviceId: device_id
        }
      },
      data: {
        name: name ?? undefined,
        deviceType: device_type ?? undefined,
        protocol: protocol ?? undefined,
        macAddress: mac_address ?? undefined,
        mqttTopic: mqtt_topic ?? undefined,
        capabilities: capabilities ? JSON.stringify(capabilities) : undefined
      }
    });

    if (!device) {
      throw createError('DEVICE_NOT_FOUND',
        `Device '${device_id}' not found in gateway '${gateway_id}'`);
    }

    res.json({ id: device.id, device_id: device.deviceId });
  } catch (err) {
    next(err);
  }
}

/**
 * DELETE /gateways/{gateway_id}/devices/{device_id}
 */
async function deleteDevice(req, res, next) {
  try {
    const { gateway_id, device_id } = req.params;

    // Check if device is referenced by automation rules or schedules
    const ruleRefs = await prisma.automationRule.count({
      where: {
        gatewayId: gateway_id,
        OR: [
          { sensorDeviceId: device_id },
          { targetDeviceId: device_id }
        ]
      }
    });

    if (ruleRefs > 0) {
      // Cascade delete — FSD says "delete device + related rules"
      await prisma.automationRule.deleteMany({
        where: {
          gatewayId: gateway_id,
          OR: [
            { sensorDeviceId: device_id },
            { targetDeviceId: device_id }
          ]
        }
      });
    }

    await prisma.device.delete({
      where: {
        gatewayId_deviceId: {
          gatewayId: gateway_id,
          deviceId: device_id
        }
      }
    });

    res.status(204).send();
  } catch (err) {
    if (err.code === 'P2025') {
      throw createError('DEVICE_NOT_FOUND',
        `Device '${device_id}' not found in gateway '${gateway_id}'`);
    }
    next(err);
  }
}

const crypto = require('crypto');

/**
 * POST /gateways/{gateway_id}/devices/{device_id}/command (FSD §8.3)
 * Sends a command to a device via MQTT → Gateway.
 */
async function sendDeviceCommand(req, res, next) {
  try {
    const { gateway_id, device_id } = req.params;
    const { command, ttl_seconds } = req.body;
    const commandId = crypto.randomUUID();

    // Verify device exists
    const device = await prisma.device.findFirst({
      where: { gatewayId: gateway_id, deviceId: device_id },
      select: { id: true, deviceId: true, protocol: true }
    });
    if (!device) {
      throw createError('DEVICE_NOT_FOUND',
        `Device '${device_id}' not found in gateway '${gateway_id}'`);
    }

    // Relay command via MQTT with FSD §9.2 format
    const result = await relay.sendCommand({
      gatewayId: gateway_id,
      deviceId: device_id,
      command,
      commandId,
      ttlSeconds: ttl_seconds || 60
    });

    if (!result.success) {
      if (result.offline) {
        throw createError('GATEWAY_OFFLINE', result.message);
      }
      throw createError('INTERNAL_ERROR', result.message);
    }

    res.status(202).json({
      command_id: commandId,
      status: 'QUEUED',
      message: result.message
    });
  } catch (err) {
    next(err);
  }
}

module.exports = {
  listDevices,
  createDevice,
  updateDevice,
  deleteDevice,
  sendDeviceCommand
};
