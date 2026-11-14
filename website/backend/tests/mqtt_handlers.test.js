const { EventEmitter } = require('events');

const mockMqttClient = new EventEmitter();
mockMqttClient.connected = true;
mockMqttClient.subscribe = jest.fn();
mockMqttClient.publish = jest.fn((topic, msg, opts, cb) => {
  if (typeof opts === 'function') opts(null);
  else if (cb) cb(null);
});
mockMqttClient.end = jest.fn((force, cb) => {
  if (typeof force === 'function') force();
  else if (cb) cb();
});

jest.mock('mqtt', () => ({
  connect: jest.fn(() => mockMqttClient)
}));

const mockPrisma = {
  gateway: {
    findUnique: jest.fn().mockResolvedValue({ id: 'gw_01', status: 'online' }),
    updateMany: jest.fn().mockResolvedValue({ count: 1 })
  },
  device: {
    findFirst: jest.fn(),
    create: jest.fn().mockResolvedValue({ id: 10 }),
    update: jest.fn().mockResolvedValue({ id: 10 }),
    updateMany: jest.fn().mockResolvedValue({ count: 1 })
  },
  telemetryLog: {
    create: jest.fn().mockResolvedValue({ id: 1 }),
    createMany: jest.fn().mockResolvedValue({ count: 2 })
  },
  eventLog: {
    create: jest.fn().mockResolvedValue({ id: 1 })
  },
  schedule: {
    findMany: jest.fn().mockResolvedValue([]),
    create: jest.fn().mockResolvedValue({ id: 1 }),
    deleteMany: jest.fn().mockResolvedValue({ count: 0 })
  },
  automationRule: {
    findMany: jest.fn().mockResolvedValue([]),
    create: jest.fn().mockResolvedValue({ id: 1 }),
    deleteMany: jest.fn().mockResolvedValue({ count: 0 })
  }
};

jest.mock('@prisma/client', () => ({
  PrismaClient: jest.fn(() => mockPrisma)
}));

jest.mock('../src/middleware/auth', () => ({
  authenticate: (req, res, next) => {
    req.user = { id: 1, username: 'admin' };
    next();
  }
}));

const request = require('supertest');
const app = require('../src/app');
const broker = require('../src/mqtt/broker');
const {
  resolveRelayTargets,
  getAliasCandidates,
  clearPendingCommands
} = require('../src/mqtt/relayAliases');

