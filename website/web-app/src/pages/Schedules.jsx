import React, { useState, useEffect } from 'react'
import ConfirmDialog from '../components/ConfirmDialog'
import { getActiveGateway, schedulesAPI } from '../api/client'

const TYPE_LABELS = {
  automation: 'Automation',
  alarm: 'Alarm',
  personal_event: 'Personal Event',
}

const TYPE_COLORS = {
  automation: 'bg-purple-100 text-purple-800',
  alarm: 'bg-blue-100 text-blue-800',
  personal_event: 'bg-orange-100 text-orange-800',
}

const REPEAT_DAYS = ['MON', 'TUE', 'WED', 'THU', 'FRI', 'SAT', 'SUN']

function Schedules() {
  const gatewayId = getActiveGateway()
  const [schedules, setSchedules] = useState([])
  const [loading, setLoading] = useState(true)
  const [showModal, setShowModal] = useState(false)
  const [editSchedule, setEditSchedule] = useState(null)
  const [filterType, setFilterType] = useState('')
  const [deleteSchedule, setDeleteSchedule] = useState(null)

  useEffect(() => {
    if (gatewayId) loadSchedules()
  }, [gatewayId, filterType])

  const loadSchedules = async () => {
    if (!gatewayId) return
    setLoading(true)
    try {
      const params = filterType ? { type: filterType } : {}
      const res = await schedulesAPI.list(gatewayId, params)
      setSchedules(res.data || [])
    } catch (err) {
      console.error('Failed to load schedules:', err)
    } finally {
      setLoading(false)
    }
  }

  const handleSubmit = async (e) => {
    e.preventDefault()
    const type = e.target.type.value
    const hour = parseInt(e.target.hour.value, 10)
    const minute = parseInt(e.target.minute.value, 10)

    let repeatDays = null
    let oneTimeDate = null

    if (type === 'automation' || type === 'alarm' || type === 'personal_event') {
      const repeats = e.target.repeat_days.value
      if (repeats) {
        repeatDays = repeats
      }
      const otd = e.target.one_time_date.value
      if (otd) {
        oneTimeDate = otd
      }
    }

    const payload = {
      type,
      hour,
      minute,
      repeat_days: repeatDays,
      one_time_date: oneTimeDate,
      enabled: e.target.enabled.checked,
      target_device_id: e.target.target_device_id.value.trim() || null,
      action_command: e.target.action_command.value.trim() || null,
      title: e.target.title.value.trim() || null,
      buzzer: e.target.buzzer.checked,
    }

    try {
      if (editSchedule) {
        await schedulesAPI.update(gatewayId, editSchedule.id, payload)
      } else {
        await schedulesAPI.create(gatewayId, payload)
      }
      setShowModal(false)
      setEditSchedule(null)
      e.target.reset()
      loadSchedules()
    } catch (err) {
      alert(err.message || 'Failed to save schedule')
    }
  }

  const handleToggle = async (schedule, enabled) => {
    try {
      await schedulesAPI.toggle(gatewayId, schedule.id, enabled)
      schedule.enabled = enabled
      setSchedules([...schedules])
    } catch (err) {
      alert(err.message || 'Failed to toggle schedule')
    }
  }

  const handleDelete = async () => {
    if (!deleteSchedule) return
    try {
      await schedulesAPI.delete(gatewayId, deleteSchedule.id)
      setDeleteSchedule(null)
      loadSchedules()
    } catch (err) {
      alert(err.message || 'Failed to delete schedule')
    }
  }

  const openEdit = (schedule) => {
    setEditSchedule(schedule)
    setShowModal(true)
  }
  const openAdd = () => {
    setEditSchedule(null)
    setShowModal(true)
  }

  const formatTime = (h, m) => `${String(h).padStart(2, '0')}:${String(m).padStart(2, '0')}`
  const formatRepeat = (days) => {
    if (!days) return 'No repeat'
    const dayList = days.split(',')
    if (dayList.length === 7) return 'Every day'
    if (dayList.length >= 5) return 'Weekdays'
    return dayList.join(', ')
  }

  if (!gatewayId) {
    return <div className="text-center py-12">No gateway selected.</div>
  }

  if (loading) {
    return <div className="text-center py-12 text-gray-500">Loading schedules…</div>
  }

  return (
    <div className="space-y-4">
      <div className="flex items-center justify-between">
        <h1 className="text-2xl font-bold text-gray-800">Schedules</h1>
        <div className="flex gap-2">
          <select
            value={filterType}
            onChange={(e) => setFilterType(e.target.value)}
            className="px-3 py-2 border rounded-md text-sm bg-white"
          >
            <option value="">All Types</option>
            <option value="automation">Automation</option>
            <option value="alarm">Alarm</option>
            <option value="personal_event">Personal Event</option>
          </select>
          <button
            onClick={openAdd}
            className="px-4 py-2 bg-blue-600 text-white rounded-md hover:bg-blue-700"
          >
            + Add Schedule
          </button>
        </div>
      </div>

      {schedules.length === 0 ? (
        <div className="text-center py-12 text-gray-500">
          <p>No schedules found.</p>
          <button
            onClick={openAdd}
            className="mt-3 px-4 py-2 bg-blue-600 text-white rounded-md hover:bg-blue-700"
          >
            Create first schedule
          </button>
        </div>
      ) : (
        <div className="space-y-3">
          {schedules.map((s) => (
            <div key={s.id} className="bg-white rounded-lg shadow p-4">
              <div className="flex items-start justify-between">
                <div className="flex-1">
                  <div className="flex items-center gap-2">
                    <span className={`px-2 py-0.5 rounded-full text-xs font-medium ${TYPE_COLORS[s.type] || 'bg-gray-100 text-gray-800'}`}>
                      {TYPE_LABELS[s.type] || s.type}
                    </span>
                    <h3 className="font-semibold text-gray-800">
                      {s.title || formatTime(s.hour, s.minute)}
                    </h3>
                    {!s.enabled && (
                      <span className="text-xs text-gray-400">(disabled)</span>
                    )}
                  </div>
                  <div className="mt-2 text-sm text-gray-600">
                    Time: <span className="font-medium">{formatTime(s.hour, s.minute)}</span>
                  </div>
                  <div className="mt-1 text-sm text-gray-500">
                    Repeat: <span className="font-medium">{formatRepeat(s.repeat_days)}</span>
                  </div>
                  {s.one_time_date && (
                    <div className="mt-1 text-sm text-gray-500">
                      Date: <span className="font-medium">{s.one_time_date}</span>
                    </div>
                  )}
                  {s.target_device_id && (
                    <div className="mt-1 text-sm text-gray-500">
                      Device: <span className="font-medium">{s.target_device_id}</span>
                      {s.action_command && (
                        <span> → Command: <span className="font-medium">{s.action_command}</span></span>
                      )}
                    </div>
                  )}
                  {s.buzzer && (
                    <div className="mt-1 text-sm text-gray-500">
                      Buzzer: <span className="font-medium">Enabled</span>
                    </div>
                  )}
                </div>
                <div className="flex flex-col gap-2 ml-4">
                  <button
                    onClick={() => handleToggle(s, !s.enabled)}
                    className={`px-3 py-1 text-sm rounded-md ${
                      s.enabled
                        ? 'bg-gray-100 text-gray-700 hover:bg-gray-200'
                        : 'bg-green-100 text-green-800 hover:bg-green-200'
                    }`}
                  >
                    {s.enabled ? 'Disable' : 'Enable'}
                  </button>
                  <button
                    onClick={() => openEdit(s)}
                    className="px-3 py-1 text-sm border border-gray-300 rounded hover:bg-gray-50"
                  >
                    Edit
                  </button>
                  <button
                    onClick={() => setDeleteSchedule(s)}
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
          <div className="bg-white rounded-lg p-6 w-96 max-h-[90vh] overflow-y-auto">
            <h2 className="text-lg font-semibold mb-4">
              {editSchedule ? 'Edit Schedule' : 'Add Schedule'}
            </h2>
            <form onSubmit={handleSubmit} className="space-y-4">
              <div>
                <label className="block text-sm font-medium text-gray-700 mb-1">Type</label>
                <select name="type" defaultValue={editSchedule?.type || 'alarm'} className="w-full px-3 py-2 border rounded-md" required>
                  <option value="alarm">Alarm</option>
                  <option value="automation">Automation</option>
                  <option value="personal_event">Personal Event</option>
                </select>
              </div>
              <div className="grid grid-cols-3 gap-2">
                <div>
                  <label className="block text-sm font-medium text-gray-700 mb-1">Hour</label>
                  <input name="hour" type="number" min="0" max="23" defaultValue={editSchedule?.hour || 7} className="w-full px-3 py-2 border rounded-md" required />
                </div>
                <div>
                  <label className="block text-sm font-medium text-gray-700 mb-1">Minute</label>
                  <input name="minute" type="number" min="0" max="59" defaultValue={editSchedule?.minute || 0} className="w-full px-3 py-2 border rounded-md" required />
                </div>
                <div className="col-span-3">
                  <label className="flex items-center gap-2">
                    <input type="checkbox" name="enabled" defaultChecked={editSchedule?.enabled !== false} />
                    <span className="text-sm font-medium">Enabled</span>
                  </label>
                </div>
              </div>
              <div>
                <label className="block text-sm font-medium text-gray-700 mb-1">Repeat Days</label>
                <input
                  name="repeat_days"
                  type="text"
                  placeholder="MON,TUE,WED"
                  defaultValue={editSchedule?.repeat_days || ''}
                  className="w-full px-3 py-2 border rounded-md"
                />
                <p className="text-xs text-gray-500 mt-1">Comma-separated: MON,TUE,WED,THU,FRI,SAT,SUN</p>
              </div>
              <div>
                <label className="block text-sm font-medium text-gray-700 mb-1">One-time Date</label>
                <input
                  name="one_time_date"
                  type="date"
                  defaultValue={editSchedule?.one_time_date || ''}
                  className="w-full px-3 py-2 border rounded-md"
                />
              </div>
              <div>
                <label className="block text-sm font-medium text-gray-700 mb-1">Title</label>
                <input
                  name="title"
                  type="text"
                  placeholder="Dậy đi học"
                  defaultValue={editSchedule?.title || ''}
                  className="w-full px-3 py-2 border rounded-md"
                />
              </div>
              <div>
                <label className="block text-sm font-medium text-gray-700 mb-1">Target Device ID (automation only)</label>
                <input
                  name="target_device_id"
                  type="text"
                  placeholder="fan_01"
                  defaultValue={editSchedule?.target_device_id || ''}
                  className="w-full px-3 py-2 border rounded-md"
                />
              </div>
              <div>
                <label className="block text-sm font-medium text-gray-700 mb-1">Action Command (automation only)</label>
                <input
                  name="action_command"
                  type="text"
                  placeholder="ON"
                  defaultValue={editSchedule?.action_command || ''}
                  className="w-full px-3 py-2 border rounded-md"
                />
              </div>
              <label className="flex items-center gap-2">
                <input type="checkbox" name="buzzer" defaultChecked={editSchedule?.buzzer || false} />
                <span className="text-sm font-medium">Buzzer</span>
              </label>
              <div className="flex gap-3 pt-2">
                <button type="submit" className="flex-1 px-4 py-2 bg-blue-600 text-white rounded-md hover:bg-blue-700">
                  {editSchedule ? 'Update' : 'Create'}
                </button>
                <button
                  type="button"
                  onClick={() => { setShowModal(false); setEditSchedule(null) }}
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
        isOpen={!!deleteSchedule}
        message={`Delete schedule "${deleteSchedule?.title || formatTime(deleteSchedule?.hour, deleteSchedule?.minute)}"?`}
        onConfirm={handleDelete}
        onCancel={() => setDeleteSchedule(null)}
      />
    </div>
  )
}

export default Schedules