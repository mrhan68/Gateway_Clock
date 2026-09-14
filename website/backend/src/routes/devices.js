const express = require('express');
const router = express.Router({ mergeParams: true });
const deviceController = require('../controllers/deviceController');
const { deviceRules, commandRules, gatewayIdRules, deviceIdRules } = require('../middleware/validation');
const { validationResult } = require('express-validator');

// Validation middleware helper
const validate = (req, res, next) => {
  const errors = validationResult(req);
  if (!errors.isEmpty()) return next(errors);
  next();
};

// GET /gateways/{gateway_id}/devices
router.get('/:gateway_id/devices', gatewayIdRules, validate, deviceController.listDevices);

// POST /gateways/{gateway_id}/devices
router.post('/:gateway_id/devices', [...gatewayIdRules, deviceRules], validate, deviceController.createDevice);

// PUT /gateways/{gateway_id}/devices/{device_id}
router.put('/:gateway_id/devices/:device_id',
  [...gatewayIdRules, ...deviceIdRules, deviceRules], validate, deviceController.updateDevice);

// DELETE /gateways/{gateway_id}/devices/{device_id}
router.delete('/:gateway_id/devices/:device_id',
  [...gatewayIdRules, ...deviceIdRules], validate, deviceController.deleteDevice);

// POST /gateways/{gateway_id}/devices/{device_id}/command
router.post('/:gateway_id/devices/:device_id/command',
  [...gatewayIdRules, ...deviceIdRules, commandRules], validate, deviceController.sendDeviceCommand);

module.exports = router;
