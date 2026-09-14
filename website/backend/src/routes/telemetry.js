const express = require('express');
const router = express.Router({ mergeParams: true });
const telemetryController = require('../controllers/telemetryController');
const { gatewayIdRules, deviceIdRules, paginationRules, dateRangeRules } = require('../middleware/validation');
const { validationResult } = require('express-validator');

const validate = (req, res, next) => {
  const errors = validationResult(req);
  if (!errors.isEmpty()) return next(errors);
  next();
};

// GET /gateways/{gateway_id}/devices/{device_id}/telemetry
router.get('/:gateway_id/devices/:device_id/telemetry',
  [...gatewayIdRules, ...deviceIdRules, ...dateRangeRules, ...paginationRules],
  validate,
  telemetryController.getTelemetry);

module.exports = router;
