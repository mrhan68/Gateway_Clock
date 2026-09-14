import React, { useState, useEffect } from 'react'
import ConfirmDialog from '../components/ConfirmDialog'
import { getActiveGateway, rulesAPI, devicesAPI } from '../api/client'

const CONDITION_LABELS = {
  gt: 'Greater than',
  lt: 'Less than',
  eq: 'Equal to',
}

function Rules() {
  const gatewayId = getActiveGateway()
  const [rules, setRules] = useState([])
  const [devices, setDevices] = useState([])
  const [loading, setLoading] = useState(true)
  const [showModal, setShowModal] = useState(false)
  const [editRule, setEditRule] = useState(null)
  const [deleteRule, setDeleteRule] = useState(null)

  useEffect(() => {
    if (gatewayId) {
      loadRules()
      loadDevices()
    }
  }, [gatewayId])

  const loadRules = async () => {
    if (!gatewayId) return
    setLoading(true)
    try {
      const res = await rulesAPI.list(gatewayId)
      setRules(res.data || [])
    } catch (err) {
      console.error('Failed to load rules:', err)
    } finally {
      setLoading(false)
    }
  }

  const loadDevices = async () => {
    if (!gatewayId) return
    try {
      const res = await devicesAPI.list(gatewayId)
      setDevices(res.data || [])
    } catch (err) {
      console.error('Failed to load devices:', err)
    }
  }

  const handleSubmit = async (e) => {
    e.preventDefault()
    const payload = {
      sensor_device_id: e.target.sensor_device_id.value.trim(),
      condition_type: e.target.condition_type.value,
      threshold: parseFloat(e.target.threshold.value),
      target_device_id: e.target.target_device_id.value.trim(),
      action_command: e.target.action_command.value.trim(),
      enabled: e.target.enabled.checked,
    }

    try {
      if (editRule) {
        await rulesAPI.update(gatewayId, editRule.id, payload)
      } else {
        await rulesAPI.create(gatewayId, payload)
      }
      setShowModal(false)
      setEditRule(null)
      e.target.reset()
      loadRules()
    } catch (err) {
      alert(err.message || 'Failed to save rule')
    }
  }

  const handleDelete = async () => {
    if (!deleteRule) return
    try {
      await rulesAPI.delete(gatewayId, deleteRule.id)
      setDeleteRule(null)
      loadRules()
    } catch (err) {
      alert(err.message || 'Failed to delete rule')
    }
  }

  const openEdit = (rule) => {
    setEditRule(rule)
    setShowModal(true)
  }
  const openAdd = () => {
    setEditRule(null)
    setShowModal(true)
  }

  // Devices that are actuators (can receive commands)
  const actuatorDevices = devices.filter((d) => d.device_type === 'actuator')
  // Devices that are sensors (can trigger conditions)
  const sensorDevices = devices.filter((d) => d.device_type === 'sensor')

  if (!gatewayId) {
    return <div className="text-center py-12">No gateway selected.</div>
  }

  if (loading) {
    return <div className="text-center py-12 text-gray-500">Loading rules…</div>
  }

  return (
    <div className="space-y-4">
      <div className="flex items-center justify-between">
        <h1 className="text-2xl font-bold text-gray-800">Automation Rules</h1>
        <button
          onClick={openAdd}
          className="px-4 py-2 bg-blue-600 text-white rounded-md hover:bg-blue-700"
        >
          + Add Rule
        </button>
      </div>

      {rules.length === 0 ? (
        <div className="text-center py-12 text-gray-500">
          <p>No automation rules configured.</p>
          <button
            onClick={openAdd}
            className="mt-3 px-4 py-2 bg-blue-600 text-white rounded-md hover:bg-blue-700"
          >
            Create first rule
          </button>
        </div>
      ) : (
        <div className="space-y-3">
          {rules.map((rule) => (
            <div key={rule.id} className="bg-white rounded-lg shadow p-4">
              <div className="flex items-start justify-between">
                <div className="flex-1">
                  <h3 className="font-semibold text-gray-800">
                    If <span className="text-purple-600">{rule.sensor_device_id}</span> →{' '}
                    {CONDITION_LABELS[rule.condition_type]} {rule.threshold}
                  </h3>
                  <p className="text-sm text-gray-600 mt-1">
                    Then → <span className="text-purple-600">{rule.target_device_id}</span> ={' '}
                    <span className="font-medium">{rule.action_command}</span>
                  </p>
                  <div className="mt-2">
                    {rule.enabled ? (
                      <span className="text-xs px-2 py-0.5 bg-green-100 text-green-800 rounded-full">Active</span>
                    ) : (
                      <span className="text-xs px-2 py-0.5 bg-gray-100 text-gray-600 rounded-full">Disabled</span>
                    )}
                  </div>
                </div>
                <div className="flex flex-col gap-2 ml-4">
                  <button
                    onClick={() => openEdit(rule)}
                    className="px-3 py-1 text-sm border border-gray-300 rounded hover:bg-gray-50"
                  >
                    Edit
                  </button>
                  <button
                    onClick={() => setDeleteRule(rule)}
                    className="px-3 py-1 text-sm text-red-600 border border-red-200 rounded hover:bg-red-50"
                  >
                    Delete
                  </button>
                </div>
              </div>
            </div>
          ))}
        </div>
      )}

      {/* Add/Edit Modal */}
      {showModal && (
        <div className="fixed inset-0 bg-black/40 flex items-center justify-center z-50">
          <div className="bg-white rounded-lg p-6 w-96">
            <h2 className="text-lg font-semibold mb-4">
              {editRule ? 'Edit Rule' : 'Add Rule'}
            </h2>
            <form onSubmit={handleSubmit} className="space-y-4">
              <div>
                <label className="block text-sm font-medium text-gray-700 mb-1">Sensor Device</label>
                <select name="sensor_device_id" defaultValue={editRule?.sensor_device_id || ''} className="w-full px-3 py-2 border rounded-md" required>
                  <option value="">Select sensor device</option>
                  {sensorDevices.map((d) => (
                    <option key={d.id} value={d.device_id}>{d.name} ({d.device_id})</option>
                  ))}
                </select>
              </div>
              <div className="grid grid-cols-2 gap-4">
                <div>
                  <label className="block text-sm font-medium text-gray-700 mb-1">Condition</label>
                  <select name="condition_type" defaultValue={editRule?.condition_type || 'gt'} className="w-full px-3 py-2 border rounded-md">
                    <option value="gt">Greater than (&gt;)</option>
                    <option value="lt">Less than (&lt;)</option>
                    <option value="eq">Equal to (=)</option>
                  </select>
                </div>
                <div>
                  <label className="block text-sm font-medium text-gray-700 mb-1">Threshold</label>
                  <input name="threshold" type="number" step="0.1" defaultValue={editRule?.threshold || ''} className="w-full px-3 py-2 border rounded-md" required />
                </div>
              </div>
              <div>
                <label className="block text-sm font-medium text-gray-700 mb-1">Target Device</label>
                <select name="target_device_id" defaultValue={editRule?.target_device_id || ''} className="w-full px-3 py-2 border rounded-md" required>
                  <option value="">Select target device</option>
                  {actuatorDevices.map((d) => (
                    <option key={d.id} value={d.device_id}>{d.name} ({d.device_id})</option>
                  ))}
                </select>
              </div>
              <div>
                <label className="block text-sm font-medium text-gray-700 mb-1">Action Command</label>
                <input name="action_command" type="text" placeholder="ON" defaultValue={editRule?.action_command || ''} className="w-full px-3 py-2 border rounded-md" required />
              </div>
              <label className="flex items-center gap-2">
                <input type="checkbox" name="enabled" defaultChecked={editRule?.enabled !== false} />
                <span className="text-sm font-medium">Enabled</span>
              </label>
              <div className="flex gap-3 pt-2">
                <button type="submit" className="flex-1 px-4 py-2 bg-blue-600 text-white rounded-md hover:bg-blue-700">
                  {editRule ? 'Update' : 'Create'}
                </button>
                <button
                  type="button"
                  onClick={() => { setShowModal(false); setEditRule(null) }}
                  className="flex-1 px-4 py-2 bg-gray-200 text-gray-800 rounded-md hover:bg-gray-300"
                >
                  Cancel
                </button>
              </div>
            </form>
          </div>
        </div>
      )}

      <ConfirmDialog
        isOpen={!!deleteRule}
        message={`Delete rule "If ${deleteRule?.sensor_device_id} → ${deleteRule?.action_command}"?`}
        onConfirm={handleDelete}
        onCancel={() => setDeleteRule(null)}
      />
    </div>
  )
}

export default Rules