const express = require('express');
const router = express.Router({ mergeParams: true });
const ruleController = require('../controllers/ruleController');
const { ruleRules, gatewayIdRules, numericIdRules } = require('../middleware/validation');
const { validationResult } = require('express-validator');

const validate = (req, res, next) => {
  const errors = validationResult(req);
  if (!errors.isEmpty()) return next(errors);
  next();
};

// GET /gateways/{gateway_id}/rules
router.get('/:gateway_id/rules', gatewayIdRules, validate, ruleController.listRules);

// POST /gateways/{gateway_id}/rules
router.post('/:gateway_id/rules', [...gatewayIdRules, ...ruleRules], validate, ruleController.createRule);

// PUT /gateways/{gateway_id}/rules/{id}
router.put('/:gateway_id/rules/:id',
  [...gatewayIdRules, ...numericIdRules('id'), ...ruleRules], validate, ruleController.updateRule);

// DELETE /gateways/{gateway_id}/rules/{id}
router.delete('/:gateway_id/rules/:id',
  [...gatewayIdRules, numericIdRules('id')], validate, ruleController.deleteRule);

module.exports = router;
