/**
 * REST-to-MQTT Relay
 *
 * Bridges HTTP (REST API calls from Web App) to MQTT (commands sent via broker
 * to the Gateway → device).
 *
 * When the Web App POSTs to /devices/{id}/command, the controller calls
 * relay.sendCommand(), which publishes to the MQTT topic:
 *   gateway/{gatewayId}/device/{deviceId}/command
 *
 * The Gateway (ESP32) is subscribed to this topic and forwards the command
 * to the appropriate protocol adapter (BLE GATT write or MQTT device publish).
 */
const broker = require('./broker');

/**
 * Send a device command from REST API through the MQTT broker.
 *
 * @param {Object} params
 * @param {string} params.gatewayId - The gateway that owns this device
 * @param {string} params.deviceId - The device to command
 * @param {string} params.command - The command string (e.g., "ON", "OFF", "SET:75")
 * @returns {Promise<{success: boolean, message: string}>}
 */
async function sendCommand({ gatewayId, deviceId, command }) {
  // Check if gateway is online before sending
  const online = await broker.isGatewayOnline(gatewayId);
  if (!online) {
    return {
      success: false,
      message: `Gateway '${gatewayId}' is currently offline. Command cannot be sent.`,
      offline: true
    };
  }

  // Publish via the in-process MQTT broker
  const published = await broker.publishDeviceCommand(gatewayId, deviceId, command);

  if (!published) {
    return {
      success: false,
      message: `Failed to publish command to MQTT broker for device '${deviceId}'`
    };
  }

  return {
    success: true,
    message: `Command '${command}' sent to device '${deviceId}' via gateway '${gatewayId}'`
  };
}

module.exports = {
  sendCommand
};
