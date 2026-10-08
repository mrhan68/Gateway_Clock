const mqtt = require('../website/backend/node_modules/mqtt');

const BACKEND_URL = process.env.BACKEND_URL || 'https://smartclock-backend.onrender.com';
const GATEWAY_ID = process.env.GATEWAY_ID || 'gw_smartclock_01';
const DEVICE_ID = 'sensor_temp_01';
const MQTT_BROKER = process.env.MQTT_URI || 'mqtts://your-emqx-broker.emqxsl.com:8883';
const MQTT_USER = process.env.MQTT_USER || 'gw_client';
const MQTT_PASS = process.env.MQTT_PASS || 'your_mqtt_password';

const args = process.argv.slice(2);
const isRevertOnly = args.includes('--revert');
const durationArg = args.indexOf('--duration');
const durationSeconds = durationArg !== -1 ? parseInt(args[durationArg + 1], 10) : 90;

async function revertClean() {
  console.log(`[CLEANUP] Restoring system: removing simulated device '${DEVICE_ID}'...`);
  try {
    const res = await fetch(`${BACKEND_URL}/api/v1/gateways/${GATEWAY_ID}/devices/${DEVICE_ID}`, {
      method: 'DELETE'
    });
    if (res.status === 204 || res.status === 404) {
      console.log(`[CLEANUP] Successfully removed '${DEVICE_ID}'. Backend reverted to clean standard state.`);
    } else {
      console.warn(`[CLEANUP] DELETE status: ${res.status}`);
    }
  } catch (err) {
    console.error(`[CLEANUP] Error during revert:`, err.message);
  }
}

async function ensureDeviceExists() {
  console.log(`[INIT] Checking device '${DEVICE_ID}' on backend...`);
  const listRes = await fetch(`${BACKEND_URL}/api/v1/gateways/${GATEWAY_ID}/devices`);
  const devices = await listRes.json();
  const found = Array.isArray(devices) && devices.find(d => d.device_id === DEVICE_ID);
  
  if (!found) {
    console.log(`[INIT] Registering simulated BLE sensor '${DEVICE_ID}'...`);
    const createRes = await fetch(`${BACKEND_URL}/api/v1/gateways/${GATEWAY_ID}/devices`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        device_id: DEVICE_ID,
        name: 'Cảm Biến BTHome Phòng Khách (Test)',
        device_type: 'sensor',
        protocol: 'ble',
        mac_address: 'AA:BB:CC:DD:EE:01',
        capabilities: JSON.stringify(['temperature', 'humidity'])
      })
    });
    const createJson = await createRes.json();
    console.log(`[INIT] Device registered:`, createJson);
  } else {
    console.log(`[INIT] Device '${DEVICE_ID}' is already registered.`);
  }
}

async function postTelemetryBatch(records) {
  try {
    const res = await fetch(`${BACKEND_URL}/api/v1/gateways/${GATEWAY_ID}/telemetry/backfill`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ records })
    });
    return await res.json();
  } catch (err) {
    console.error('[ERROR] Failed to post telemetry backfill:', err.message);
    return null;
  }
}

async function run() {
  if (isRevertOnly) {
    await revertClean();
    process.exit(0);
  }

  console.log(`=== STARTING SENSOR TELEMETRY SIMULATION ===`);
  console.log(`Gateway: ${GATEWAY_ID}`);
  console.log(`Device : ${DEVICE_ID}`);
  console.log(`Duration: ${durationSeconds} seconds`);
  console.log(`Target Backend: ${BACKEND_URL}`);

  await ensureDeviceExists();

  // Populate initial 5 historic points so charts display rich history immediately
  console.log(`[INIT] Pre-populating baseline trend data for immediate dashboard charting...`);
  const initialRecords = [];
  const baseTime = Math.floor(Date.now() / 1000) - 60;
  for (let i = 0; i < 5; i++) {
    const t = baseTime + i * 10;
    const temp = +(27.8 + Math.sin(i * 0.5) * 0.5).toFixed(1);
    const hum = +(63.5 + Math.cos(i * 0.5) * 1.5).toFixed(1);
    initialRecords.push({
      device_id: DEVICE_ID,
      value: temp,
      unit: '°C',
      timestamp: t
    });
    initialRecords.push({
      device_id: DEVICE_ID,
      value: hum,
      unit: '%',
      timestamp: t + 1
    });
  }
  await postTelemetryBatch(initialRecords);

  // Connect to EMQX Cloud MQTT broker
  console.log(`[MQTT] Connecting to ${MQTT_BROKER}...`);
  const mqttClient = mqtt.connect(MQTT_BROKER, {
    username: MQTT_USER,
    password: MQTT_PASS,
    clientId: 'sim_gateway_' + Math.random().toString(16).slice(2, 8)
  });

  mqttClient.on('connect', () => {
    console.log(`[MQTT] Connected to broker.`);
  });

  mqttClient.on('error', (err) => {
    console.warn(`[MQTT] Warning: ${err.message}`);
  });

  let elapsed = 0;
  const intervalSeconds = 5;

  const timer = setInterval(async () => {
    elapsed += intervalSeconds;
    const now = Math.floor(Date.now() / 1000);

    // Realistic fluctuating values around 28.5°C and 65%
    const currentTemp = +(28.2 + Math.sin(elapsed / 10) * 0.9).toFixed(1);
    const currentHum = +(64.5 + Math.cos(elapsed / 10) * 2.2).toFixed(1);

    console.log(`[SIM ${elapsed}/${durationSeconds}s] Temp: ${currentTemp} °C | Hum: ${currentHum} %RH`);

    // 1. Post to Backend Telemetry DB
    await postTelemetryBatch([
      { device_id: DEVICE_ID, value: currentTemp, unit: '°C', timestamp: now - 1 },
      { device_id: DEVICE_ID, value: currentHum, unit: '%', timestamp: now }
    ]);

    // 2. Publish to MQTT
    if (mqttClient.connected) {
      const mqttPayload = JSON.stringify({
        device_id: DEVICE_ID,
        temperature: currentTemp,
        humidity: currentHum,
        timestamp: now
      });
      mqttClient.publish(`gateway/${GATEWAY_ID}/telemetry`, mqttPayload, { qos: 1 });
    }

    if (elapsed >= durationSeconds) {
      clearInterval(timer);
      console.log(`\n=== 60-90s SIMULATION TIME ELAPSED ===`);
      if (mqttClient.connected) mqttClient.end();
      
      console.log(`[AUTO-REVERT] Restoring clean standard state now...`);
      await revertClean();
      console.log(`[DONE] Simulation completed and cleaned up successfully.`);
      process.exit(0);
    }
  }, intervalSeconds * 1000);

  // Handle Ctrl+C gracefully
  process.on('SIGINT', async () => {
    console.log(`\n[ABORT] Interrupted! Cleaning up...`);
    clearInterval(timer);
    if (mqttClient.connected) mqttClient.end();
    await revertClean();
    process.exit(0);
  });
}

run();
