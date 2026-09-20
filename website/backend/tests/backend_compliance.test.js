jest.mock('aedes', () => {
  const { EventEmitter } = require('events');
  return {
    Aedes: jest.fn().mockImplementation(() => {
      const emitter = new EventEmitter();
      emitter.listen = jest.fn().mockResolvedValue();
      emitter.handle = jest.fn();
      emitter.publish = jest.fn((packet, cb) => cb && cb(null));
      emitter.close = jest.fn((cb) => cb && cb());
      return emitter;
    })
  };
});

// Mock PrismaClient to test logic without live DB
jest.mock('@prisma/client', () => {
  const mockGateway = {
    id: 'gw_a1b2c3d4',
    name: 'Đồng hồ Test',
    status: 'unclaimed',
    claimCode: 'GW-123456',
    claimExpiresAt: new Date(Date.now() + 86400000),
    macAddress: 'AA:BB:CC:DD:EE:FF',
    createdAt: new Date(),
    updatedAt: new Date()
  };

  const mockPrisma = {
    $connect: jest.fn().mockResolvedValue(),
    $disconnect: jest.fn().mockResolvedValue(),
    gateway: {
      findUnique: jest.fn().mockResolvedValue(mockGateway),
      findFirst: jest.fn().mockResolvedValue(mockGateway),
      findMany: jest.fn().mockResolvedValue([mockGateway]),
      create: jest.fn().mockResolvedValue(mockGateway),
      update: jest.fn().mockResolvedValue({ ...mockGateway, status: 'active', claimCode: null }),
      updateMany: jest.fn().mockResolvedValue({ count: 1 })
    },
    device: {
      findFirst: jest.fn().mockResolvedValue({
        id: 1,
        deviceId: 'dev_01',
        gatewayId: 'gw_a1b2c3d4',
        name: 'Relay 1',
        deviceType: 'actuator',
        protocol: 'ble',
        status: 'online'
      }),
      findMany: jest.fn().mockResolvedValue([]),
      create: jest.fn().mockResolvedValue({ id: 2, deviceId: 'dev_02' }),
      update: jest.fn().mockResolvedValue({ id: 1, deviceId: 'dev_01' }),
      delete: jest.fn().mockResolvedValue({ id: 1 }),
      updateMany: jest.fn().mockResolvedValue({ count: 1 })
    },
    schedule: {
      findMany: jest.fn().mockResolvedValue([
        {
          id: 101,
          gatewayId: 'gw_a1b2c3d4',
          type: 'automation',
          hour: 7,
          minute: 0,
          repeatDays: 'MON,TUE,WED',
          oneTimeDate: null,
          enabled: true,
          targetDeviceId: 'dev_01',
          actionCommand: 'ON',
          title: null,
          buzzer: false
        }
      ]),
      create: jest.fn().mockResolvedValue({ id: 102 }),
      update: jest.fn().mockResolvedValue({ id: 101, enabled: true }),
      delete: jest.fn().mockResolvedValue({ id: 101 })
    },
    automationRule: {
      findMany: jest.fn().mockResolvedValue([
        {
          id: 1,
          gatewayId: 'gw_a1b2c3d4',
          sensorDeviceId: 'sensor_01',
          conditionType: 'gt',
          threshold: 30.0,
          targetDeviceId: 'dev_01',
          actionCommand: 'ON',
          enabled: true
        }
      ]),
      create: jest.fn().mockResolvedValue({ id: 2 }),
      update: jest.fn().mockResolvedValue({ id: 1 }),
      delete: jest.fn().mockResolvedValue({ id: 1 }),
      count: jest.fn().mockResolvedValue(0)
    },
    eventLog: {
      findMany: jest.fn().mockResolvedValue([]),
      create: jest.fn().mockResolvedValue({ id: 1 })
    },
    telemetryLog: {
      findMany: jest.fn().mockResolvedValue([]),
      findFirst: jest.fn().mockResolvedValue(null),
      create: jest.fn().mockResolvedValue({ id: 1 })
    }
  };

  return {
    PrismaClient: jest.fn().mockImplementation(() => mockPrisma)
  };
});

const request = require('supertest');
const app = require('../src/app');
const broker = require('../src/mqtt/broker');
const { createError, errorHandler, ApiError } = require('../src/middleware/errorHandler');

