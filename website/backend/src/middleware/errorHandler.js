/**
 * Error handling middleware for Express.
 * Produces consistent JSON error responses matching the FSD §8.9 format.
 */

// ─── Error code registry ───
const ERROR_CODES = {
  VALIDATION_ERROR: { status: 400, message: 'Request validation failed' },
  INVALID_DEVICE_PROTOCOL: { status: 400, message: 'Invalid device protocol' },
  GATEWAY_NOT_FOUND: { status: 404, message: 'Gateway not found' },
  DEVICE_NOT_FOUND: { status: 404, message: 'Device not found' },
  SCHEDULE_NOT_FOUND: { status: 404, message: 'Schedule not found' },
  RULE_NOT_FOUND: { status: 404, message: 'Rule not found' },
  DEVICE_ID_CONFLICT: { status: 409, message: 'Device ID already exists' },
  DEVICE_IN_USE: { status: 409, message: 'Device is referenced by an automation rule' },
  GATEWAY_OFFLINE: { status: 422, message: 'Gateway is currently offline' },
  UNAUTHORIZED: { status: 401, message: 'Unauthorized' },
  FORBIDDEN: { status: 403, message: 'Forbidden' },
  INTERNAL_ERROR: { status: 500, message: 'Internal server error' }
};

/**
 * Custom API Error class — carries a known error code.
 * Usage: throw new ApiError('DEVICE_NOT_FOUND', "Device 'foo' not found in gateway 'bar'")
 */
class ApiError extends Error {
  constructor(code, message) {
    const entry = ERROR_CODES[code] || ERROR_CODES.INTERNAL_ERROR;
    super(message || entry.message);
    this.code = code;
    this.status = entry.status;
  }
}

/**
 * Create an API error with a specific code and custom message.
 * @param {string} code - One of the keys in ERROR_CODES
 * @param {string} [message] - Override message (optional)
 */
function createError(code, message) {
  return new ApiError(code, message);
}

/**
 * Express error-handling middleware.
 * Must be the last app.use() call.
 */
function errorHandler(err, req, res, next) {
  // Already-structured API error
  if (err instanceof ApiError) {
    return res.status(err.status).json({
      error: {
        code: err.code,
        message: err.message,
        status: err.status
      }
    });
  }

  // Prisma / DB errors
  if (err.code === 'P2002') {
    return res.status(409).json({
      error: {
        code: 'DEVICE_ID_CONFLICT',
        message: 'A record with this field already exists',
        status: 409
      }
    });
  }

  if (err.code === 'P2025') {
    return res.status(404).json({
      error: {
        code: 'NOT_FOUND',
        message: err.meta?.cause || 'Record not found',
        status: 404
      }
    });
  }

  // Validation (express-validator) errors
  if (err.array && typeof err.array === 'function') {
    const firstErr = err.array()[0];
    return res.status(400).json({
      error: {
        code: 'VALIDATION_ERROR',
        message: firstErr.msg,
        status: 400,
        details: err.array()
      }
    });
  }

  // Fallback — log and return 500
  console.error('[ERROR]', err);
  res.status(500).json({
    error: {
      code: 'INTERNAL_ERROR',
      message: err.message || 'Something went wrong',
      status: 500
    }
  });
}

module.exports = {
  ApiError,
  createError,
  errorHandler,
  ERROR_CODES
};
