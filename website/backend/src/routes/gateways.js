const express = require('express');
const router = express.Router();
const { body, validationResult } = require('express-validator');
const gatewayController = require('../controllers/gatewayController');
const { errorHandler } = require('../middleware/errorHandler');

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

// GET /gateways/{gateway_id}
router.get('/:gateway_id', gatewayController.getGateway);

// PUT /gateways/{gateway_id}
router.put('/:gateway_id', gatewayController.updateGateway);

module.exports = router;