describe('FSD Backend Compliance Tests', () => {

  describe('1. Health Check & Error Handling (§8.9)', () => {
    test('GET /health returns 200 with status ok and timestamp', async () => {
      const res = await request(app).get('/health');
      expect(res.statusCode).toBe(200);
      expect(res.body.status).toBe('ok');
      expect(res.body.timestamp).toBeDefined();
    });

    test('ApiError returns consistent JSON structure per §8.9', () => {
      const err = createError('DEVICE_NOT_FOUND', 'Device sensor_99 not found');
      expect(err).toBeInstanceOf(ApiError);
      expect(err.code).toBe('DEVICE_NOT_FOUND');
      expect(err.status).toBe(404);
      expect(err.message).toBe('Device sensor_99 not found');
    });

    test('ErrorHandler formats custom ApiError correctly', () => {
      const req = {};
      const res = {
        statusCode: 0,
        body: null,
        status(code) {
          this.statusCode = code;
          return this;
        },
        json(data) {
          this.body = data;
          return this;
        }
      };
      const next = jest.fn();

      const apiErr = createError('GATEWAY_NOT_FOUND', 'Gateway gw_001 not found');
      errorHandler(apiErr, req, res, next);

      expect(res.statusCode).toBe(404);
      expect(res.body).toEqual({
        error: {
          code: 'GATEWAY_NOT_FOUND',
          message: 'Gateway gw_001 not found',
          status: 404
        }
      });
    });
  });

  describe('2. Gateway Claim Handshake (§8.2)', () => {
    test('POST /api/v1/gateways/register-claim succeeds with valid MAC', async () => {
      const res = await request(app)
        .post('/api/v1/gateways/register-claim')
        .send({
          mac_address: 'AA:BB:CC:DD:EE:FF',
          claim_code: 'GW-9821A0',
          firmware_version: '1.0.0'
        });

      expect(res.statusCode).toBe(200);
      expect(res.body.status).toBeDefined();
    });

    test('POST /api/v1/gateways/claim validates required claim_code and name', async () => {
      const res = await request(app)
        .post('/api/v1/gateways/claim')
        .send({});

      expect(res.statusCode).toBe(400);
      expect(res.body.error).toBeDefined();
      expect(res.body.error.code).toBe('VALIDATION_ERROR');
    });

    test('POST /api/v1/gateways/claim pairs gateway successfully', async () => {
      const res = await request(app)
        .post('/api/v1/gateways/claim')
        .send({
          claim_code: 'GW-9821A0',
          name: 'Đồng hồ Phòng Khách'
        });

      expect(res.statusCode).toBe(200);
      expect(res.body.message).toContain('Gateway claimed successfully');
    });
  });

  describe('3. Schedules Validation & Mutual Exclusivity (§7.2.4, §8.4)', () => {
    test('POST /api/v1/gateways/gw_test/schedules rejects missing hour or minute', async () => {
      const res = await request(app)
        .post('/api/v1/gateways/gw_test/schedules')
        .send({
          type: 'alarm'
        });

      expect(res.statusCode).toBe(400);
      expect(res.body.error.code).toBe('VALIDATION_ERROR');
    });

    test('POST /api/v1/gateways/gw_test/schedules rejects invalid schedule type', async () => {
      const res = await request(app)
        .post('/api/v1/gateways/gw_test/schedules')
        .send({
          type: 'invalid_type',
          hour: 7,
          minute: 0
        });

      expect(res.statusCode).toBe(400);
      expect(res.body.error.code).toBe('VALIDATION_ERROR');
    });

    test('POST /api/v1/gateways/gw_test/schedules rejects both repeat_days and one_time_date', async () => {
      const res = await request(app)
        .post('/api/v1/gateways/gw_test/schedules')
        .send({
          type: 'alarm',
          hour: 6,
          minute: 30,
          repeat_days: 'MON,TUE',
          one_time_date: '2026-08-20'
        });

      expect(res.statusCode).toBe(400);
      expect(res.body.error.message).toContain('mutually exclusive');
    });
  });

  describe('4. Command Downlink & Device APIs (§8.3, §9.2)', () => {
    test('POST /api/v1/gateways/gw_test/devices rejects invalid device_type and protocol', async () => {
      const res = await request(app)
        .post('/api/v1/gateways/gw_test/devices')
        .send({
          device_id: 'dev_01',
          name: 'Invalid Device',
          device_type: 'unknown',
          protocol: 'zigbee'
        });

      expect(res.statusCode).toBe(400);
      expect(res.body.error.code).toBe('VALIDATION_ERROR');
    });

    test('POST /api/v1/gateways/gw_test/devices/:dev/command returns 202 with command_id UUID', async () => {
      // Mock gateway as online
      const mockPrisma = new (require('@prisma/client').PrismaClient)();
      mockPrisma.gateway.findUnique.mockResolvedValueOnce({ status: 'online' });

      const res = await request(app)
        .post('/api/v1/gateways/gw_test/devices/dev_01/command')
        .send({
          command: 'ON',
          ttl_seconds: 60
        });

      expect(res.statusCode).toBe(202);
      expect(res.body.status).toBe('QUEUED');
      expect(res.body.command_id).toBeDefined();
      expect(res.body.command_id).toMatch(/^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/);
    });
  });

  describe('5. Telemetry Backfill Deduplication (§4.11.3)', () => {
    test('POST /api/v1/gateways/gw_test/telemetry/backfill processes batch with deduplication', async () => {
      const mockPrisma = new (require('@prisma/client').PrismaClient)();
      // First call finds null (new), second call finds existing
      mockPrisma.telemetryLog.findFirst
        .mockResolvedValueOnce(null)
        .mockResolvedValueOnce({ id: 1 });

      const res = await request(app)
        .post('/api/v1/gateways/gw_test/telemetry/backfill')
        .send({
          records: [
            { device_id: 'sensor_01', value: 25.5, timestamp: 1723621000 },
            { device_id: 'sensor_01', value: 25.5, timestamp: 1723621000 }
          ]
        });

      expect(res.statusCode).toBe(200);
      expect(res.body.total_received).toBe(2);
      expect(res.body.inserted_count).toBe(1);
      expect(res.body.deduplicated_count).toBe(1);
    });
  });

  describe('6. MQTT Broker Module Interface & Lifecycle (§9.1, §9.2, §9.3)', () => {
    test('Broker exports required functions', () => {
      expect(typeof broker.startBroker).toBe('function');
      expect(typeof broker.stopBroker).toBe('function');
      expect(typeof broker.publishDeviceCommand).toBe('function');
      expect(typeof broker.publishScheduleConfig).toBe('function');
      expect(typeof broker.isGatewayOnline).toBe('function');
      expect(broker.aedes).toBeDefined();
    });

    test('startBroker and stopBroker lifecycle works cleanly', async () => {
      const testMqttPort = 11883;
      await broker.startBroker(testMqttPort);
      await broker.stopBroker();
    });
  });
});
