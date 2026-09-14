const express = require('express');
const router = express.Router({ mergeParams: true });
const syncController = require('../controllers/syncController');
const { gatewayIdRules, deviceIdRules } = require('../middleware/validation');
const { validationResult } = require('express-validator');

const validate = (req, res, next) => {
  const errors = validationResult(req);
  if (!errors.isEmpty()) return next(errors);
  next();
};

// GET /gateways/{gateway_id}/sync/schedule — Gateway pulls schedule + rules
router.get('/:gateway_id/sync/schedule', gatewayIdRules, validate, syncController.syncSchedule);

// GET /gateways/{gateway_id}/sync/devices — Gateway pulls device list
router.get('/:gateway_id/sync/devices', gatewayIdRules, validate, syncController.syncDevices);

// POST /gateways/{gateway_id}/sync/events — Gateway pushes event log
router.post('/:gateway_id/sync/events', gatewayIdRules, validate, syncController.syncEvents);

module.exports = router;
