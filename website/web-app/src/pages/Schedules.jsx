import React, { useState, useEffect } from 'react'
import {
  PlusIcon,
  ClockIcon,
  BellAlertIcon,
  BoltIcon,
  CalendarDaysIcon,
  PencilSquareIcon,
  TrashIcon,
  ArrowPathIcon,
  FunnelIcon,
  XMarkIcon,
  SpeakerWaveIcon,
} from '@heroicons/react/24/outline'
import ConfirmDialog from '../components/ConfirmDialog'
import Toast from '../components/Toast'
import { getActiveGateway, schedulesAPI, devicesAPI } from '../api/client'

const TYPE_CONFIG = {
  alarm: {
    label: 'Báo thức (Alarm)',
    badge: 'bg-sky-500/10 text-sky-400 border-sky-500/20',
    icon: BellAlertIcon,
  },
  automation: {
    label: 'Tự động hóa (Automation)',
    badge: 'bg-cyan-500/10 text-cyan-400 border-cyan-500/20',
    icon: BoltIcon,
  },
  personal_event: {
    label: 'Sự kiện (Event)',
    badge: 'bg-amber-500/10 text-amber-400 border-amber-500/20',
    icon: CalendarDaysIcon,
  },
}

const ALL_DAYS = [
  { key: 'MON', label: 'T2' },
  { key: 'TUE', label: 'T3' },
  { key: 'WED', label: 'T4' },
  { key: 'THU', label: 'T5' },
  { key: 'FRI', label: 'T6' },
  { key: 'SAT', label: 'T7' },
  { key: 'SUN', label: 'CN' },
]

