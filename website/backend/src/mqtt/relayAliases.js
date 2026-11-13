/**
 * Shared bidirectional relay alias mapping and pending command state tracker.
 */
const RELAY_TO_ALIAS = {
  relay_1: 'fan_01',
  relay_2: 'heater_01',
  relay_3: 'dehumidifier_01',
  relay_4: 'aux_01'
};

const ALIAS_TO_RELAY = {
  fan_01: 'relay_1',
  heater_01: 'relay_2',
  dehumidifier_01: 'relay_3',
  aux_01: 'relay_4',
  light_01: 'relay_4'
};

const DEFAULT_ACTUATOR_NAMES = {
  relay_1: 'Relay 1 (Quạt)',
  fan_01: 'Quạt (Fan)',
  relay_2: 'Relay 2 (Sưởi)',
  heater_01: 'Sưởi (Heater)',
  relay_3: 'Relay 3 (Hút ẩm)',
  dehumidifier_01: 'Hút ẩm (Dehumidifier)',
  relay_4: 'Relay 4 (Aux)',
  aux_01: 'Phụ tải (Aux)',
  light_01: 'Đèn / Aux (Legacy)'
};

const PENDING_TTL_MS = 10 * 60 * 1000;
const pendingCommands = new Map();

function resolveRelayTargets(deviceId) {
  if (!deviceId) return { primary: [], optionalLegacy: [] };
  if (RELAY_TO_ALIAS[deviceId]) {
    return {
      primary: [deviceId, RELAY_TO_ALIAS[deviceId]],
      optionalLegacy: deviceId === 'relay_4' ? ['light_01'] : []
    };
  }
  if (ALIAS_TO_RELAY[deviceId]) {
    const physical = ALIAS_TO_RELAY[deviceId];
    if (deviceId === 'light_01') {
      return {
        primary: ['light_01', 'relay_4'],
        optionalLegacy: ['aux_01']
      };
    }
    return {
      primary: [deviceId, physical],
      optionalLegacy: physical === 'relay_4' ? ['light_01'] : []
    };
  }
  return { primary: [deviceId], optionalLegacy: [] };
}

function getAliasCandidates(deviceId) {
  const { primary, optionalLegacy } = resolveRelayTargets(deviceId);
  return [...new Set([...primary, ...optionalLegacy])];
}

function recordPendingCommand(msgId, { gatewayId, deviceId, state, timestamp }) {
  if (!msgId) return;
  const now = Date.now();
  if (pendingCommands.size > 500) {
    for (const [k, v] of pendingCommands.entries()) {
      if (now - v.timestamp > PENDING_TTL_MS) pendingCommands.delete(k);
    }
  }
  pendingCommands.set(msgId, {
    gatewayId,
    deviceId,
    state: state ? String(state).trim().toUpperCase() : undefined,
    timestamp: timestamp || now
  });
}

function consumePendingCommand(msgId) {
  if (!msgId || !pendingCommands.has(msgId)) return null;
  const entry = pendingCommands.get(msgId);
  pendingCommands.delete(msgId);
  if (Date.now() - entry.timestamp > PENDING_TTL_MS) return null;
  return entry;
}

function clearPendingCommands() {
  pendingCommands.clear();
}

module.exports = {
  RELAY_TO_ALIAS,
  ALIAS_TO_RELAY,
  DEFAULT_ACTUATOR_NAMES,
  resolveRelayTargets,
  getAliasCandidates,
  recordPendingCommand,
  consumePendingCommand,
  clearPendingCommands
};
