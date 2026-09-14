const express = require('express');
const router = express.Router({ mergeParams: true });
const eventController = require('../controllers/eventController');
const { gatewayIdRules, paginationRules, dateRangeRules } = require('../middleware/validation');
const { validationResult } = require('express-validator');

const validate = (req, res, next) => {
  const errors = validationResult(req);
  if (!errors.isEmpty()) return next(errors);
  next();
};

// GET /gateways/{gateway_id}/events
router.get('/:gateway_id/events',
  [...gatewayIdRules, ...dateRangeRules, ...paginationRules],
  validate,
  eventController.listEvents);

module.exports = router;
