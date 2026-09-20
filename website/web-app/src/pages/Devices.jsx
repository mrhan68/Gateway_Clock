import React, { useState, useEffect } from 'react'
import StatusBadge from '../components/StatusBadge'
import ConfirmDialog from '../components/ConfirmDialog'
import TelemetryChart from '../components/TelemetryChart'
import {
  getActiveGateway,
  devicesAPI,
  telemetryAPI,
} from '../api/client'

function Devices() {
  const gatewayId = getActiveGateway()
  const [devices, setDevices] = useState([])
  const [loading, setLoading] = useState(true)
  const [showModal, setShowModal] = useState(false)
  const [editDevice, setEditDevice] = useState(null)
  const [deleteDevice, setDeleteDevice] = useState(null)
  const [commandDevice, setCommandDevice] = useState(null)
  const [commandText, setCommandText] = useState('')
  const [togglingDevices, setTogglingDevices] = useState({})
  const [notification, setNotification] = useState(null)
  const [telemetryDevice, setTelemetryDevice] = useState(null)
  const [telemetryData, setTelemetryData] = useState([])
  const [loadingTelemetry, setLoadingTelemetry] = useState(false)

  useEffect(() => {
    if (gatewayId) {
      loadDevices()
      const interval = setInterval(loadDevices, 15000)
      return () => clearInterval(interval)
    }
  }, [gatewayId])

  const loadDevices = async () => {
    if (!gatewayId) return
    setLoading(true)
    try {
      const res = await devicesAPI.list(gatewayId)
      setDevices(res.data || [])
    } catch (err) {
      console.error('Failed to load devices:', err)
    } finally {
      setLoading(false)
    }
  }

  const handleSubmit = async (e) => {
    e.preventDefault()
    const payload = {
      device_id: e.target.device_id.value.trim(),
      name: e.target.name.value.trim(),
      device_type: e.target.device_type.value,
      protocol: e.target.protocol.value,
      mac_address: e.target.mac_address.value.trim() || null,
      mqtt_topic: e.target.mqtt_topic.value.trim() || null,
      capabilities: e.target.capabilities.value.trim() || null,
    }
    try {
      if (editDevice) {
        await devicesAPI.update(gatewayId, editDevice.device_id, payload)
      } else {
        await devicesAPI.create(gatewayId, payload)
      }
      setShowModal(false)
      setEditDevice(null)
      e.target.reset()
      loadDevices()
    } catch (err) {
      alert(err.message || 'Failed to save device')
    }
  }

  const handleDelete = async () => {
    if (!deleteDevice) return
    try {
      await devicesAPI.delete(gatewayId, deleteDevice.device_id)
      setDeleteDevice(null)
      loadDevices()
    } catch (err) {
      alert(err.message || 'Failed to delete device')
    }
  }

  const handleCommand = async () => {
    if (!commandDevice || !commandText) return
    try {
      await devicesAPI.sendCommand(gatewayId, commandDevice.device_id, commandText)
      setNotification({ type: 'success', message: `Command "${commandText}" dispatched to ${commandDevice.name} (QUEUED)` })
      setTimeout(() => setNotification(null), 4000)
      setCommandDevice(null)
      setCommandText('')
      loadDevices()
    } catch (err) {
      alert(err.message || 'Failed to send command')
    }
  }

  const handleQuickToggle = async (device) => {
    const isCurrentlyOn =
      String(device.last_value).toUpperCase() === 'ON' ||
      device.last_value === 1 ||
      device.last_value === '1' ||
      device.last_value === true
    const nextCommand = isCurrentlyOn ? 'OFF' : 'ON'

    setTogglingDevices((prev) => ({ ...prev, [device.device_id]: true }))
    try {
      await devicesAPI.sendCommand(gatewayId, device.device_id, nextCommand)
      // Optimistic update of local state
      setDevices((prev) =>
        prev.map((d) =>
          d.device_id === device.device_id ? { ...d, last_value: nextCommand } : d
        )
      )
      setNotification({
        type: 'success',
        message: `Relay ${device.name}: Switched to ${nextCommand} (Dispatched via MQTT)`,
      })
      setTimeout(() => setNotification(null), 4000)
    } catch (err) {
      setNotification({
        type: 'error',
        message: err.message || `Failed to toggle ${device.name}`,
      })
      setTimeout(() => setNotification(null), 5000)
    } finally {
      setTogglingDevices((prev) => ({ ...prev, [device.device_id]: false }))
    }
  }

  const handleOpenTelemetry = async (device) => {
    setTelemetryDevice(device)
    setLoadingTelemetry(true)
    try {
      const res = await telemetryAPI.list(gatewayId, device.device_id, { limit: 50 })
      setTelemetryData(res.data || [])
    } catch (err) {
      console.error('Failed to load telemetry:', err)
      setTelemetryData([])
    } finally {
      setLoadingTelemetry(false)
    }
  }

  const openEdit = (device) => {
    setEditDevice(device)
    setShowModal(true)
  }
  const openAdd = () => {
    setEditDevice(null)
    setShowModal(true)
  }

  if (!gatewayId) {
    return <div className="text-center py-12">No gateway selected.</div>
  }

  if (loading) {
    return <div className="text-center py-12 text-gray-500">Loading devices…</div>
  }

  return (
    <div className="space-y-4">
      {/* Header */}
      <div className="flex items-center justify-between">
        <h1 className="text-2xl font-bold text-gray-800">Devices</h1>
        <button
          onClick={openAdd}
          className="px-4 py-2 bg-blue-600 text-white rounded-md hover:bg-blue-700"
        >
          + Add Device
        </button>
      </div>

      {/* Notification Toast */}
      {notification && (
        <div
          className={`p-3 rounded-md text-sm border flex items-center justify-between transition-all ${
            notification.type === 'success'
              ? 'bg-green-50 text-green-800 border-green-200'
              : 'bg-red-50 text-red-800 border-red-200'
          }`}
        >
          <span>{notification.message}</span>
          <button
            onClick={() => setNotification(null)}
            className="text-gray-400 hover:text-gray-600 font-bold ml-3"
          >
            ✕
          </button>
        </div>
      )}

      {/* Device grid */}
      {devices.length === 0 ? (
        <div className="text-center py-12 text-gray-500">
          <p>No devices registered.</p>
          <button
            onClick={openAdd}
            className="mt-3 px-4 py-2 bg-blue-600 text-white rounded-md hover:bg-blue-700"
          >
            Add first device
          </button>
        </div>
      ) : (
        <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-4">
          {devices.map((device) => {
            const isActuator = device.device_type === 'actuator'
            const isSensor = device.device_type === 'sensor'
            const isRelayOn =
              String(device.last_value).toUpperCase() === 'ON' ||
              device.last_value === 1 ||
              device.last_value === '1' ||
              device.last_value === true
            const isToggling = !!togglingDevices[device.device_id]

            return (
              <div key={device.id} className="bg-white rounded-lg shadow p-4 flex flex-col justify-between">
                <div>
                  <div className="flex items-start justify-between">
                    <div className="flex-1">
                      <div className="flex items-center gap-2">
                        <h3 className="font-semibold text-gray-800">{device.name}</h3>
                        <span className={`text-xs px-1.5 py-0.5 rounded font-medium ${
                          isActuator ? 'bg-purple-100 text-purple-700' : 'bg-blue-100 text-blue-700'
                        }`}>
                          {device.device_type}
                        </span>
                      </div>
                      <p className="text-sm text-gray-500 font-mono mt-0.5">{device.device_id}</p>
                    </div>
                    <StatusBadge status={device.status} />
                  </div>

                  <div className="mt-3 grid grid-cols-2 gap-2 text-xs">
                    <div>
                      <span className="text-gray-500">Protocol:</span>{' '}
                      <span className="font-medium uppercase">{device.protocol}</span>
                    </div>
                    <div>
                      <span className="text-gray-500">Last seen:</span>{' '}
                      <span className="font-medium text-gray-700">
                        {device.last_seen ? new Date(device.last_seen).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' }) : 'Never'}
                      </span>
                    </div>
                    {device.mac_address && (
                      <div className="col-span-2 text-gray-500 truncate">
                        <span>MAC:</span> <span className="font-mono">{device.mac_address}</span>
                      </div>
                    )}
                    {device.mqtt_topic && (
                      <div className="col-span-2 text-gray-500 truncate">
                        <span>Topic:</span> <span className="font-mono">{device.mqtt_topic}</span>
                      </div>
                    )}
                  </div>

                  {/* Relay Toggle Switch (Actuator) */}
                  {isActuator && (
                    <div className="mt-4 p-3 bg-gray-50 rounded-lg border border-gray-100 flex items-center justify-between">
                      <div>
                        <span className="text-xs font-semibold text-gray-700 block">Relay Power</span>
                        <span className="text-xs text-gray-500">
                          State:{' '}
                          <span
                            className={`font-bold ${
                              isRelayOn ? 'text-green-600' : 'text-gray-500'
                            }`}
                          >
                            {isRelayOn ? '● ON' : '○ OFF'}
                          </span>
                        </span>
                      </div>
                      <button
                        type="button"
                        onClick={() => handleQuickToggle(device)}
                        disabled={isToggling}
                        title={`Click to switch ${isRelayOn ? 'OFF' : 'ON'}`}
                        className={`relative inline-flex h-7 w-12 flex-shrink-0 cursor-pointer rounded-full border-2 border-transparent transition-colors duration-200 ease-in-out focus:outline-none ${
                          isRelayOn ? 'bg-green-600' : 'bg-gray-300'
                        } ${isToggling ? 'opacity-60 cursor-not-allowed' : ''}`}
                      >
                        <span
                          className={`pointer-events-none inline-block h-6 w-6 transform rounded-full bg-white shadow ring-0 transition duration-200 ease-in-out ${
                            isRelayOn ? 'translate-x-5' : 'translate-x-0'
                          }`}
                        />
                      </button>
                    </div>
                  )}

                  {/* Sensor Last Reading & Telemetry Link */}
                  {isSensor && (
                    <div className="mt-4 p-3 bg-blue-50/60 rounded-lg border border-blue-100 flex items-center justify-between">
                      <div>
                        <span className="text-xs text-blue-800 font-medium block">Last Reading</span>
                        <span className="text-base font-bold text-blue-900">
                          {device.last_value !== null && device.last_value !== undefined
                            ? `${device.last_value}`
                            : 'No data'}
                        </span>
                      </div>
                      <button
                        type="button"
                        onClick={() => handleOpenTelemetry(device)}
                        className="px-2.5 py-1 text-xs bg-blue-600 text-white rounded hover:bg-blue-700 transition-colors shadow-sm"
                      >
                        📊 Telemetry
                      </button>
                    </div>
                  )}
                </div>

                {/* Card Actions */}
                <div className="mt-4 pt-3 border-t border-gray-100 flex gap-2">
                  <button
                    onClick={() => setCommandDevice(device)}
                    className="flex-1 px-2.5 py-1.5 text-xs font-medium border border-gray-300 rounded hover:bg-gray-50 text-gray-700"
                  >
                    Custom Cmd
                  </button>
                  <button
                    onClick={() => openEdit(device)}
                    className="px-2.5 py-1.5 text-xs font-medium border border-gray-300 rounded hover:bg-gray-50 text-gray-700"
                  >
                    Edit
                  </button>
                  <button
                    onClick={() => setDeleteDevice(device)}
                    className="px-2.5 py-1.5 text-xs font-medium text-red-600 border border-red-200 rounded hover:bg-red-50"
                  >
                    Delete
                  </button>
                </div>
              </div>
            )
          })}
        </div>
      )}

      {/* Add/Edit Device Modal */}
      {showModal && (
        <div className="fixed inset-0 bg-black/40 flex items-center justify-center z-50">
          <div className="bg-white rounded-lg p-6 w-96 max-h-[90vh] overflow-y-auto">
            <h2 className="text-lg font-semibold mb-4">
              {editDevice ? 'Edit Device' : 'Add Device'}
            </h2>
            <form onSubmit={handleSubmit} className="space-y-4">
              <div>
                <label className="block text-sm font-medium text-gray-700 mb-1">
                  Device ID
                </label>
                <input
                  name="device_id"
                  type="text"
                  placeholder="sensor_01"
                  defaultValue={editDevice?.device_id || ''}
                  className="w-full px-3 py-2 border rounded-md"
                  required
                />
              </div>
              <div>
                <label className="block text-sm font-medium text-gray-700 mb-1">
                  Name
                </label>
                <input
                  name="name"
                  type="text"
                  placeholder="Living Room Sensor"
                  defaultValue={editDevice?.name || ''}
                  className="w-full px-3 py-2 border rounded-md"
                  required
                />
              </div>
              <div className="grid grid-cols-2 gap-4">
                <div>
                  <label className="block text-sm font-medium text-gray-700 mb-1">
                    Type
                  </label>
                  <select
                    name="device_type"
                    defaultValue={editDevice?.device_type || 'sensor'}
                    className="w-full px-3 py-2 border rounded-md"
                  >
                    <option value="sensor">Sensor</option>
                    <option value="actuator">Actuator</option>
                  </select>
                </div>
                <div>
                  <label className="block text-sm font-medium text-gray-700 mb-1">
                    Protocol
                  </label>
                  <select
                    name="protocol"
                    defaultValue={editDevice?.protocol || 'ble'}
                    className="w-full px-3 py-2 border rounded-md"
                  >
                    <option value="ble">BLE</option>
                    <option value="mqtt">MQTT</option>
                  </select>
                </div>
              </div>
              <div>
                <label className="block text-sm font-medium text-gray-700 mb-1">
                  MAC Address (BLE only)
                </label>
                <input
                  name="mac_address"
                  type="text"
                  placeholder="AA:BB:CC:DD:EE:FF"
                  defaultValue={editDevice?.mac_address || ''}
                  className="w-full px-3 py-2 border rounded-md"
                />
              </div>
              <div>
                <label className="block text-sm font-medium text-gray-700 mb-1">
                  MQTT Topic (MQTT only)
                </label>
                <input
                  name="mqtt_topic"
                  type="text"
                  placeholder="home/living/fan"
                  defaultValue={editDevice?.mqtt_topic || ''}
                  className="w-full px-3 py-2 border rounded-md"
                />
              </div>
              <div>
                <label className="block text-sm font-medium text-gray-700 mb-1">
                  Capabilities (JSON array)
                </label>
                <input
                  name="capabilities"
                  type="text"
                  placeholder='["temperature","humidity"]'
                  defaultValue={editDevice?.capabilities || ''}
                  className="w-full px-3 py-2 border rounded-md"
                />
              </div>
              <div className="flex gap-3 pt-2">
                <button
                  type="submit"
                  className="flex-1 px-4 py-2 bg-blue-600 text-white rounded-md hover:bg-blue-700"
                >
                  {editDevice ? 'Update' : 'Create'}
                </button>
                <button
                  type="button"
                  onClick={() => {
                    setShowModal(false)
                    setEditDevice(null)
                  }}
                  className="flex-1 px-4 py-2 bg-gray-200 text-gray-800 rounded-md hover:bg-gray-300"
                >
                  Cancel
                </button>
              </div>
            </form>
          </div>
        </div>
      )}

      {/* Send Command Modal */}
      {commandDevice && (
        <div className="fixed inset-0 bg-black/40 flex items-center justify-center z-50">
          <div className="bg-white rounded-lg p-6 w-80">
            <h2 className="text-lg font-semibold mb-2">Send Command</h2>
            <p className="text-sm text-gray-500 mb-3">
              To: <span className="font-medium">{commandDevice.name}</span> ({commandDevice.device_id})
            </p>
            <input
              type="text"
              placeholder='e.g. ON, OFF, SET:50'
              value={commandText}
              onChange={(e) => setCommandText(e.target.value)}
              className="w-full px-3 py-2 border rounded-md mb-4"
            />
            <div className="flex gap-3">
              <button
                onClick={handleCommand}
                disabled={!commandText}
                className="flex-1 px-4 py-2 bg-blue-600 text-white rounded-md hover:bg-blue-700 disabled:bg-gray-300"
              >
                Send
              </button>
              <button
                onClick={() => setCommandDevice(null)}
                className="flex-1 px-4 py-2 bg-gray-200 text-gray-800 rounded-md hover:bg-gray-300"
              >
                Cancel
              </button>
            </div>
          </div>
        </div>
      )}

      {/* Sensor Telemetry Modal */}
      {telemetryDevice && (
        <div className="fixed inset-0 bg-black/40 flex items-center justify-center z-50 p-4">
          <div className="bg-white rounded-lg p-6 w-full max-w-xl shadow-xl">
            <div className="flex items-center justify-between pb-3 border-b mb-4">
              <div>
                <h3 className="text-lg font-bold text-gray-800">
                  {telemetryDevice.name} — Telemetry
                </h3>
                <p className="text-xs text-gray-500 font-mono">
                  Device ID: {telemetryDevice.device_id} | Protocol: {telemetryDevice.protocol}
                </p>
              </div>
              <button
                onClick={() => setTelemetryDevice(null)}
                className="text-gray-400 hover:text-gray-600 text-lg font-bold"
              >
                ✕
              </button>
            </div>

            <TelemetryChart
              data={telemetryData}
              title={`${telemetryDevice.name} Historical Trend`}
              unit={telemetryDevice.capabilities?.includes('humidity') ? '%' : '°C'}
              color="blue"
              loading={loadingTelemetry}
              height={220}
            />

            <div className="mt-4 flex justify-end">
              <button
                onClick={() => setTelemetryDevice(null)}
                className="px-4 py-2 bg-gray-100 hover:bg-gray-200 text-gray-700 text-xs font-medium rounded-md transition-colors"
              >
                Close
              </button>
            </div>
          </div>
        </div>
      )}

      {/* Delete Confirmation */}
      <ConfirmDialog
        isOpen={!!deleteDevice}
        message={`Delete device "${deleteDevice?.name}" (${deleteDevice?.device_id})?`}
        onConfirm={handleDelete}
        onCancel={() => setDeleteDevice(null)}
      />
    </div>
  )
}

export default Devices
