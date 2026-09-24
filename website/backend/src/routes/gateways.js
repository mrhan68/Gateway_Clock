const express = require('express');
const router = express.Router();
const { body, validationResult } = require('express-validator');
const gatewayController = require('../controllers/gatewayController');

// GET /gateways — list all gateways
router.get('/', gatewayController.listGateways);

// POST /gateways — register a new gateway
router.post('/', [
  body('id').trim().isLength({ min: 1, max: 32 }).matches(/^[a-zA-Z0-9_-]+$/),
  body('name').trim().isLength({ min: 1, max: 64 }),
  body('timezone').optional().isString(),
  (req, res, next) => {
    const errors = validationResult(req);
    if (!errors.isEmpty()) {
      return next(errors);
    }
    next();
  }
], gatewayController.createGateway);

// POST /gateways/register-claim — Gateway registers initial claim code (FSD §8.2)
router.post('/register-claim', [
  body('mac_address').trim().isLength({ min: 11, max: 18 }),
  body('claim_code').optional().trim().isLength({ min: 4, max: 16 }),
  body('gateway_id').optional().trim(),
  body('firmware_version').optional().isString(),
  (req, res, next) => {
    const errors = validationResult(req);
    if (!errors.isEmpty()) return next(errors);
    next();
  }
], gatewayController.registerClaim);

// POST /gateways/claim — User claims gateway from Web App (FSD §8.2)
router.post('/claim', [
  body('claim_code').trim().isLength({ min: 4, max: 16 }),
  body('name').trim().isLength({ min: 1, max: 64 }),
  (req, res, next) => {
    const errors = validationResult(req);
    if (!errors.isEmpty()) return next(errors);
    next();
  }
], gatewayController.claimGateway);

// GET /gateways/{gateway_id}
router.get('/:gateway_id', gatewayController.getGateway);

// PUT /gateways/{gateway_id}
router.put('/:gateway_id', gatewayController.updateGateway);

module.exports = router;
