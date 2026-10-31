import React, { useState, useEffect } from 'react'
import {
  PlusIcon,
  AdjustmentsHorizontalIcon,
  BoltIcon,
  SignalIcon,
  ArrowRightIcon,
  PencilSquareIcon,
  TrashIcon,
  ArrowPathIcon,
  XMarkIcon,
} from '@heroicons/react/24/outline'
import ConfirmDialog from '../components/ConfirmDialog'
import Toast from '../components/Toast'
import { getActiveGateway, rulesAPI, devicesAPI } from '../api/client'

const CONDITION_SYMBOLS = {
  gt: { symbol: '>', label: 'Lớn hơn (Greater than)' },
  lt: { symbol: '<', label: 'Nhỏ hơn (Less than)' },
  eq: { symbol: '=', label: 'Bằng (Equal to)' },
}

function Rules() {
  const gatewayId = getActiveGateway()
  const [rules, setRules] = useState([])
  const [devices, setDevices] = useState([])
  const [loading, setLoading] = useState(true)
  const [showModal, setShowModal] = useState(false)
  const [editRule, setEditRule] = useState(null)
  const [deleteRule, setDeleteRule] = useState(null)
  const [notification, setNotification] = useState(null)

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
      setNotification({ type: 'error', message: err.message || 'Không thể tải danh sách quy tắc' })
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
        setNotification({ type: 'success', message: 'Đã cập nhật quy tắc tự động hóa' })
      } else {
        await rulesAPI.create(gatewayId, payload)
        setNotification({ type: 'success', message: 'Đã tạo mới quy tắc tự động hóa' })
      }
      setShowModal(false)
      setEditRule(null)
      loadRules()
    } catch (err) {
      setNotification({ type: 'error', message: err.message || 'Không thể lưu quy tắc' })
    }
  }

  const handleToggleRule = async (rule, enabled) => {
    try {
      await rulesAPI.update(gatewayId, rule.id, { ...rule, enabled })
      setRules((prev) =>
        prev.map((r) => (r.id === rule.id ? { ...r, enabled } : r))
      )
      setNotification({
        type: 'success',
        message: `Quy tắc ${rule.sensor_device_id}: ${enabled ? 'Đã kích hoạt' : 'Đã tạm dừng'}`,
      })
    } catch (err) {
      setNotification({ type: 'error', message: err.message || 'Không thể chuyển trạng thái quy tắc' })
    }
  }

  const handleDelete = async () => {
    if (!deleteRule) return
    try {
      await rulesAPI.delete(gatewayId, deleteRule.id)
      setNotification({ type: 'success', message: 'Đã xóa quy tắc tự động hóa' })
      setDeleteRule(null)
      loadRules()
    } catch (err) {
      setNotification({ type: 'error', message: err.message || 'Không thể xóa quy tắc' })
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

  const actuatorDevices = devices.filter((d) => d.device_type === 'actuator')
  const sensorDevices = devices.filter((d) => d.device_type === 'sensor')

  if (!gatewayId) {
    return (
      <div className="max-w-md mx-auto text-center py-20">
        <div className="w-12 h-12 rounded-xl bg-zinc-900 border border-zinc-800 text-zinc-400 flex items-center justify-center mx-auto mb-4">
          <AdjustmentsHorizontalIcon className="w-6 h-6" />
        </div>
        <h2 className="text-base font-semibold text-zinc-100">Chưa chọn Gateway</h2>
        <p className="text-xs text-zinc-400 mt-1.5">
          Vui lòng chọn Gateway trên thanh điều hướng để xem quy tắc tự động hóa.
        </p>
      </div>
    )
  }

  return (
    <div className="space-y-6">
      {/* Toast Notification */}
      {notification && (
        <Toast
          message={notification.message}
          type={notification.type}
          onClose={() => setNotification(null)}
        />
      )}

      {/* Header Bar */}
      <div className="flex flex-col sm:flex-row sm:items-center justify-between gap-4">
        <div>
          <div className="flex items-center gap-2.5">
            <h1 className="text-xl font-bold tracking-tight text-zinc-100">Quy Tắc Tự Động Hóa</h1>
            <span className="text-xs font-mono px-2 py-0.5 rounded-full bg-zinc-800 text-zinc-300 border border-zinc-700">
              {rules.length}
            </span>
          </div>
          <p className="text-xs text-zinc-400 mt-1">
            Bộ máy Rules Engine cục bộ trên ESP32: Tự động kích hoạt rơ-le khi cảm biến đo vượt ngưỡng an toàn.
          </p>
        </div>

        <div className="flex items-center gap-3">
          <button
            onClick={() => {
              loadRules()
              loadDevices()
            }}
            disabled={loading}
            className="p-2 rounded-lg bg-dark-900 border border-zinc-800 text-zinc-400 hover:text-zinc-200 hover:bg-zinc-800/80 transition-colors focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-emerald-500"
            title="Làm mới danh sách"
          >
            <ArrowPathIcon className={`w-4 h-4 ${loading ? 'animate-spin text-emerald-400' : ''}`} />
          </button>
          <button
            onClick={openAdd}
            className="inline-flex items-center gap-2 px-3.5 py-2 bg-emerald-500 hover:bg-emerald-400 text-dark-950 font-semibold text-xs rounded-lg transition-colors shadow-[0_0_15px_rgba(52,211,153,0.25)] focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-emerald-400"
          >
            <PlusIcon className="w-4 h-4 stroke-[2.5]" />
            <span>Thêm quy tắc</span>
          </button>
        </div>
      </div>

      {/* Rules List */}
      {loading && rules.length === 0 ? (
        <div className="space-y-3">
          {[...Array(3)].map((_, i) => (
            <div key={i} className="h-28 bg-dark-900 border border-zinc-800 rounded-xl animate-pulse" />
          ))}
        </div>
      ) : rules.length === 0 ? (
        <div className="bg-dark-900 border border-zinc-800 rounded-xl p-12 text-center">
          <div className="w-12 h-12 rounded-xl bg-zinc-950 border border-zinc-800 text-zinc-500 flex items-center justify-center mx-auto mb-3">
            <AdjustmentsHorizontalIcon className="w-6 h-6" />
          </div>
          <h3 className="text-sm font-semibold text-zinc-200">Chưa có quy tắc tự động hóa</h3>
          <p className="text-xs text-zinc-400 mt-1 max-w-sm mx-auto">
            Tạo quy tắc để tự động bật quạt khi nhiệt độ phòng quá cao hoặc ngắt thiết bị khi độ ẩm vượt ngưỡng.
          </p>
          <button
            onClick={openAdd}
            className="mt-4 inline-flex items-center gap-2 px-3.5 py-2 bg-emerald-500 hover:bg-emerald-400 text-dark-950 font-semibold text-xs rounded-lg transition-colors shadow-sm"
          >
            <PlusIcon className="w-4 h-4 stroke-[2.5]" />
            <span>Tạo quy tắc đầu tiên</span>
          </button>
        </div>
      ) : (
        <div className="space-y-3.5">
          {rules.map((rule) => {
            const cond = CONDITION_SYMBOLS[rule.condition_type] || { symbol: '?', label: rule.condition_type }
            const sensorObj = devices.find((d) => d.device_id === rule.sensor_device_id)
            const targetObj = devices.find((d) => d.device_id === rule.target_device_id)

            return (
              <div
                key={rule.id}
                className={`bg-dark-900 border rounded-xl p-4.5 transition-all duration-150 flex flex-col md:flex-row md:items-center justify-between gap-4 ${
                  rule.enabled
                    ? 'border-zinc-800 hover:border-zinc-700/80 shadow-sm'
                    : 'border-zinc-900/80 opacity-70 bg-dark-950/60'
                }`}
              >
                <div className="flex-1 min-w-0">
                  {/* Logic Flow Diagram */}
                  <div className="flex flex-wrap items-center gap-2">
                    {/* IF Block */}
                    <span className="text-xs font-bold text-zinc-500 uppercase tracking-wider">
                      NẾU (IF)
                    </span>

                    {/* Sensor Badge */}
                    <div className="inline-flex items-center gap-1.5 px-2.5 py-1 rounded-lg bg-sky-950/40 border border-sky-800/40 text-sky-300 text-xs font-medium">
                      <SignalIcon className="w-3.5 h-3.5 text-sky-400" />
                      <span className="font-mono">{sensorObj?.name || rule.sensor_device_id}</span>
                    </div>

                    {/* Condition & Threshold */}
                    <div className="inline-flex items-center gap-1 px-2.5 py-1 rounded-lg bg-amber-950/40 border border-amber-800/40 text-amber-300 text-xs font-mono font-semibold">
                      <span className="text-amber-400 text-sm font-bold">{cond.symbol}</span>
                      <span className="tabular-nums">{rule.threshold}</span>
                    </div>

                    {/* THEN Connector */}
                    <div className="flex items-center gap-1 text-zinc-500 px-1">
                      <ArrowRightIcon className="w-3.5 h-3.5 text-zinc-500" />
                      <span className="text-xs font-bold text-zinc-500 uppercase tracking-wider">
                        THÌ (THEN)
                      </span>
                    </div>

                    {/* Target Actuator & Command */}
                    <div className="inline-flex items-center gap-1.5 px-2.5 py-1 rounded-lg bg-emerald-950/40 border border-emerald-800/40 text-emerald-300 text-xs font-medium">
                      <BoltIcon className="w-3.5 h-3.5 text-emerald-400" />
                      <span className="font-mono">{targetObj?.name || rule.target_device_id}</span>
                      <span className="text-zinc-600">=</span>
                      <span className="font-bold text-emerald-400 font-mono">{rule.action_command}</span>
                    </div>
                  </div>

                  {/* Subtext info */}
                  <div className="mt-2 flex items-center gap-3 text-xs text-zinc-500">
                    <span>
                      Loại: <span className="font-medium text-zinc-400">{cond.label}</span>
                    </span>
                    <span>•</span>
                    <span>
                      Độ trễ kích hoạt: <span className="font-mono text-zinc-400">&lt; 100ms (cục bộ)</span>
                    </span>
                  </div>
                </div>

                {/* Right Actions & Toggle */}
                <div className="flex items-center justify-end gap-3 flex-shrink-0 pt-3 md:pt-0 border-t md:border-t-0 border-zinc-800/80">
                  {/* Tactile Enable / Disable switch */}
                  <div className="flex items-center gap-2">
                    <span className="text-[11px] font-mono text-zinc-400">
                      {rule.enabled ? 'Đang chạy' : 'Đã ngắt'}
                    </span>
                    <button
                      type="button"
                      onClick={() => handleToggleRule(rule, !rule.enabled)}
                      aria-label="Kích hoạt/vô hiệu hóa quy tắc"
                      className={`relative inline-flex h-6 w-11 flex-shrink-0 cursor-pointer rounded-full border-2 border-transparent transition-colors duration-200 ease-in-out focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-emerald-500 ${
                        rule.enabled ? 'bg-emerald-500' : 'bg-zinc-700'
                      }`}
                    >
                      <span
                        className={`pointer-events-none inline-block h-5 w-5 transform rounded-full bg-white shadow ring-0 transition duration-200 ease-in-out ${
                          rule.enabled ? 'translate-x-5' : 'translate-x-0'
                        }`}
                      />
                    </button>
                  </div>

                  <div className="h-4 w-px bg-zinc-800 mx-1" />

                  {/* Edit button */}
                  <button
                    onClick={() => openEdit(rule)}
                    className="p-1.5 text-zinc-400 hover:text-zinc-200 bg-zinc-800/60 hover:bg-zinc-800 border border-zinc-750 rounded-lg transition-colors focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-zinc-400"
                    title="Chỉnh sửa quy tắc"
                  >
                    <PencilSquareIcon className="w-4 h-4" />
                  </button>

                  {/* Delete button */}
                  <button
                    onClick={() => setDeleteRule(rule)}
                    className="p-1.5 text-rose-400 hover:text-rose-300 bg-rose-500/10 hover:bg-rose-500/20 border border-rose-500/20 rounded-lg transition-colors focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-rose-500"
                    title="Xóa quy tắc"
                  >
                    <TrashIcon className="w-4 h-4" />
                  </button>
                </div>
              </div>
            )
          })}
        </div>
      )}

      {/* Add / Edit Modal */}
      {showModal && (
        <div className="fixed inset-0 bg-black/75 backdrop-blur-sm flex items-center justify-center z-50 p-4">
          <div className="bg-dark-900 border border-zinc-800 rounded-2xl p-6 w-full max-w-md shadow-2xl text-zinc-100 max-h-[90vh] overflow-y-auto animate-in fade-in zoom-in-95 duration-150">
            <div className="flex items-center justify-between pb-3 border-b border-zinc-800 mb-4">
              <h2 className="text-base font-semibold text-zinc-100 flex items-center gap-2">
                <AdjustmentsHorizontalIcon className="w-5 h-5 text-emerald-400" />
                {editRule ? 'Chỉnh sửa quy tắc' : 'Tạo quy tắc mới'}
              </h2>
              <button
                onClick={() => {
                  setShowModal(false)
                  setEditRule(null)
                }}
                className="text-zinc-400 hover:text-zinc-200 p-1 rounded-lg"
              >
                <XMarkIcon className="w-5 h-5" />
              </button>
            </div>

            <form onSubmit={handleSubmit} className="space-y-4">
              {/* Trigger Section */}
              <div className="space-y-3 p-3 bg-zinc-950/60 border border-zinc-800 rounded-xl">
                <span className="text-[11px] font-semibold text-sky-400 uppercase tracking-wider block">
                  1. Cảm biến kích hoạt (Trigger)
                </span>
                <div>
                  <label className="block text-xs font-medium text-zinc-300 mb-1">
                    Chọn cảm biến đầu vào *
                  </label>
                  <select
                    name="sensor_device_id"
                    defaultValue={editRule?.sensor_device_id || ''}
                    className="w-full px-3 py-2 text-xs bg-zinc-900 border border-zinc-750 rounded-lg text-zinc-100 focus:outline-none focus:border-emerald-500"
                    required
                  >
                    <option value="">-- Chọn cảm biến --</option>
                    {sensorDevices.map((d) => (
                      <option key={d.id} value={d.device_id}>
                        {d.name} ({d.device_id})
                      </option>
                    ))}
                    {/* Fallback if sensors not loaded */}
                    {sensorDevices.length === 0 && (
                      <option value="temp_sensor_01">temp_sensor_01</option>
                    )}
                  </select>
                </div>

                <div className="grid grid-cols-2 gap-3">
                  <div>
                    <label className="block text-xs font-medium text-zinc-300 mb-1">
                      Điều kiện
                    </label>
                    <select
                      name="condition_type"
                      defaultValue={editRule?.condition_type || 'gt'}
                      className="w-full px-3 py-2 text-xs bg-zinc-900 border border-zinc-750 rounded-lg text-zinc-100 focus:outline-none focus:border-emerald-500"
                    >
                      <option value="gt">Lớn hơn (&gt;)</option>
                      <option value="lt">Nhỏ hơn (&lt;)</option>
                      <option value="eq">Bằng (=)</option>
                    </select>
                  </div>
                  <div>
                    <label className="block text-xs font-medium text-zinc-300 mb-1">
                      Ngưỡng giá trị (Threshold) *
                    </label>
                    <input
                      name="threshold"
                      type="number"
                      step="0.1"
                      placeholder="vd: 35.5"
                      defaultValue={editRule?.threshold ?? ''}
                      className="w-full px-3 py-2 text-xs bg-zinc-900 border border-zinc-750 rounded-lg text-zinc-100 font-mono tabular-nums focus:outline-none focus:border-emerald-500"
                      required
                    />
                  </div>
                </div>
              </div>

              {/* Action Section */}
              <div className="space-y-3 p-3 bg-zinc-950/60 border border-zinc-800 rounded-xl">
                <span className="text-[11px] font-semibold text-emerald-400 uppercase tracking-wider block">
                  2. Hành động thực thi (Action)
                </span>
                <div>
                  <label className="block text-xs font-medium text-zinc-300 mb-1">
                    Thiết bị nhận lệnh (Rơ-le / Chấp hành) *
                  </label>
                  <select
                    name="target_device_id"
                    defaultValue={editRule?.target_device_id || ''}
                    className="w-full px-3 py-2 text-xs bg-zinc-900 border border-zinc-750 rounded-lg text-zinc-100 focus:outline-none focus:border-emerald-500"
                    required
                  >
                    <option value="">-- Chọn thiết bị chấp hành --</option>
                    {actuatorDevices.map((d) => (
                      <option key={d.id} value={d.device_id}>
                        {d.name} ({d.device_id})
                      </option>
                    ))}
                    {actuatorDevices.length === 0 && (
                      <option value="relay_1">relay_1 (GPIO 15)</option>
                    )}
                  </select>
                </div>

                <div>
                  <label className="block text-xs font-medium text-zinc-300 mb-1">
                    Lệnh thực thi *
                  </label>
                  <input
                    name="action_command"
                    type="text"
                    placeholder="ON, OFF hoặc SET:50"
                    defaultValue={editRule?.action_command || 'ON'}
                    className="w-full px-3 py-2 text-xs bg-zinc-900 border border-zinc-750 rounded-lg text-zinc-100 font-mono focus:outline-none focus:border-emerald-500"
                    required
                  />
                </div>
              </div>

              {/* Enabled toggle */}
              <div className="pt-1">
                <label className="flex items-center gap-2 cursor-pointer select-none">
                  <input
                    type="checkbox"
                    name="enabled"
                    defaultChecked={editRule?.enabled !== false}
                    className="w-4 h-4 rounded border-zinc-700 bg-zinc-950 text-emerald-500 focus:ring-emerald-500"
                  />
                  <span className="text-xs text-zinc-300 font-medium">
                    Kích hoạt quy tắc này ngay sau khi lưu
                  </span>
                </label>
              </div>

              {/* Form Buttons */}
              <div className="flex gap-3 pt-3 border-t border-zinc-800">
                <button
                  type="button"
                  onClick={() => {
                    setShowModal(false)
                    setEditRule(null)
                  }}
                  className="flex-1 px-4 py-2 bg-zinc-800 hover:bg-zinc-750 text-zinc-300 rounded-lg text-xs font-medium transition-colors border border-zinc-700"
                >
                  Hủy
                </button>
                <button
                  type="submit"
                  className="flex-1 px-4 py-2 bg-emerald-500 hover:bg-emerald-400 text-dark-950 rounded-lg text-xs font-semibold transition-colors shadow-sm focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-emerald-400"
                >
                  {editRule ? 'Lưu thay đổi' : 'Tạo quy tắc'}
                </button>
              </div>
            </form>
          </div>
        </div>
      )}

      {/* Delete Confirmation */}
      <ConfirmDialog
        isOpen={!!deleteRule}
        message={`Bạn có chắc muốn xóa quy tắc tự động hóa nếu ${deleteRule?.sensor_device_id} → ${deleteRule?.action_command}?`}
        confirmText="Xóa quy tắc"
        onConfirm={handleDelete}
        onCancel={() => setDeleteRule(null)}
      />
    </div>
  )
}

export default Rules