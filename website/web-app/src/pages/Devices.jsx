import React, { useState, useEffect } from 'react'
import StatusBadge from '../components/StatusBadge'
import ConfirmDialog from '../components/ConfirmDialog'
import {
  getActiveGateway,
  devicesAPI,
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
      setCommandDevice(null)
      setCommandText('')
    } catch (err) {
      alert(err.message || 'Failed to send command')
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
          {devices.map((device) => (
            <div key={device.id} className="bg-white rounded-lg shadow p-4">
              <div className="flex items-start justify-between">
                <div className="flex-1">
                  <h3 className="font-semibold text-gray-800">{device.name}</h3>
                  <p className="text-sm text-gray-500 font-mono mt-0.5">{device.device_id}</p>
                </div>
                <StatusBadge status={device.status} />
              </div>

              <div className="mt-3 grid grid-cols-2 gap-3 text-sm">
                <div>
                  <span className="text-gray-500">Type:</span> {device.device_type}
                </div>
                <div>
                  <span className="text-gray-500">Protocol:</span> {device.protocol}
                </div>
                {device.last_value !== null && device.last_value !== undefined && (
                  <div className="col-span-2">
                    <span className="text-gray-500">Last Value:</span>{' '}
                    <span className="font-medium">{device.last_value}</span>
                  </div>
                )}
                {device.mac_address && (
                  <div className="col-span-2">
                    <span className="text-gray-500">MAC:</span> {device.mac_address}
                  </div>
                )}
              </div>

              <div className="mt-4 flex gap-2">
                <button
                  onClick={() => setCommandDevice(device)}
                  className="flex-1 px-3 py-1.5 text-sm border border-gray-300 rounded hover:bg-gray-50"
                >
                  Send Command
                </button>
                <button
                  onClick={() => openEdit(device)}
                  className="flex-1 px-3 py-1.5 text-sm border border-gray-300 rounded hover:bg-gray-50"
                >
                  Edit
                </button>
                <button
                  onClick={() => setDeleteDevice(device)}
                  className="px-3 py-1.5 text-sm text-red-600 border border-red-200 rounded hover:bg-red-50"
                >
                  Delete
                </button>
              </div>
            </div>
          ))}
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
