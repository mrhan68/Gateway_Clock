const express = require('express');
const router = express.Router({ mergeParams: true });
const scheduleController = require('../controllers/scheduleController');
const { scheduleRules, gatewayIdRules, numericIdRules, ScheduleToggle } = require('../middleware/validation');
const { body } = require('express-validator');
const { validationResult } = require('express-validator');

const validate = (req, res, next) => {
  const errors = validationResult(req);
  if (!errors.isEmpty()) return next(errors);
  next();
};

// GET /gateways/{gateway_id}/schedules?type=...
router.get('/:gateway_id/schedules', gatewayIdRules, validate, scheduleController.listSchedules);

// POST /gateways/{gateway_id}/schedules
router.post('/:gateway_id/schedules', [...gatewayIdRules, scheduleRules], validate, scheduleController.createSchedule);

// PUT /gateways/{gateway_id}/schedules/{id}
router.put('/:gateway_id/schedules/:id',
  [...gatewayIdRules, numericIdRules('id'), scheduleRules], validate, scheduleController.updateSchedule);

// DELETE /gateways/{gateway_id}/schedules/{id}
router.delete('/:gateway_id/schedules/:id',
  [...gatewayIdRules, numericIdRules('id')], validate, scheduleController.deleteSchedule);

// PATCH /gateways/{gateway_id}/schedules/{id}/toggle
router.patch('/:gateway_id/schedules/:id/toggle',
  [...gatewayIdRules, numericIdRules('id'),
    body('enabled').isBoolean().withMessage('enabled must be boolean')],
  validate, scheduleController.toggleSchedule);

module.exports = router;