function Schedules() {
  const gatewayId = getActiveGateway()
  const [schedules, setSchedules] = useState([])
  const [actuators, setActuators] = useState([])
  const [loading, setLoading] = useState(true)
  const [showModal, setShowModal] = useState(false)
  const [editSchedule, setEditSchedule] = useState(null)
  const [filterType, setFilterType] = useState('all')
  const [deleteSchedule, setDeleteSchedule] = useState(null)
  const [notification, setNotification] = useState(null)

  // Form modal state
  const [modalType, setModalType] = useState('alarm')
  const [selectedDays, setSelectedDays] = useState([])

  useEffect(() => {
    if (gatewayId) {
      loadSchedules()
      loadActuatorDevices()
    }
  }, [gatewayId, filterType])

  const loadSchedules = async () => {
    if (!gatewayId) return
    setLoading(true)
    try {
      const params = filterType !== 'all' ? { type: filterType } : {}
      const res = await schedulesAPI.list(gatewayId, params)
      setSchedules(res.data || [])
    } catch (err) {
      console.error('Failed to load schedules:', err)
      setNotification({ type: 'error', message: err.message || 'Không thể tải danh sách lịch trình' })
    } finally {
      setLoading(false)
    }
  }

  const loadActuatorDevices = async () => {
    if (!gatewayId) return
    try {
      const res = await devicesAPI.list(gatewayId)
      const list = res.data || []
      setActuators(list.filter((d) => d.device_type === 'actuator'))
    } catch (err) {
      console.error('Failed to load actuator devices:', err)
    }
  }

  const handleOpenAdd = () => {
    setEditSchedule(null)
    setModalType('alarm')
    setSelectedDays(['MON', 'TUE', 'WED', 'THU', 'FRI'])
    setShowModal(true)
  }

  const handleOpenEdit = (schedule) => {
    setEditSchedule(schedule)
    setModalType(schedule.type || 'alarm')
    setSelectedDays(schedule.repeat_days ? schedule.repeat_days.split(',') : [])
    setShowModal(true)
  }

  const toggleDaySelection = (dayKey) => {
    setSelectedDays((prev) =>
      prev.includes(dayKey) ? prev.filter((d) => d !== dayKey) : [...prev, dayKey]
    )
  }

  const handleSubmit = async (e) => {
    e.preventDefault()
    const type = modalType
    const hour = parseInt(e.target.hour.value, 10)
    const minute = parseInt(e.target.minute.value, 10)

    const repeatDays = selectedDays.length > 0 ? selectedDays.join(',') : null
    const oneTimeDate = e.target.one_time_date?.value || null

    const payload = {
      type,
      hour,
      minute,
      repeat_days: repeatDays,
      one_time_date: oneTimeDate,
      enabled: e.target.enabled.checked,
      target_device_id: type === 'automation' ? (e.target.target_device_id?.value.trim() || null) : null,
      action_command: type === 'automation' ? (e.target.action_command?.value.trim() || null) : null,
      title: e.target.title.value.trim() || null,
      buzzer: e.target.buzzer.checked,
    }

    try {
      if (editSchedule) {
        await schedulesAPI.update(gatewayId, editSchedule.id, payload)
        setNotification({ type: 'success', message: `Đã cập nhật lịch trình thành công` })
      } else {
        await schedulesAPI.create(gatewayId, payload)
        setNotification({ type: 'success', message: `Đã tạo lịch hẹn mới lúc ${formatTime(hour, minute)}` })
      }
      setShowModal(false)
      setEditSchedule(null)
      loadSchedules()
    } catch (err) {
      setNotification({ type: 'error', message: err.message || 'Không thể lưu lịch trình' })
    }
  }

  const handleToggle = async (schedule, enabled) => {
    try {
      await schedulesAPI.toggle(gatewayId, schedule.id, enabled)
      setSchedules((prev) =>
        prev.map((s) => (s.id === schedule.id ? { ...s, enabled } : s))
      )
      setNotification({
        type: 'success',
        message: `Lịch ${schedule.title || formatTime(schedule.hour, schedule.minute)}: ${enabled ? 'Đã bật' : 'Đã tắt'}`,
      })
    } catch (err) {
      setNotification({ type: 'error', message: err.message || 'Lỗi cập nhật trạng thái lịch trình' })
    }
  }

  const handleDelete = async () => {
    if (!deleteSchedule) return
    try {
      await schedulesAPI.delete(gatewayId, deleteSchedule.id)
      setNotification({ type: 'success', message: `Đã xóa lịch trình` })
      setDeleteSchedule(null)
      loadSchedules()
    } catch (err) {
      setNotification({ type: 'error', message: err.message || 'Không thể xóa lịch trình' })
    }
  }

  const formatTime = (h, m) =>
    `${String(h).padStart(2, '0')}:${String(m).padStart(2, '0')}`

  if (!gatewayId) {
    return (
      <div className="max-w-md mx-auto text-center py-20">
        <div className="w-12 h-12 rounded-xl bg-zinc-900 border border-zinc-800 text-zinc-400 flex items-center justify-center mx-auto mb-4">
          <ClockIcon className="w-6 h-6" />
        </div>
        <h2 className="text-base font-semibold text-zinc-100">Chưa chọn Gateway</h2>
        <p className="text-xs text-zinc-400 mt-1.5">
          Vui lòng chọn Gateway trên thanh điều hướng để xem lịch trình.
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
            <h1 className="text-xl font-bold tracking-tight text-zinc-100">Lịch Trình & Báo Thức</h1>
            <span className="text-xs font-mono px-2 py-0.5 rounded-full bg-zinc-800 text-zinc-300 border border-zinc-700">
              {schedules.length}
            </span>
          </div>
          <p className="text-xs text-zinc-400 mt-1">
            Đồng bộ báo thức RTC, còi chip buzzer và kích hoạt rơ-le định kỳ theo thời gian thực.
          </p>
        </div>

        <div className="flex items-center gap-3">
          <button
            onClick={loadSchedules}
            disabled={loading}
            className="p-2 rounded-lg bg-dark-900 border border-zinc-800 text-zinc-400 hover:text-zinc-200 hover:bg-zinc-800/80 transition-colors focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-emerald-500"
            title="Làm mới danh sách"
          >
            <ArrowPathIcon className={`w-4 h-4 ${loading ? 'animate-spin text-emerald-400' : ''}`} />
          </button>
          <button
            onClick={handleOpenAdd}
            className="inline-flex items-center gap-2 px-3.5 py-2 bg-emerald-500 hover:bg-emerald-400 text-dark-950 font-semibold text-xs rounded-lg transition-colors shadow-[0_0_15px_rgba(52,211,153,0.25)] focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-emerald-400"
          >
            <PlusIcon className="w-4 h-4 stroke-[2.5]" />
            <span>Thêm lịch trình</span>
          </button>
        </div>
      </div>

      {/* Filter Tabs */}
      <div className="flex items-center gap-2 bg-dark-900 border border-zinc-800 p-2.5 rounded-xl">
        <span className="text-xs text-zinc-500 font-medium pl-1 flex items-center gap-1.5">
          <FunnelIcon className="w-3.5 h-3.5" /> Bộ lọc:
        </span>
        <div className="flex flex-wrap items-center gap-1.5">
          {[
            { id: 'all', label: 'Tất cả' },
            { id: 'alarm', label: 'Báo thức (Alarm)' },
            { id: 'automation', label: 'Tự động hóa' },
            { id: 'personal_event', label: 'Sự kiện cá nhân' },
          ].map((tab) => (
            <button
              key={tab.id}
              onClick={() => setFilterType(tab.id)}
              className={`px-3 py-1 rounded-lg text-xs font-medium transition-colors ${
                filterType === tab.id
                  ? 'bg-zinc-800 text-zinc-100 border border-zinc-700 shadow-sm'
                  : 'text-zinc-400 hover:text-zinc-200 hover:bg-zinc-850'
              }`}
            >
              {tab.label}
            </button>
          ))}
        </div>
      </div>

      {/* Schedule Cards List */}
      {loading && schedules.length === 0 ? (
        <div className="space-y-3">
          {[...Array(4)].map((_, i) => (
            <div key={i} className="h-28 bg-dark-900 border border-zinc-800 rounded-xl animate-pulse" />
          ))}
        </div>
      ) : schedules.length === 0 ? (
        <div className="bg-dark-900 border border-zinc-800 rounded-xl p-12 text-center">
          <div className="w-12 h-12 rounded-xl bg-zinc-950 border border-zinc-800 text-zinc-500 flex items-center justify-center mx-auto mb-3">
            <ClockIcon className="w-6 h-6" />
          </div>
          <h3 className="text-sm font-semibold text-zinc-200">Không có lịch trình nào</h3>
          <p className="text-xs text-zinc-400 mt-1 max-w-sm mx-auto">
            {filterType !== 'all'
              ? 'Không tìm thấy lịch trình theo bộ lọc đã chọn. Hãy chuyển sang Tất cả hoặc tạo mới.'
              : 'Chưa có lịch hẹn giờ hoặc báo thức nào được cấu hình cho Gateway này.'}
          </p>
          {filterType === 'all' && (
            <button
              onClick={handleOpenAdd}
              className="mt-4 inline-flex items-center gap-2 px-3.5 py-2 bg-emerald-500 hover:bg-emerald-400 text-dark-950 font-semibold text-xs rounded-lg transition-colors shadow-sm"
            >
              <PlusIcon className="w-4 h-4 stroke-[2.5]" />
              <span>Tạo lịch trình đầu tiên</span>
            </button>
          )}
        </div>
      ) : (
        <div className="space-y-3.5">
          {schedules.map((s) => {
            const typeConfig = TYPE_CONFIG[s.type] || TYPE_CONFIG.alarm
            const Icon = typeConfig.icon
            const repeatList = s.repeat_days ? s.repeat_days.split(',') : []

            return (
              <div
                key={s.id}
                className={`bg-dark-900 border rounded-xl p-4.5 transition-all duration-150 flex flex-col md:flex-row md:items-center justify-between gap-4 ${
                  s.enabled
                    ? 'border-zinc-800 hover:border-zinc-700/80 shadow-sm'
                    : 'border-zinc-900/80 opacity-70 bg-dark-950/60'
                }`}
              >
                <div className="flex items-start sm:items-center gap-4 flex-1 min-w-0">
                  {/* Digital Clock Display */}
                  <div className="flex-shrink-0 text-center bg-zinc-950/80 border border-zinc-800/80 px-4 py-2 rounded-xl">
                    <div
                      className={`text-2xl font-bold font-mono tabular-nums tracking-wider ${
                        s.enabled ? 'text-zinc-100' : 'text-zinc-500'
                      }`}
                    >
                      {formatTime(s.hour, s.minute)}
                    </div>
                    <div className="text-[10px] text-zinc-500 uppercase font-mono tracking-wider mt-0.5">
                      {s.hour < 12 ? 'SÁNG (AM)' : 'CHIỀU (PM)'}
                    </div>
                  </div>

                  {/* Schedule Details */}
                  <div className="flex-1 min-w-0 space-y-1.5">
                    <div className="flex flex-wrap items-center gap-2">
                      <span
                        className={`inline-flex items-center gap-1 text-[11px] font-semibold px-2 py-0.5 rounded-full border ${typeConfig.badge}`}
                      >
                        <Icon className="w-3.5 h-3.5" />
                        <span>{typeConfig.label}</span>
                      </span>

                      <h3 className="text-sm font-semibold text-zinc-100 truncate">
                        {s.title || 'Lịch định kỳ'}
                      </h3>

                      {!s.enabled && (
                        <span className="text-[10px] font-mono uppercase px-2 py-0.5 bg-zinc-800 text-zinc-400 rounded-md border border-zinc-700">
                          Tạm tắt
                        </span>
                      )}
                    </div>

                    {/* Target device & actions for automation */}
                    {s.target_device_id && (
                      <div className="flex items-center gap-1.5 text-xs text-zinc-400 font-mono">
                        <span>Thiết bị:</span>
                        <span className="bg-zinc-800 text-zinc-200 px-2 py-0.5 rounded text-[11px] border border-zinc-700">
                          {s.target_device_id}
                        </span>
                        {s.action_command && (
                          <>
                            <span className="text-zinc-600">→</span>
                            <span className="text-emerald-400 font-bold bg-emerald-500/10 px-2 py-0.5 rounded text-[11px] border border-emerald-500/20">
                              {s.action_command}
                            </span>
                          </>
                        )}
                      </div>
                    )}

                    {/* Buzzer and date badges */}
                    <div className="flex flex-wrap items-center gap-2 text-xs">
                      {s.buzzer && (
                        <span className="inline-flex items-center gap-1 text-[11px] text-amber-400 bg-amber-500/10 px-2 py-0.5 rounded-full border border-amber-500/20">
                          <SpeakerWaveIcon className="w-3 h-3" />
                          <span>Còi Buzzer</span>
                        </span>
                      )}
                      {s.one_time_date && (
                        <span className="inline-flex items-center gap-1 text-[11px] text-zinc-400 bg-zinc-800 px-2 py-0.5 rounded-full border border-zinc-700 font-mono">
                          <CalendarDaysIcon className="w-3 h-3 text-zinc-400" />
                          <span>{s.one_time_date}</span>
                        </span>
                      )}

                      {/* Repeat day pills */}
                      <div className="flex items-center gap-1">
                        {ALL_DAYS.map((day) => {
                          const isActive = repeatList.includes(day.key)
                          return (
                            <span
                              key={day.key}
                              className={`w-5 h-5 rounded-md flex items-center justify-center text-[10px] font-mono font-semibold transition-colors ${
                                isActive
                                  ? 'bg-emerald-500/20 text-emerald-400 border border-emerald-500/30'
                                  : 'bg-zinc-950 text-zinc-600 border border-zinc-850'
                              }`}
                              title={day.key}
                            >
                              {day.label}
                            </span>
                          )
                        })}
                      </div>
                    </div>
                  </div>
                </div>

                {/* Right Actions & Toggle */}
                <div className="flex items-center justify-end gap-3 flex-shrink-0 pt-3 md:pt-0 border-t md:border-t-0 border-zinc-800/80">
                  {/* Tactile Enable / Disable switch */}
                  <div className="flex items-center gap-2">
                    <span className="text-[11px] font-mono text-zinc-400">
                      {s.enabled ? 'Bật' : 'Tắt'}
                    </span>
                    <button
                      type="button"
                      onClick={() => handleToggle(s, !s.enabled)}
                      aria-label={`Bật/tắt lịch trình ${s.title}`}
                      className={`relative inline-flex h-6 w-11 flex-shrink-0 cursor-pointer rounded-full border-2 border-transparent transition-colors duration-200 ease-in-out focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-emerald-500 ${
                        s.enabled ? 'bg-emerald-500' : 'bg-zinc-700'
                      }`}
                    >
                      <span
                        className={`pointer-events-none inline-block h-5 w-5 transform rounded-full bg-white shadow ring-0 transition duration-200 ease-in-out ${
                          s.enabled ? 'translate-x-5' : 'translate-x-0'
                        }`}
                      />
                    </button>
                  </div>

                  <div className="h-4 w-px bg-zinc-800 mx-1" />

                  {/* Edit button */}
                  <button
                    onClick={() => handleOpenEdit(s)}
                    className="p-1.5 text-zinc-400 hover:text-zinc-200 bg-zinc-800/60 hover:bg-zinc-800 border border-zinc-750 rounded-lg transition-colors focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-zinc-400"
                    title="Chỉnh sửa lịch"
                  >
                    <PencilSquareIcon className="w-4 h-4" />
                  </button>

                  {/* Delete button */}
                  <button
                    onClick={() => setDeleteSchedule(s)}
                    className="p-1.5 text-rose-400 hover:text-rose-300 bg-rose-500/10 hover:bg-rose-500/20 border border-rose-500/20 rounded-lg transition-colors focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-rose-500"
                    title="Xóa lịch"
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
                <ClockIcon className="w-5 h-5 text-emerald-400" />
                {editSchedule ? 'Chỉnh sửa lịch trình' : 'Tạo lịch trình mới'}
              </h2>
              <button
                onClick={() => {
                  setShowModal(false)
                  setEditSchedule(null)
                }}
                className="text-zinc-400 hover:text-zinc-200 p-1 rounded-lg"
              >
                <XMarkIcon className="w-5 h-5" />
              </button>
            </div>

            <form onSubmit={handleSubmit} className="space-y-4">
              <div>
                <label className="block text-xs font-medium text-zinc-300 mb-1.5">
                  Loại lịch trình
                </label>
                <select
                  value={modalType}
                  onChange={(e) => setModalType(e.target.value)}
                  className="w-full px-3 py-2 text-xs bg-zinc-950 border border-zinc-800 rounded-lg text-zinc-100 focus:outline-none focus:border-emerald-500 focus:ring-1 focus:ring-emerald-500"
                >
                  <option value="alarm">Báo thức (Alarm) - Chuông OLED & Còi Buzzer</option>
                  <option value="automation">Tự động hóa (Automation) - Bật/Tắt Rơ-le</option>
                  <option value="personal_event">Sự kiện cá nhân (Personal Event)</option>
                </select>
              </div>

              <div>
                <label className="block text-xs font-medium text-zinc-300 mb-1.5">
                  Tiêu đề / Ghi chú
                </label>
                <input
                  name="title"
                  type="text"
                  placeholder="ví dụ: Báo thức buổi sáng, Tưới cây, v.v..."
                  defaultValue={editSchedule?.title || ''}
                  className="w-full px-3 py-2 text-xs bg-zinc-950 border border-zinc-800 rounded-lg text-zinc-100 placeholder-zinc-600 focus:outline-none focus:border-emerald-500 focus:ring-1 focus:ring-emerald-500"
                />
              </div>

              {/* Time Picker */}
              <div className="grid grid-cols-2 gap-3 p-3 bg-zinc-950/60 border border-zinc-800 rounded-xl">
                <div>
                  <label className="block text-[11px] font-medium text-zinc-400 mb-1">
                    Giờ (0 - 23) *
                  </label>
                  <input
                    name="hour"
                    type="number"
                    min="0"
                    max="23"
                    defaultValue={editSchedule?.hour ?? 7}
                    className="w-full px-3 py-2 text-sm font-mono tabular-nums bg-zinc-900 border border-zinc-750 rounded-lg text-zinc-100 focus:outline-none focus:border-emerald-500 focus:ring-1 focus:ring-emerald-500"
                    required
                  />
                </div>
                <div>
                  <label className="block text-[11px] font-medium text-zinc-400 mb-1">
                    Phút (0 - 59) *
                  </label>
                  <input
                    name="minute"
                    type="number"
                    min="0"
                    max="59"
                    defaultValue={editSchedule?.minute ?? 0}
                    className="w-full px-3 py-2 text-sm font-mono tabular-nums bg-zinc-900 border border-zinc-750 rounded-lg text-zinc-100 focus:outline-none focus:border-emerald-500 focus:ring-1 focus:ring-emerald-500"
                    required
                  />
                </div>
              </div>

              {/* Repeat Days Selection */}
              <div>
                <div className="flex items-center justify-between mb-1.5">
                  <label className="text-xs font-medium text-zinc-300">
                    Lặp lại các ngày trong tuần
                  </label>
                  <div className="flex gap-2 text-[11px]">
                    <button
                      type="button"
                      onClick={() => setSelectedDays(['MON', 'TUE', 'WED', 'THU', 'FRI', 'SAT', 'SUN'])}
                      className="text-emerald-400 hover:underline"
                    >
                      Cả tuần
                    </button>
                    <span className="text-zinc-600">•</span>
                    <button
                      type="button"
                      onClick={() => setSelectedDays(['MON', 'TUE', 'WED', 'THU', 'FRI'])}
                      className="text-emerald-400 hover:underline"
                    >
                      Ngày thường
                    </button>
                    <span className="text-zinc-600">•</span>
                    <button
                      type="button"
                      onClick={() => setSelectedDays([])}
                      className="text-zinc-500 hover:underline"
                    >
                      Xóa
                    </button>
                  </div>
                </div>

                <div className="grid grid-cols-7 gap-1.5">
                  {ALL_DAYS.map((day) => {
                    const isSelected = selectedDays.includes(day.key)
                    return (
                      <button
                        key={day.key}
                        type="button"
                        onClick={() => toggleDaySelection(day.key)}
                        className={`py-2 rounded-lg text-xs font-mono font-semibold transition-all border ${
                          isSelected
                            ? 'bg-emerald-500 text-dark-950 border-emerald-400 shadow-[0_0_10px_rgba(52,211,153,0.3)]'
                            : 'bg-zinc-950 text-zinc-400 border-zinc-800 hover:border-zinc-700'
                        }`}
                      >
                        {day.label}
                      </button>
                    )
                  })}
                </div>
              </div>

              {/* One-time Date */}
              <div>
                <label className="block text-xs font-medium text-zinc-300 mb-1.5">
                  Ngày cụ thể (Tùy chọn nếu chạy một lần)
                </label>
                <input
                  name="one_time_date"
                  type="date"
                  defaultValue={editSchedule?.one_time_date || ''}
                  className="w-full px-3 py-2 text-xs bg-zinc-950 border border-zinc-800 rounded-lg text-zinc-100 font-mono focus:outline-none focus:border-emerald-500 focus:ring-1 focus:ring-emerald-500"
                />
              </div>

              {/* Automation fields */}
              {modalType === 'automation' && (
                <div className="space-y-3 p-3 bg-sky-950/20 border border-sky-800/30 rounded-xl">
                  <div>
                    <label className="block text-xs font-medium text-sky-300 mb-1">
                      Thiết bị chấp hành mục tiêu
                    </label>
                    <select
                      name="target_device_id"
                      defaultValue={editSchedule?.target_device_id || ''}
                      className="w-full px-3 py-2 text-xs bg-zinc-950 border border-zinc-800 rounded-lg text-zinc-100 focus:outline-none focus:border-sky-500"
                    >
                      <option value="">-- Chọn rơ-le / thiết bị --</option>
                      {actuators.map((d) => (
                        <option key={d.device_id} value={d.device_id}>
                          {d.name} ({d.device_id})
                        </option>
                      ))}
                    </select>
                  </div>
                  <div>
                    <label className="block text-xs font-medium text-sky-300 mb-1">
                      Lệnh thực thi
                    </label>
                    <input
                      name="action_command"
                      type="text"
                      placeholder="ON hoặc OFF"
                      defaultValue={editSchedule?.action_command || 'ON'}
                      className="w-full px-3 py-2 text-xs bg-zinc-950 border border-zinc-800 rounded-lg text-zinc-100 font-mono focus:outline-none focus:border-sky-500"
                    />
                  </div>
                </div>
              )}

              {/* Checkboxes */}
              <div className="flex items-center justify-between pt-1">
                <label className="flex items-center gap-2 cursor-pointer select-none">
                  <input
                    type="checkbox"
                    name="buzzer"
                    defaultChecked={editSchedule?.buzzer || false}
                    className="w-4 h-4 rounded border-zinc-700 bg-zinc-950 text-emerald-500 focus:ring-emerald-500"
                  />
                  <span className="text-xs text-zinc-300 font-medium">Bật còi Buzzer</span>
                </label>

                <label className="flex items-center gap-2 cursor-pointer select-none">
                  <input
                    type="checkbox"
                    name="enabled"
                    defaultChecked={editSchedule?.enabled !== false}
                    className="w-4 h-4 rounded border-zinc-700 bg-zinc-950 text-emerald-500 focus:ring-emerald-500"
                  />
                  <span className="text-xs text-zinc-300 font-medium">Kích hoạt lịch</span>
                </label>
              </div>

              {/* Form Buttons */}
              <div className="flex gap-3 pt-3 border-t border-zinc-800">
                <button
                  type="button"
                  onClick={() => {
                    setShowModal(false)
                    setEditSchedule(null)
                  }}
                  className="flex-1 px-4 py-2 bg-zinc-800 hover:bg-zinc-750 text-zinc-300 rounded-lg text-xs font-medium transition-colors border border-zinc-700"
                >
                  Hủy
                </button>
                <button
                  type="submit"
                  className="flex-1 px-4 py-2 bg-emerald-500 hover:bg-emerald-400 text-dark-950 rounded-lg text-xs font-semibold transition-colors shadow-sm focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-emerald-400"
                >
                  {editSchedule ? 'Lưu thay đổi' : 'Tạo lịch trình'}
                </button>
              </div>
            </form>
          </div>
        </div>
      )}

      {/* Delete Confirmation */}
      <ConfirmDialog
        isOpen={!!deleteSchedule}
        message={`Bạn có chắc muốn xóa lịch "${deleteSchedule?.title || formatTime(deleteSchedule?.hour, deleteSchedule?.minute)}"?`}
        confirmText="Xóa lịch trình"
        onConfirm={handleDelete}
        onCancel={() => setDeleteSchedule(null)}
      />
    </div>
  )
}

export default Schedules