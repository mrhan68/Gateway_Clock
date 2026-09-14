import React, { useState, useEffect } from 'react'
import StatusBadge from '../components/StatusBadge'
import { getActiveGateway, gatewaysAPI } from '../api/client'

function Settings() {
  const gatewayId = getActiveGateway()
  const [gateway, setGateway] = useState(null)
  const [loading, setLoading] = useState(true)
  const [saving, setSaving] = useState(false)
  const [form, setForm] = useState({
    name: '',
    timezone: '',
    firmware_version: '',
  })

  useEffect(() => {
    if (gatewayId) loadGateway()
  }, [gatewayId])

  const loadGateway = async () => {
    if (!gatewayId) return
    setLoading(true)
    try {
      const res = await gatewaysAPI.get(gatewayId)
      setGateway(res.data)
      setForm({
        name: res.data.name || '',
        timezone: res.data.timezone || '',
        firmware_version: res.data.firmware_version || '',
      })
    } catch (err) {
      console.error('Failed to load gateway:', err)
    } finally {
      setLoading(false)
    }
  }

  const handleChange = (e) => {
    setForm({ ...form, [e.target.name]: e.target.value })
  }

  const handleSave = async (e) => {
    e.preventDefault()
    setSaving(true)
    try {
      const res = await gatewaysAPI.update(gatewayId, form)
      setGateway(res.data)
      alert('Settings saved successfully')
    } catch (err) {
      alert(err.message || 'Failed to save settings')
    } finally {
      setSaving(false)
    }
  }

  if (!gatewayId) {
    return <div className="text-center py-12">No gateway selected.</div>
  }

  if (loading) {
    return <div className="text-center py-12 text-gray-500">Loading settings…</div>
  }

  return (
    <div className="space-y-6 max-w-2xl">
      <h1 className="text-2xl font-bold text-gray-800">Gateway Settings</h1>

      {/* Gateway info card */}
      <div className="bg-white rounded-lg shadow p-6">
        <div className="flex items-start justify-between">
          <div>
            <h3 className="text-lg font-semibold text-gray-800">Gateway Status</h3>
            <p className="text-sm text-gray-500 mt-2">
              ID: <span className="font-mono">{gateway?.id}</span>
            </p>
            <p className="text-sm text-gray-500">
              Firmware: {gateway?.firmware_version || 'Unknown'}
            </p>
            <p className="text-sm text-gray-500">
              Last seen: {gateway?.last_seen
                ? new Date(gateway.last_seen).toLocaleString()
                : 'Never'}
            </p>
            <p className="text-sm text-gray-500">
              Timezone: {gateway?.timezone || 'Not set'}
            </p>
          </div>
          <StatusBadge status={gateway?.status || 'offline'} />
        </div>
      </div>

      {/* Edit form */}
      <div className="bg-white rounded-lg shadow p-6">
        <h3 className="text-lg font-semibold text-gray-800 mb-4">Edit Gateway</h3>
        <form onSubmit={handleSave} className="space-y-4">
          <div>
            <label className="block text-sm font-medium text-gray-700 mb-1">
              Gateway Name
            </label>
            <input
              name="name"
              type="text"
              value={form.name}
              onChange={handleChange}
              className="w-full px-3 py-2 border rounded-md"
              required
            />
          </div>
          <div>
            <label className="block text-sm font-medium text-gray-700 mb-1">
              Timezone
            </label>
            <input
              name="timezone"
              type="text"
              value={form.timezone}
              onChange={handleChange}
              placeholder="Asia/Ho_Chi_Minh"
              className="w-full px-3 py-2 border rounded-md"
            />
          </div>
          <div>
            <label className="block text-sm font-medium text-gray-700 mb-1">
              Firmware Version (read-only)
            </label>
            <input
              name="firmware_version"
              type="text"
              value={form.firmware_version}
              onChange={handleChange}
              disabled
              className="w-full px-3 py-2 border rounded-md bg-gray-100"
            />
            <p className="text-xs text-gray-500 mt-1">
              Firmware version is reported by the gateway via MQTT and cannot be set manually.
            </p>
          </div>
          <button
            type="submit"
            disabled={saving}
            className="px-4 py-2 bg-blue-600 text-white rounded-md hover:bg-blue-700 disabled:bg-gray-300"
          >
            {saving ? 'Saving…' : 'Save Changes'}
          </button>
        </form>
      </div>

      {/* Future feature placeholder */}
      <div className="bg-white rounded-lg shadow p-6 border border-dashed border-gray-200">
        <h3 className="text-lg font-semibold text-gray-800 mb-2">OTA Updates</h3>
        <p className="text-sm text-gray-500">
          Over-the-air firmware updates will be available in a future release.
          This feature requires a firmware image upload and device-compatible
          update mechanism on the ESP32 Gateway.
        </p>
      </div>
    </div>
  )
}

export default Settings