describe('Plan 1 Backend: Relay Aliases, MQTT Handlers & Telemetry Batching', () => {
  beforeAll(async () => {
    await broker.startBroker();
    mockMqttClient.emit('connect');
  });

  afterAll(async () => {
    await broker.stopBroker();
  });

  beforeEach(() => {
    jest.clearAllMocks();
    clearPendingCommands();
    if (typeof broker.resetSeededGateways === 'function') {
      broker.resetSeededGateways();
    }
    mockMqttClient.connected = true;
    mockPrisma.gateway.findUnique.mockResolvedValue({ id: 'gw_01', status: 'online' });
    mockPrisma.gateway.updateMany.mockResolvedValue({ count: 1 });
    mockPrisma.device.findFirst.mockResolvedValue(null);
    mockPrisma.device.create.mockResolvedValue({ id: 10 });
    mockPrisma.device.update.mockResolvedValue({ id: 10 });
    mockPrisma.device.updateMany.mockResolvedValue({ count: 1 });
    mockPrisma.telemetryLog.createMany.mockResolvedValue({ count: 2 });
    mockPrisma.eventLog.create.mockResolvedValue({ id: 1 });
    mockPrisma.schedule.findMany.mockResolvedValue([]);
    mockPrisma.schedule.create.mockResolvedValue({ id: 1 });
    mockPrisma.schedule.deleteMany.mockResolvedValue({ count: 0 });
    mockPrisma.automationRule.findMany.mockResolvedValue([]);
    mockPrisma.automationRule.create.mockResolvedValue({ id: 1 });
    mockPrisma.automationRule.deleteMany.mockResolvedValue({ count: 0 });
  });

  test('startBroker subscribes to gateway/+/device/+/action and routes action & command_ack', async () => {
    // Re-trigger connect handler on a fresh listener check via startBroker spy/calls
    await broker.stopBroker();
    mockMqttClient.subscribe.mockClear();
    await broker.startBroker();
    mockMqttClient.emit('connect');

    const subscribedTopics = mockMqttClient.subscribe.mock.calls.map((c) => c[0]);
    expect(subscribedTopics).toContain('gateway/+/device/+/action');
    expect(subscribedTopics).toContain('gateway/+/device/+/command_ack');

    mockMqttClient.emit(
      'message',
      'gateway/gw_01/device/relay_1/action',
      Buffer.from(JSON.stringify({ rule_id: 1, action: 'ON', status: 'EXECUTED' }))
    );
    await new Promise((r) => setImmediate(r));

    expect(mockPrisma.eventLog.create).toHaveBeenCalledWith(
      expect.objectContaining({
        data: expect.objectContaining({
          gatewayId: 'gw_01',
          eventType: 'rule_action',
          deviceId: 'relay_1'
        })
      })
    );
  });

  test('resolveRelayTargets maps all 4 physical relays and legacy light_01 bidirectionally', () => {
    expect(resolveRelayTargets('relay_1').primary).toEqual(['relay_1', 'fan_01']);
    expect(resolveRelayTargets('fan_01').primary).toEqual(['fan_01', 'relay_1']);
    expect(resolveRelayTargets('relay_2').primary).toEqual(['relay_2', 'heater_01']);
    expect(resolveRelayTargets('heater_01').primary).toEqual(['heater_01', 'relay_2']);
    expect(resolveRelayTargets('relay_3').primary).toEqual(['relay_3', 'dehumidifier_01']);
    expect(resolveRelayTargets('dehumidifier_01').primary).toEqual(['dehumidifier_01', 'relay_3']);
    expect(resolveRelayTargets('relay_4')).toEqual({
      primary: ['relay_4', 'aux_01'],
      optionalLegacy: ['light_01']
    });
    expect(getAliasCandidates('light_01')).toEqual(['light_01', 'relay_4', 'aux_01']);
  });

  test('handleTelemetry uses identical timestamp, batches via createMany, and sets sensor_hum_01 lastValue to hum', async () => {
    await broker.handleTelemetry('gw_01', JSON.stringify({
      device_id: 'sensor_hum_01',
      temperature: 26.0,
      humidity: 82.0
    }));

    expect(mockPrisma.device.create).toHaveBeenCalledWith(
      expect.objectContaining({
        data: expect.objectContaining({
          deviceId: 'sensor_hum_01',
          lastValue: 82.0
        })
      })
    );

    expect(mockPrisma.telemetryLog.createMany).toHaveBeenCalledTimes(1);
    const batchArg = mockPrisma.telemetryLog.createMany.mock.calls[0][0];
    expect(batchArg.data).toHaveLength(2);
    expect(batchArg.data[0]).toMatchObject({ deviceId: 'sensor_hum_01', value: 26.0, unit: '°C' });
    expect(batchArg.data[1]).toMatchObject({ deviceId: 'sensor_hum_01', value: 82.0, unit: '%' });
    expect(batchArg.data[0].recordedAt.getTime()).toBe(batchArg.data[1].recordedAt.getTime());
    expect(mockPrisma.telemetryLog.create).not.toHaveBeenCalled();
  });

  test('handleTelemetry sets sensor_temp_01 lastValue to temp on existing device update', async () => {
    mockPrisma.device.findFirst.mockResolvedValueOnce({ id: 247, deviceId: 'sensor_temp_01' });

    await broker.handleTelemetry('gw_01', JSON.stringify({
      device_id: 'sensor_temp_01',
      temp: 29.5,
      hum: 61.0
    }));

    expect(mockPrisma.device.update).toHaveBeenCalledWith(
      expect.objectContaining({
        where: { id: 247 },
        data: expect.objectContaining({ status: 'online', lastValue: 29.5 })
      })
    );
  });

  test('sendDeviceCommand records pending state and handleCommandAck updates both relay and alias', async () => {
    mockPrisma.device.findFirst.mockResolvedValueOnce({
      id: 5,
      deviceId: 'fan_01',
      protocol: 'mqtt'
    });

    const res = await request(app)
      .post('/api/v1/gateways/gw_01/devices/fan_01/command')
      .send({ command: 'ON', ttl_seconds: 60 });

    expect(res.statusCode).toBe(202);
    const cmdId = res.body.command_id;

    // Simulate relay_1 missing in DB while fan_01 exists
    mockPrisma.device.updateMany.mockImplementation(({ where }) => {
      if (where.deviceId === 'relay_1') return Promise.resolve({ count: 0 });
      return Promise.resolve({ count: 1 });
    });

    await broker.handleCommandAck('gw_01', 'fan_01', JSON.stringify({
      msg_id: cmdId,
      success: true,
      error_code: 0,
      message: 'OK'
    }));

    expect(mockPrisma.eventLog.create).toHaveBeenCalledWith(
      expect.objectContaining({
        data: expect.objectContaining({
          gatewayId: 'gw_01',
          eventType: 'command_ack',
          deviceId: 'fan_01'
        })
      })
    );
    const eventPayload = JSON.parse(mockPrisma.eventLog.create.mock.calls[0][0].data.payload);
    expect(eventPayload.state).toBe('ON');
    expect(eventPayload.msg_id).toBe(cmdId);

    // fan_01 updated via updateMany, relay_1 auto-created as actuator with lastValue: 1
    expect(mockPrisma.device.updateMany).toHaveBeenCalledWith(
      expect.objectContaining({
        where: { gatewayId: 'gw_01', deviceId: 'fan_01' },
        data: expect.objectContaining({ status: 'online', lastValue: 1 })
      })
    );
    expect(mockPrisma.device.create).toHaveBeenCalledWith(
      expect.objectContaining({
        data: expect.objectContaining({
          gatewayId: 'gw_01',
          deviceId: 'relay_1',
          deviceType: 'actuator',
          status: 'online',
          lastValue: 1
        })
      })
    );
  });

  test('handleCommandAck maps error_code 3 -> safety_interlock and error_code 4 -> compressor_dwell_lock', async () => {
    await broker.handleCommandAck('gw_01', 'relay_2', JSON.stringify({
      command_id: 'cmd-interlock-1',
      success: false,
      error_code: 3,
      message: 'Cannot turn ON Heater while Fan is running'
    }));

    expect(mockPrisma.eventLog.create).toHaveBeenCalledWith(
      expect.objectContaining({
        data: expect.objectContaining({
          eventType: 'safety_interlock',
          deviceId: 'relay_2'
        })
      })
    );
    expect(mockPrisma.device.updateMany).not.toHaveBeenCalled();

    jest.clearAllMocks();

    await broker.handleCommandAck('gw_01', 'relay_3', JSON.stringify({
      command_id: 'cmd-dwell-1',
      success: false,
      error_code: 4,
      message: 'Compressor dwell locked (180s)'
    }));

    expect(mockPrisma.eventLog.create).toHaveBeenCalledWith(
      expect.objectContaining({
        data: expect.objectContaining({
          eventType: 'compressor_dwell_lock',
          deviceId: 'relay_3'
        })
      })
    );
    expect(mockPrisma.device.updateMany).not.toHaveBeenCalled();
  });

  test('handleDeviceAction persists rule_action and syncs relay_4 + aux_01 + legacy light_01', async () => {
    await broker.handleDeviceAction('gw_01', 'relay_4', JSON.stringify({
      rule_id: 7,
      action: 'OFF',
      status: 'EXECUTED',
      timestamp: 1731500000
    }));

    expect(mockPrisma.eventLog.create).toHaveBeenCalledWith(
      expect.objectContaining({
        data: expect.objectContaining({
          gatewayId: 'gw_01',
          eventType: 'rule_action',
          deviceId: 'relay_4'
        })
      })
    );
    const updatedIds = mockPrisma.device.updateMany.mock.calls.map((c) => c[0].where.deviceId);
    expect(updatedIds).toEqual(expect.arrayContaining(['relay_4', 'aux_01', 'light_01']));
    for (const call of mockPrisma.device.updateMany.mock.calls) {
      expect(call[0].data).toMatchObject({ status: 'online', lastValue: 0 });
    }
  });

  test('handleDeviceAction supports firmware payload (source_rule/command) and skips update on REJECTED', async () => {
    await broker.handleDeviceAction('gw_01', 'relay_2', JSON.stringify({
      source_rule: 2,
      command: 'ON',
      status: 'REJECTED',
      command_id: 'cmd-rule-2-100'
    }));

    expect(mockPrisma.eventLog.create).toHaveBeenCalledWith(
      expect.objectContaining({
        data: expect.objectContaining({
          eventType: 'rule_action',
          deviceId: 'relay_2'
        })
      })
    );
    expect(mockPrisma.device.updateMany).not.toHaveBeenCalled();
  });

  test('handleDeviceAction logs TRIGGERED rule action without premature actuator update', async () => {
    await broker.handleDeviceAction('gw_01', 'relay_2', JSON.stringify({
      source_rule: 2,
      command: 'ON',
      status: 'TRIGGERED',
      command_id: 'cmd-rule-2-101'
    }));

    expect(mockPrisma.eventLog.create).toHaveBeenCalledWith(
      expect.objectContaining({
        data: expect.objectContaining({
          eventType: 'rule_action',
          deviceId: 'relay_2'
        })
      })
    );
    expect(mockPrisma.device.updateMany).not.toHaveBeenCalled();
  });

  test('handleEvents syncs relay and alias state on relay_state_changed events', async () => {
    await broker.handleEvents('gw_01', JSON.stringify({
      type: 'relay_state_changed',
      device_id: 'relay_2',
      relay_id: 2,
      state: 'OFF',
      reason: 'COOLING_OVERRIDE',
      timestamp: 1731500100
    }));

    expect(mockPrisma.eventLog.create).toHaveBeenCalledWith(
      expect.objectContaining({
        data: expect.objectContaining({
          gatewayId: 'gw_01',
          eventType: 'relay_state_changed',
          deviceId: 'relay_2'
        })
      })
    );
    const updatedIds = mockPrisma.device.updateMany.mock.calls.map((c) => c[0].where.deviceId);
    expect(updatedIds).toEqual(expect.arrayContaining(['relay_2', 'heater_01']));
    for (const call of mockPrisma.device.updateMany.mock.calls) {
      expect(call[0].data).toMatchObject({ status: 'online', lastValue: 0 });
    }
  });
});

