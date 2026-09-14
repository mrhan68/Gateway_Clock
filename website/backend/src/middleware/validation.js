/**
 * Validation middleware helpers using express-validator.
 */
const { body, param, query } = require('express-validator');

// ─── Reusable validation rule sets ───

// Gateway ID in path
const gatewayIdRules = [
  param('gateway_id')
    .trim()
    .isLength({ min: 1, max: 32 })
    .withMessage("gateway_id must be 1-32 characters")
    .matches(/^[a-zA-Z0-9_-]+$/)
    .withMessage("gateway_id can only contain letters, numbers, underscores, and hyphens")
];

// Device ID in path
const deviceIdRules = [
  param('device_id')
    .trim()
    .isLength({ min: 1, max: 32 })
    .withMessage("device_id must be 1-32 characters")
];

// Numeric route param
const numericIdRules = (name) => [
  param(name)
    .isInt({ min: 1 })
    .withMessage(`${name} must be a positive integer`)
];

// Device create / update body
const deviceRules = [
  body('device_id')
    .trim()
    .isLength({ min: 1, max: 32 })
    .withMessage('device_id must be 1-32 characters'),
  body('name')
    .trim()
    .isLength({ min: 1, max: 64 })
    .withMessage('name must be 1-64 characters'),
  body('device_type')
    .isIn(['sensor', 'actuator'])
    .withMessage('device_type must be sensor or actuator'),
  body('protocol')
    .isIn(['ble', 'mqtt'])
    .withMessage('protocol must be ble or mqtt'),
  body('mac_address')
    .optional({ nullable: true })
    .matches(/^[0-9A-Fa-f:]{17}$/)
    .withMessage('mac_address must be in format AA:BB:CC:DD:EE:FF'),
  body('mqtt_topic')
    .optional({ nullable: true })
    .isLength({ max: 128 })
    .withMessage('mqtt_topic must be ≤ 128 characters'),
  body('capabilities')
    .optional({ nullable: true })
    .isString()
    .withMessage('capabilities must be a JSON string')
];

// Schedule create / update body
const scheduleRules = [
  body('type')
    .isIn(['automation', 'alarm', 'personal_event'])
    .withMessage('type must be automation, alarm, or personal_event'),
  body('hour')
    .isInt({ min: 0, max: 23 })
    .withMessage('hour must be 0-23'),
  body('minute')
    .isInt({ min: 0, max: 59 })
    .withMessage('minute must be 0-59'),
  body('enabled')
    .optional()
    .isBoolean()
    .withMessage('enabled must be boolean'),
  body('repeat_days')
    .optional({ nullable: true })
    .isString()
    .withMessage('repeat_days must be a string'),
  body('one_time_date')
    .optional({ nullable: true })
    .isISO8601({ strict: true })
    .toDate()
    .withMessage('one_time_date must be a valid date'),
  body('title')
    .optional({ nullable: true })
    .isLength({ max: 64 })
    .withMessage('title must be ≤ 64 characters'),
  body('buzzer')
    .optional()
    .isBoolean()
    .withMessage('buzzer must be boolean'),
  body('target_device_id')
    .optional({ nullable: true })
    .isString(),
  body('action_command')
    .optional({ nullable: true })
    .isLength({ max: 32 })
];

// Automation rule create / update body
const ruleRules = [
  body('sensor_device_id')
    .trim()
    .isLength({ min: 1, max: 32 })
    .withMessage('sensor_device_id is required (1-32 chars)'),
  body('condition_type')
    .isIn(['gt', 'lt', 'eq'])
    .withMessage('condition_type must be gt, lt, or eq'),
  body('threshold')
    .isFloat()
    .withMessage('threshold must be a number'),
  body('target_device_id')
    .trim()
    .isLength({ min: 1, max: 32 })
    .withMessage('target_device_id is required (1-32 chars)'),
  body('action_command')
    .trim()
    .isLength({ min: 1, max: 32 })
    .withMessage('action_command is required (1-32 chars)'),
  body('enabled')
    .optional()
    .isBoolean()
    .withMessage('enabled must be boolean')
];

// Device command body
const commandRules = [
  body('command')
    .trim()
    .isLength({ min: 1, max: 32 })
    .withMessage('command must be 1-32 characters')
];

// Pagination query
const paginationRules = [
  query('limit')
    .optional()
    .isInt({ min: 1, max: 200 })
    .toInt(),
  query('offset')
    .optional()
    .isInt({ min: 0 })
    .toInt()
];

// Date range query
const dateRangeRules = [
  query('from')
    .optional()
    .isISO8601({ strict: true })
    .toDate(),
  query('to')
    .optional()
    .isISO8601({ strict: true })
    .toDate()
];

module.exports = {
  gatewayIdRules,
  deviceIdRules,
  numericIdRules,
  deviceRules,
  scheduleRules,
  ruleRules,
  commandRules,
  paginationRules,
  dateRangeRules
};
