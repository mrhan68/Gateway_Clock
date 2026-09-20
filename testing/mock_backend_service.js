/**
 * Mock Backend Service for Local E2E System Testing
 * =================================================
 * Runs the real Express app and real Aedes MQTT broker on ports 3000 and 1883
 * with a comprehensive in-memory database store that mimics Prisma Client.
 */

const { EventEmitter } = require('events');
const http = require('http');

// ── In-Memory Database Store ────────────────────────────────────────────────
class InMemoryStore {
  constructor() {
    this.gateways = new Map();
    this.devices = new Map();
    this.schedules = new Map();
    this.rules = new Map();
    this.eventLogs = [];
    this.telemetryLogs = [];
    this.autoInc = { device: 1, schedule: 1, rule: 1, event: 1, telem: 1 };
  }

  // ── Gateway Operations ──
  async findUniqueGateway({ where }) {
    if (where.id) {
      const gw = this.gateways.get(where.id);
      if (!gw) return null;
      let countDevices = 0, countSchedules = 0, countRules = 0;
      for (const d of this.devices.values()) if (d.gatewayId === where.id) countDevices++;
      for (const s of this.schedules.values()) if (s.gatewayId === where.id) countSchedules++;
      for (const r of this.rules.values()) if (r.gatewayId === where.id) countRules++;
      return {
        ...gw,
        _count: {
          devices: countDevices,
          schedules: countSchedules,
          automationRules: countRules
        }
      };
    }
    return null;
  }

  async findFirstGateway({ where }) {
    for (const gw of this.gateways.values()) {
      if (where.claimCode && gw.claimCode === where.claimCode) {
        if (where.status && gw.status !== where.status) continue;
        if (where.claimExpiresAt && where.claimExpiresAt.gte && gw.claimExpiresAt < where.claimExpiresAt.gte) continue;
        return gw;
      }
      if (where.macAddress && gw.macAddress === where.macAddress) return gw;
      if (where.id && gw.id === where.id) return gw;
      if (where.OR) {
        for (const cond of where.OR) {
          if (cond.macAddress && gw.macAddress === cond.macAddress) return gw;
          if (cond.id && gw.id === cond.id) return gw;
        }
      }
    }
    return null;
  }

  async findManyGateways() {
    return Array.from(this.gateways.values());
  }

  async createGateway({ data }) {
    const gw = {
      ...data,
      createdAt: new Date(),
      updatedAt: new Date(),
      lastSeen: new Date()
    };
    this.gateways.set(gw.id, gw);
    return gw;
  }

  async updateGateway({ where, data }) {
    const gw = this.gateways.get(where.id);
    if (!gw) return null;
    Object.assign(gw, data, { updatedAt: new Date() });
    return gw;
  }

  async updateManyGateways({ where, data }) {
    let count = 0;
    for (const gw of this.gateways.values()) {
      if (where.id && gw.id !== where.id) continue;
      Object.assign(gw, data, { updatedAt: new Date() });
      count++;
    }
    return { count };
  }

  // ── Device Operations ──
  async findFirstDevice({ where }) {
    for (const d of this.devices.values()) {
      if (where.gatewayId && d.gatewayId !== where.gatewayId) continue;
      if (where.deviceId && d.deviceId !== where.deviceId) continue;
      return d;
    }
    return null;
  }

  async findManyDevices({ where }) {
    const res = [];
    for (const d of this.devices.values()) {
      if (where && where.gatewayId && d.gatewayId !== where.gatewayId) continue;
      res.push(d);
    }
    return res;
  }

  async createDevice({ data }) {
    const id = this.autoInc.device++;
    const dev = {
      id,
      ...data,
      createdAt: new Date(),
      updatedAt: new Date(),
      lastSeen: new Date(),
      status: 'online'
    };
    this.devices.set(`${data.gatewayId}_${data.deviceId}`, dev);
    return dev;
  }

  async updateManyDevices({ where, data }) {
    let count = 0;
    for (const d of this.devices.values()) {
      if (where.gatewayId && d.gatewayId !== where.gatewayId) continue;
      if (where.deviceId && d.deviceId !== where.deviceId) continue;
      Object.assign(d, data);
      count++;
    }
    return { count };
  }

  // ── Schedule Operations ──
  async findManySchedules({ where }) {
    const res = [];
    for (const s of this.schedules.values()) {
      if (where && where.gatewayId && s.gatewayId !== where.gatewayId) continue;
      res.push(s);
    }
    return res;
  }

  async createSchedule({ data }) {
    const id = this.autoInc.schedule++;
    const sched = {
      id,
      ...data,
      createdAt: new Date(),
      updatedAt: new Date()
    };
    this.schedules.set(id, sched);
    return sched;
  }