describe('Schedule & Rule 2-Way Sync (PLAN_SCHEDULE_SYNC)', () => {
  beforeEach(() => {
    jest.clearAllMocks();
    clearPendingCommands();
    broker.resetSeededGateways();
    mockMqttClient.connected = true;
    mockPrisma.gateway.findUnique.mockResolvedValue({ id: 'gw_01', status: 'online' });
    mockPrisma.device.findFirst.mockResolvedValue(null);
    mockPrisma.device.create.mockResolvedValue({ id: 10 });
    mockPrisma.eventLog.create.mockResolvedValue({ id: 1 });
    mockPrisma.schedule.findMany.mockResolvedValue([]);
    mockPrisma.schedule.create.mockResolvedValue({ id: 1 });
    mockPrisma.schedule.deleteMany.mockResolvedValue({ count: 0 });
    mockPrisma.automationRule.findMany.mockResolvedValue([]);
    mockPrisma.automationRule.create.mockResolvedValue({ id: 1 });
    mockPrisma.automationRule.deleteMany.mockResolvedValue({ count: 0 });
  });

  test('startBroker subscribes to alarm, schedule/sync, and rules/sync topics', async () => {
    await broker.stopBroker();
    mockMqttClient.subscribe.mockClear();
    await broker.startBroker();
    mockMqttClient.emit('connect');

    const subscribedTopics = mockMqttClient.subscribe.mock.calls.map((c) => c[0]);
    expect(subscribedTopics).toContain('gateway/+/alarm');
    expect(subscribedTopics).toContain('gateway/+/schedule/sync');
    expect(subscribedTopics).toContain('gateway/+/rules/sync');
  });

  test('publishScheduleConfig seeds defaults once, maps aliases & repeat_days_indices, and publishes retained QoS 1', async () => {
    const seededSchedules = [
      {
        id: 1,
        gatewayId: 'gw_01',
        type: 'alarm',
        hour: 7,
        minute: 0,
        repeatDays: 'SUN,MON,TUE,WED,THU,FRI,SAT',
        oneTimeDate: null,
        enabled: true,
        targetDeviceId: null,
        actionCommand: null,
        title: 'Good Morning Alarm',
        buzzer: true
      },
      {
        id: 2,
        gatewayId: 'gw_01',
        type: 'automation',
        hour: 18,
        minute: 0,
        repeatDays: 'MON,TUE,WED,THU,FRI',
        oneTimeDate: null,
        enabled: true,
        targetDeviceId: 'aux_01',
        actionCommand: 'OFF',
        title: 'Office Auto AC Off',
        buzzer: false
      }
    ];
    const seededRules = [
      {
        id: 1,
        gatewayId: 'gw_01',
        sensorDeviceId: 'sensor_temp_01',
        conditionType: 'gt',
        threshold: 30.0,
        targetDeviceId: 'fan_01',
        actionCommand: 'ON',
        enabled: true
      }
    ];

    // First findMany calls inside ensureDefaultSchedulesAndRules return [], subsequent queries return seeded rows
    mockPrisma.automationRule.findMany
      .mockResolvedValueOnce([])
      .mockResolvedValue(seededRules);
    mockPrisma.schedule.findMany
      .mockResolvedValueOnce([])
      .mockResolvedValue(seededSchedules);

    const ok = await broker.publishScheduleConfig('gw_01');
    expect(ok).toBe(true);

    // 3 default rules + 2 default schedules created
    expect(mockPrisma.automationRule.create).toHaveBeenCalledTimes(3);
    expect(mockPrisma.schedule.create).toHaveBeenCalledTimes(2);

    // Verify MQTT publish topic, qos: 1, retain: true
    expect(mockMqttClient.publish).toHaveBeenCalledTimes(1);
    const [topic, payloadStr, opts] = mockMqttClient.publish.mock.calls[0];
    expect(topic).toBe('gateway/gw_01/config/schedule');
    expect(opts).toEqual({ qos: 1, retain: true });

    const payload = JSON.parse(payloadStr);
    expect(payload.updated_at).toEqual(expect.any(Number));
    expect(payload.schedules).toHaveLength(2);
    expect(payload.schedules[0]).toMatchObject({
      id: 1,
      type: 'alarm',
      repeat_days: 'SUN,MON,TUE,WED,THU,FRI,SAT',
      repeat_days_indices: [0, 1, 2, 3, 4, 5, 6],
      target_device_id: '',
      buzzer: true
    });
    expect(payload.schedules[1]).toMatchObject({
      id: 2,
      type: 'automation',
      repeat_days: 'MON,TUE,WED,THU,FRI',
      repeat_days_indices: [1, 2, 3, 4, 5],
      target_device_id: 'relay_4',
      action_command: 'OFF',
      buzzer: false
    });
    expect(payload.rules[0]).toMatchObject({
      id: 1,
      sensor_device_id: 'sensor_temp_01',
      condition_type: 'gt',
      threshold: 30.0,
      target_device_id: 'relay_1',
      action_command: 'ON',
      enabled: true
    });

    // Calling again in same process must NOT re-seed even if DB returns empty (user deleted all)
    mockPrisma.automationRule.create.mockClear();
    mockPrisma.schedule.create.mockClear();
    mockPrisma.automationRule.findMany.mockResolvedValue([]);
    mockPrisma.schedule.findMany.mockResolvedValue([]);

    await broker.publishScheduleConfig('gw_01');
    expect(mockPrisma.automationRule.create).not.toHaveBeenCalled();
    expect(mockPrisma.schedule.create).not.toHaveBeenCalled();
  });

  test('publishScheduleConfig returns false when MQTT client is disconnected', async () => {
    mockMqttClient.connected = false;
    const ok = await broker.publishScheduleConfig('gw_01', { skipSeed: true });
    expect(ok).toBe(false);
    expect(mockMqttClient.publish).not.toHaveBeenCalled();
  });

  test('handleAlarmEvent records alarm_triggered EventLog and routes via gateway/+/alarm', async () => {
    mockMqttClient.emit(
      'message',
      'gateway/gw_01/alarm',
      Buffer.from(JSON.stringify({
        alarm_id: 1,
        title: 'Good Morning Alarm',
        time: '07:00',
        buzzer: true
      }))
    );
    await new Promise((r) => setImmediate(r));

    expect(mockPrisma.eventLog.create).toHaveBeenCalledWith(
      expect.objectContaining({
        data: expect.objectContaining({
          gatewayId: 'gw_01',
          eventType: 'alarm_triggered',
          deviceId: null
        })
      })
    );
    const savedPayload = JSON.parse(mockPrisma.eventLog.create.mock.calls[0][0].data.payload);
    expect(savedPayload.alarm_id).toBe(1);
    expect(savedPayload.message).toBe('Alarm triggered: Good Morning Alarm at 07:00');
  });

  test('handleScheduleSyncFromGateway replaces DB schedules, maps aliases/repeat_days, and republishes retained config', async () => {
    await broker.handleScheduleSyncFromGateway('gw_01', JSON.stringify({
      schedules: [
        {
          type: 'alarm',
          hour: 8,
          minute: 30,
          repeat_days: [false, true, true, true, true, true, false],
          enabled: true,
          title: 'Morning Check',
          buzzer: true
        },
        {
          type: 'automation',
          hour: 22,
          minute: 15,
          repeat_days: [0, 6],
          enabled: true,
          target_device_id: 'office_ac_relay',
          action_command: 'OFF',
          title: 'Night AC Off',
          buzzer: false
        }
      ]
    }));

    expect(mockPrisma.schedule.deleteMany).toHaveBeenCalledWith({ where: { gatewayId: 'gw_01' } });
    expect(mockPrisma.schedule.create).toHaveBeenNthCalledWith(1, {
      data: expect.objectContaining({
        gatewayId: 'gw_01',
        type: 'alarm',
        hour: 8,
        minute: 30,
        repeatDays: 'MON,TUE,WED,THU,FRI',
        targetDeviceId: null,
        title: 'Morning Check',
        buzzer: true
      })
    });
    expect(mockPrisma.schedule.create).toHaveBeenNthCalledWith(2, {
      data: expect.objectContaining({
        gatewayId: 'gw_01',
        type: 'automation',
        hour: 22,
        minute: 15,
        repeatDays: 'SUN,SAT',
        targetDeviceId: 'aux_01',
        actionCommand: 'OFF',
        buzzer: false
      })
    });

    expect(mockMqttClient.publish).toHaveBeenCalledWith(
      'gateway/gw_01/config/schedule',
      expect.any(String),
      { qos: 1, retain: true },
      expect.any(Function)
    );
  });

  test('handleRulesSyncFromGateway replaces DB rules, maps relay_1..4 -> aliases, normalizes conditions, and republishes', async () => {
    mockMqttClient.emit(
      'message',
      'gateway/gw_01/rules/sync',
      Buffer.from(JSON.stringify({
        rules: [
          {
            sensor_device_id: 'sensor_temp_01',
            condition_type: '>',
            threshold: 31.5,
            target_device_id: 'relay_1',
            action_command: 'on',
            enabled: true
          },
          {
            sensor_device_id: 'sensor_temp_01',
            condition_type: '<',
            threshold: 20.0,
            target_device_id: 'relay_2',
            action_command: 'ON',
            enabled: false
          }
        ]
      }))
    );
    await new Promise((r) => setImmediate(r));

    expect(mockPrisma.automationRule.deleteMany).toHaveBeenCalledWith({ where: { gatewayId: 'gw_01' } });
    expect(mockPrisma.automationRule.create).toHaveBeenNthCalledWith(1, {
      data: {
        gatewayId: 'gw_01',
        sensorDeviceId: 'sensor_temp_01',
        conditionType: 'gt',
        threshold: 31.5,
        targetDeviceId: 'fan_01',
        actionCommand: 'ON',
        enabled: true
      }
    });
    expect(mockPrisma.automationRule.create).toHaveBeenNthCalledWith(2, {
      data: {
        gatewayId: 'gw_01',
        sensorDeviceId: 'sensor_temp_01',
        conditionType: 'lt',
        threshold: 20.0,
        targetDeviceId: 'heater_01',
        actionCommand: 'ON',
        enabled: false
      }
    });
    expect(mockMqttClient.publish).toHaveBeenCalledWith(
      'gateway/gw_01/config/schedule',
      expect.any(String),
      { qos: 1, retain: true },
      expect.any(Function)
    );
  });

  test('createRule auto-creates known standard devices but returns 404 for unknown devices', async () => {
    // Unknown sensor -> 404 DEVICE_NOT_FOUND
    const resUnknown = await request(app)
      .post('/api/v1/gateways/gw_01/rules')
      .send({
        sensor_device_id: 'unknown_sensor_99',
        condition_type: 'gt',
        threshold: 30,
        target_device_id: 'fan_01',
        action_command: 'ON'
      });
    expect(resUnknown.statusCode).toBe(404);
    expect(resUnknown.body.error.code).toBe('DEVICE_NOT_FOUND');

    // Known standard sensor & target missing in DB -> auto-created and rule created (201)
    jest.clearAllMocks();
    mockPrisma.gateway.findUnique.mockResolvedValue({ id: 'gw_01', status: 'online' });
    mockPrisma.device.findFirst.mockResolvedValue(null);
    mockPrisma.device.create.mockResolvedValue({ id: 20 });
    mockPrisma.automationRule.create.mockResolvedValue({ id: 55 });
    mockPrisma.automationRule.findMany.mockResolvedValue([{
      id: 55,
      gatewayId: 'gw_01',
      sensorDeviceId: 'sensor_temp_01',
      conditionType: 'gt',
      threshold: 31,
      targetDeviceId: 'fan_01',
      actionCommand: 'ON',
      enabled: true
    }]);
    mockPrisma.schedule.findMany.mockResolvedValue([{
      id: 1,
      gatewayId: 'gw_01',
      type: 'alarm',
      hour: 7,
      minute: 0,
      repeatDays: 'SUN,MON,TUE,WED,THU,FRI,SAT',
      oneTimeDate: null,
      enabled: true,
      targetDeviceId: null,
      actionCommand: null,
      title: 'Good Morning Alarm',
      buzzer: true
    }]);

    const resKnown = await request(app)
      .post('/api/v1/gateways/gw_01/rules')
      .send({
        sensor_device_id: 'sensor_temp_01',
        condition_type: 'gt',
        threshold: 31,
        target_device_id: 'fan_01',
        action_command: 'ON'
      });
    await new Promise((r) => setImmediate(r));
    expect(resKnown.statusCode).toBe(201);
    expect(mockPrisma.device.create).toHaveBeenCalledTimes(2);
    expect(mockPrisma.automationRule.create).toHaveBeenCalledTimes(1);
    expect(mockMqttClient.publish).toHaveBeenCalledWith(
      'gateway/gw_01/config/schedule',
      expect.any(String),
      { qos: 1, retain: true },
      expect.any(Function)
    );
  });

  test('listSchedules and listRules auto-seed defaults when gateway DB is empty', async () => {
    mockPrisma.schedule.findMany
      .mockResolvedValueOnce([]) // initial listSchedules query
      .mockResolvedValueOnce([]) // ensureDefaultSchedulesAndRules check
      .mockResolvedValue([{
        id: 1,
        gatewayId: 'gw_01',
        type: 'alarm',
        hour: 7,
        minute: 0,
        repeatDays: 'SUN,MON,TUE,WED,THU,FRI,SAT',
        oneTimeDate: null,
        enabled: true,
        targetDeviceId: null,
        actionCommand: null,
        title: 'Good Morning Alarm',
        buzzer: true
      }]);
    mockPrisma.automationRule.findMany
      .mockResolvedValueOnce([]) // ensureDefaultSchedulesAndRules check
      .mockResolvedValue([{
        id: 1,
        gatewayId: 'gw_01',
        sensorDeviceId: 'sensor_temp_01',
        conditionType: 'gt',
        threshold: 30.0,
        targetDeviceId: 'fan_01',
        actionCommand: 'ON',
        enabled: true
      }]);

    const schedRes = await request(app).get('/api/v1/gateways/gw_01/schedules');
    await new Promise((r) => setImmediate(r));
    expect(schedRes.statusCode).toBe(200);
    expect(schedRes.body).toHaveLength(1);
    expect(mockPrisma.schedule.create).toHaveBeenCalledTimes(2);
    expect(mockPrisma.automationRule.create).toHaveBeenCalledTimes(3);
  });
});