  // ── Automation Rule Operations ──
  async findManyRules({ where }) {
    const res = [];
    for (const r of this.rules.values()) {
      if (where && where.gatewayId && r.gatewayId !== where.gatewayId) continue;
      res.push(r);
    }
    return res;
  }

  async createRule({ data }) {
    const id = this.autoInc.rule++;
    const rule = {
      id,
      ...data,
      createdAt: new Date(),
      updatedAt: new Date()
    };
    this.rules.set(id, rule);
    return rule;
  }

  // ── Event Log Operations ──
  async findManyEventLogs({ where }) {
    return this.eventLogs.filter(e => {
      if (where && where.gatewayId && e.gatewayId !== where.gatewayId) return false;
      if (where && where.eventType && e.eventType !== where.eventType) return false;
      return true;
    });
  }

  async createEventLog({ data }) {
    const id = this.autoInc.event++;
    const log = { id, ...data, createdAt: new Date() };
    this.eventLogs.push(log);
    return log;
  }

  // ── Telemetry Log Operations ──
  async findManyTelemetryLogs({ where, take }) {
    let res = this.telemetryLogs.filter(t => {
      if (where && where.gatewayId && t.gatewayId !== where.gatewayId) return false;
      if (where && where.deviceId && t.deviceId !== where.deviceId) return false;
      return true;
    });
    res.sort((a, b) => b.recordedAt - a.recordedAt);
    if (take) res = res.slice(0, take);
    return res;
  }

  async findFirstTelemetryLog({ where }) {
    for (const t of this.telemetryLogs) {
      if (where.gatewayId && t.gatewayId !== where.gatewayId) continue;
      if (where.deviceId && t.deviceId !== where.deviceId) continue;
      if (where.recordedAt && t.recordedAt.getTime() !== where.recordedAt.getTime()) continue;
      return t;
    }
    return null;
  }

  async createTelemetryLog({ data }) {
    const id = this.autoInc.telem++;
    const log = { id, ...data, createdAt: new Date() };
    this.telemetryLogs.push(log);
    return log;
  }
}

const store = new InMemoryStore();

// ── Intercept Prisma Client ────────────────────────────────────────────────
const mockPrismaClient = {
  PrismaClient: function () {
    return {
      $connect: async () => {},
      $disconnect: async () => {},
      gateway: {
        findUnique: store.findUniqueGateway.bind(store),
        findFirst: store.findFirstGateway.bind(store),
        findMany: store.findManyGateways.bind(store),
        create: store.createGateway.bind(store),
        update: store.updateGateway.bind(store),
        updateMany: store.updateManyGateways.bind(store)
      },
      device: {
        findFirst: store.findFirstDevice.bind(store),
        findMany: store.findManyDevices.bind(store),
        create: store.createDevice.bind(store),
        updateMany: store.updateManyDevices.bind(store)
      },
      schedule: {
        findMany: store.findManySchedules.bind(store),
        create: store.createSchedule.bind(store)
      },
      automationRule: {
        findMany: store.findManyRules.bind(store),
        create: store.createRule.bind(store)
      },
      eventLog: {
        findMany: store.findManyEventLogs.bind(store),
        create: store.createEventLog.bind(store)
      },
      telemetryLog: {
        findMany: store.findManyTelemetryLogs.bind(store),
        findFirst: store.findFirstTelemetryLog.bind(store),
        create: store.createTelemetryLog.bind(store)
      }
    };
  }
};

// Override require cache for @prisma/client
const prismaPath = require.resolve('@prisma/client');
require.cache[prismaPath] = {
  id: prismaPath,
  filename: prismaPath,
  loaded: true,
  exports: mockPrismaClient
};

// ── Launch Real Express App & Real Aedes Broker ─────────────────────────────
const app = require('../website/backend/src/app');
const mqttBroker = require('../website/backend/src/mqtt/broker');

const HTTP_PORT = 3000;
const MQTT_PORT = 1883;

async function start() {
  const server = http.createServer(app);
  
  await new Promise((resolve) => server.listen(HTTP_PORT, resolve));
  console.log(`[MockBackend] Express REST API listening on port ${HTTP_PORT}`);

  await mqttBroker.startBroker(MQTT_PORT);
  console.log(`[MockBackend] Aedes MQTT Broker listening on port ${MQTT_PORT}`);
  console.log('[MockBackend] Ready for E2E Live Integration Test');

  // Handle graceful shutdown
  process.on('SIGTERM', async () => {
    await mqttBroker.stopBroker();
    server.close();
    process.exit(0);
  });
  process.on('SIGINT', async () => {
    await mqttBroker.stopBroker();
    server.close();
    process.exit(0);
  });
}

start().catch((err) => {
  console.error('[MockBackend] Startup failed:', err);
  process.exit(1);
});
