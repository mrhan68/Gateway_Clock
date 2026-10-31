import React, { useState, useEffect } from 'react'
import {
  PlusIcon,
  CpuChipIcon,
  BoltIcon,
  ChartBarIcon,
  CommandLineIcon,
  PencilSquareIcon,
  TrashIcon,
  ArrowPathIcon,
  FunnelIcon,
  XMarkIcon,
} from '@heroicons/react/24/outline'
import StatusBadge from '../components/StatusBadge'
import ConfirmDialog from '../components/ConfirmDialog'
import TelemetryChart from '../components/TelemetryChart'
import Toast from '../components/Toast'
import {
  getActiveGateway,
  devicesAPI,
  telemetryAPI,
} from '../api/client'

function Devices() {
  const gatewayId = getActiveGateway()
  const [devices, setDevices] = useState([])
  const [loading, setLoading] = useState(true)
  const [filterType, setFilterType] = useState('all')
  const [searchQuery, setSearchQuery] = useState('')
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
      setNotification({ type: 'error', message: err.message || 'Không thể tải danh sách thiết bị' })
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
        setNotification({ type: 'success', message: `Đã cập nhật thiết bị "${payload.name}" thành công` })
      } else {
        await devicesAPI.create(gatewayId, payload)
        setNotification({ type: 'success', message: `Đã thêm thiết bị mới "${payload.name}"` })
      }
      setShowModal(false)
      setEditDevice(null)
      e.target.reset()
      loadDevices()
    } catch (err) {
      setNotification({ type: 'error', message: err.message || 'Không thể lưu thông tin thiết bị' })
    }
  }

  const handleDelete = async () => {
    if (!deleteDevice) return
    try {
      await devicesAPI.delete(gatewayId, deleteDevice.device_id)
      setNotification({ type: 'success', message: `Đã xóa thiết bị "${deleteDevice.name}"` })
      setDeleteDevice(null)
      loadDevices()
    } catch (err) {
      setNotification({ type: 'error', message: err.message || 'Không thể xóa thiết bị' })
    }
  }

  const handleCommand = async () => {
    if (!commandDevice || !commandText) return
    try {
      await devicesAPI.sendCommand(gatewayId, commandDevice.device_id, commandText)
      setNotification({
        type: 'success',
        message: `Lệnh "${commandText}" đã gửi đến ${commandDevice.name} (Hàng đợi MQTT)`,
      })
      setCommandDevice(null)
      setCommandText('')
      loadDevices()
    } catch (err) {
      setNotification({ type: 'error', message: err.message || 'Không thể gửi lệnh điều khiển' })
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
      // Optimistic state update
      setDevices((prev) =>
        prev.map((d) =>
          d.device_id === device.device_id ? { ...d, last_value: nextCommand } : d
        )
      )
      setNotification({
        type: 'success',
        message: `Rơ-le ${device.name}: Chuyển sang trạng thái ${nextCommand}`,
      })
    } catch (err) {
      setNotification({
        type: 'error',
        message: err.message || `Lỗi khi chuyển trạng thái ${device.name}`,
      })
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

  const filteredDevices = devices.filter((device) => {
    const matchesType =
      filterType === 'all' ? true : device.device_type === filterType
    const matchesSearch =
      !searchQuery ||
      device.name.toLowerCase().includes(searchQuery.toLowerCase()) ||
      device.device_id.toLowerCase().includes(searchQuery.toLowerCase())
    return matchesType && matchesSearch
  })

  if (!gatewayId) {
    return (
      <div className="max-w-md mx-auto text-center py-20">
        <div className="w-12 h-12 rounded-xl bg-zinc-900 border border-zinc-800 text-zinc-400 flex items-center justify-center mx-auto mb-4">
          <CpuChipIcon className="w-6 h-6" />
        </div>
        <h2 className="text-base font-semibold text-zinc-100">Chưa chọn Gateway</h2>
        <p className="text-xs text-zinc-400 mt-1.5">
          Vui lòng chọn Gateway trên thanh điều hướng để xem danh sách thiết bị.
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
            <h1 className="text-xl font-bold tracking-tight text-zinc-100">Thiết bị Ngoại Vi</h1>
            <span className="text-xs font-mono px-2 py-0.5 rounded-full bg-zinc-800 text-zinc-300 border border-zinc-700">
              {devices.length}
            </span>
          </div>
          <p className="text-xs text-zinc-400 mt-1">
            Quản lý các rơ-le chấp hành, cảm biến môi trường BLE và thiết bị ngoại vi kết nối Gateway.
          </p>
        </div>

        <div className="flex items-center gap-3">
          <button
            onClick={loadDevices}
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
            <span>Thêm thiết bị</span>
          </button>
        </div>
      </div>

      {/* Filter and Search Bar */}
      <div className="flex flex-col sm:flex-row sm:items-center justify-between gap-3 bg-dark-900 border border-zinc-800 p-3 rounded-xl">
        <div className="flex items-center gap-2">
          <span className="text-xs text-zinc-500 font-medium pl-1 flex items-center gap-1.5">
            <FunnelIcon className="w-3.5 h-3.5" /> Phân loại:
          </span>
          <div className="flex items-center gap-1.5">
            {[
              { id: 'all', label: 'Tất cả' },
              { id: 'actuator', label: 'Rơ-le / Chấp hành' },
              { id: 'sensor', label: 'Cảm biến' },
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

        <div className="relative">
          <input
            type="text"
            placeholder="Tìm theo tên hoặc ID..."
            value={searchQuery}
            onChange={(e) => setSearchQuery(e.target.value)}
            className="w-full sm:w-60 px-3 py-1.5 text-xs bg-zinc-950 border border-zinc-800 rounded-lg text-zinc-100 placeholder-zinc-500 focus:outline-none focus:border-emerald-500 focus:ring-1 focus:ring-emerald-500 transition-colors"
          />
          {searchQuery && (
            <button
              onClick={() => setSearchQuery('')}
              className="absolute right-2.5 top-2 text-zinc-500 hover:text-zinc-300"
            >
              <XMarkIcon className="w-3.5 h-3.5" />
            </button>
          )}
        </div>
      </div>

      {/* Device Grid */}
      {loading && devices.length === 0 ? (
        <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-4">
          {[...Array(6)].map((_, i) => (
            <div key={i} className="h-56 bg-dark-900 border border-zinc-800 rounded-xl animate-pulse" />
          ))}
        </div>
      ) : filteredDevices.length === 0 ? (
        <div className="bg-dark-900 border border-zinc-800 rounded-xl p-12 text-center">
          <div className="w-12 h-12 rounded-xl bg-zinc-950 border border-zinc-800 text-zinc-500 flex items-center justify-center mx-auto mb-3">
            <CpuChipIcon className="w-6 h-6" />
          </div>
          <h3 className="text-sm font-semibold text-zinc-200">Không tìm thấy thiết bị</h3>
          <p className="text-xs text-zinc-400 mt-1 max-w-sm mx-auto">
            {searchQuery || filterType !== 'all'
              ? 'Không có thiết bị nào phù hợp với bộ lọc hiện tại. Thử xóa tìm kiếm hoặc đổi tiêu chí.'
              : 'Chưa có thiết bị nào được ghép nối với Gateway này. Nhấn nút bên dưới để thêm.'}
          </p>
          {(!searchQuery && filterType === 'all') && (
            <button
              onClick={openAdd}
              className="mt-4 inline-flex items-center gap-2 px-3.5 py-2 bg-emerald-500 hover:bg-emerald-400 text-dark-950 font-semibold text-xs rounded-lg transition-colors shadow-sm"
            >
              <PlusIcon className="w-4 h-4 stroke-[2.5]" />
              <span>Thêm thiết bị đầu tiên</span>
            </button>
          )}
        </div>
      ) : (
        <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-4">
          {filteredDevices.map((device) => {
            const isActuator = device.device_type === 'actuator'
            const isSensor = device.device_type === 'sensor'
            const isRelayOn =
              String(device.last_value).toUpperCase() === 'ON' ||
              device.last_value === 1 ||
              device.last_value === '1' ||
              device.last_value === true
            const isToggling = !!togglingDevices[device.device_id]

            return (
              <div
                key={device.id || device.device_id}
                className="bg-dark-900 border border-zinc-800 hover:border-zinc-700/80 rounded-xl p-4.5 shadow-sm transition-all duration-150 flex flex-col justify-between"
              >
                <div>
                  {/* Card Header */}
                  <div className="flex items-start justify-between gap-3">
                    <div className="flex-1 min-w-0">
                      <div className="flex items-center gap-2">
                        <div className={`w-2 h-2 rounded-full ${isRelayOn ? 'bg-emerald-400 shadow-[0_0_8px_rgba(52,211,153,0.8)] animate-pulse' : 'bg-zinc-600'}`} />
                        <h3 className="text-sm font-semibold text-zinc-100 truncate" title={device.name}>
                          {device.name}
                        </h3>
                      </div>
                      <p className="text-xs text-zinc-500 font-mono mt-0.5 truncate">
                        ID: {device.device_id}
                      </p>
                    </div>

                    <div className="flex items-center gap-2 flex-shrink-0">
                      <span
                        className={`text-[10px] uppercase font-bold tracking-wider px-2 py-0.5 rounded-full border ${
                          isActuator
                            ? 'bg-cyan-500/10 text-cyan-400 border-cyan-500/20'
                            : 'bg-sky-500/10 text-sky-400 border-sky-500/20'
                        }`}
                      >
                        {isActuator ? 'Actuator' : 'Sensor'}
                      </span>
                      <StatusBadge status={device.status} />
                    </div>
                  </div>

                  {/* Device Specs Grid */}
                  <div className="mt-3.5 pt-3 border-t border-zinc-800/80 grid grid-cols-2 gap-2 text-xs">
                    <div>
                      <span className="text-zinc-500 block text-[11px]">Giao thức</span>
                      <span className="font-mono text-zinc-300 uppercase font-medium">
                        {device.protocol}
                      </span>
                    </div>
                    <div>
                      <span className="text-zinc-500 block text-[11px]">Cập nhật lần cuối</span>
                      <span className="font-mono text-zinc-300 tabular-nums">
                        {device.last_seen
                          ? new Date(device.last_seen).toLocaleTimeString('vi-VN', {
                              hour: '2-digit',
                              minute: '2-digit',
                              second: '2-digit',
                            })
                          : 'Chưa có'}
                      </span>
                    </div>
                    {device.mac_address && (
                      <div className="col-span-2">
                        <span className="text-zinc-500 block text-[11px]">Địa chỉ MAC</span>
                        <span className="font-mono text-zinc-400 text-[11px] truncate block">
                          {device.mac_address}
                        </span>
                      </div>
                    )}
                    {device.mqtt_topic && (
                      <div className="col-span-2">
                        <span className="text-zinc-500 block text-[11px]">MQTT Topic</span>
                        <span className="font-mono text-zinc-400 text-[11px] truncate block">
                          {device.mqtt_topic}
                        </span>
                      </div>
                    )}
                  </div>

                  {/* Interactive Relay Control Area */}
                  {isActuator && (
                    <div className="mt-4 p-3 rounded-lg bg-zinc-950/70 border border-zinc-800/90 flex items-center justify-between">
                      <div>
                        <div className="flex items-center gap-1.5">
                          <BoltIcon className={`w-3.5 h-3.5 ${isRelayOn ? 'text-emerald-400' : 'text-zinc-500'}`} />
                          <span className="text-xs font-medium text-zinc-300">Nguồn Rơ-le</span>
                        </div>
                        <div className="mt-1 flex items-center gap-1.5 font-mono text-xs">
                          <span
                            className={`font-semibold ${
                              isRelayOn ? 'text-emerald-400' : 'text-zinc-500'
                            }`}
                          >
                            {isRelayOn ? '● ĐANG BẬT (ON)' : '○ ĐANG TẮT (OFF)'}
                          </span>
                        </div>
                      </div>

                      {/* Tactile Toggle Switch */}
                      <button
                        type="button"
                        onClick={() => handleQuickToggle(device)}
                        disabled={isToggling}
                        aria-label={`Bật/tắt ${device.name}`}
                        className={`relative inline-flex h-6 w-11 flex-shrink-0 cursor-pointer rounded-full border-2 border-transparent transition-colors duration-200 ease-in-out focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-emerald-500 ${
                          isRelayOn ? 'bg-emerald-500' : 'bg-zinc-700'
                        } ${isToggling ? 'opacity-60 cursor-not-allowed' : ''}`}
                      >
                        <span
                          className={`pointer-events-none inline-block h-5 w-5 transform rounded-full bg-white shadow ring-0 transition duration-200 ease-in-out ${
                            isRelayOn ? 'translate-x-5' : 'translate-x-0'
                          }`}
                        />
                      </button>
                    </div>
                  )}

                  {/* Sensor Reading & Telemetry Link */}
                  {isSensor && (
                    <div className="mt-4 p-3 rounded-lg bg-sky-950/20 border border-sky-800/30 flex items-center justify-between">
                      <div>
                        <span className="text-[11px] text-sky-400/80 font-medium block">Giá trị đọc</span>
                        <div className="text-xl font-bold font-mono text-sky-300 tabular-nums">
                          {device.last_value !== null && device.last_value !== undefined
                            ? `${device.last_value}`
                            : 'Không có dữ liệu'}
                        </div>
                      </div>
                      <button
                        type="button"
                        onClick={() => handleOpenTelemetry(device)}
                        className="inline-flex items-center gap-1.5 px-3 py-1.5 text-xs font-medium bg-sky-500/10 hover:bg-sky-500/20 text-sky-300 border border-sky-500/20 rounded-lg transition-colors focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-sky-500"
                      >
                        <ChartBarIcon className="w-3.5 h-3.5" />
                        <span>Đồ thị</span>
                      </button>
                    </div>
                  )}
                </div>

                {/* Card Action Buttons */}
                <div className="mt-4 pt-3 border-t border-zinc-800/80 flex items-center gap-2">
                  <button
                    onClick={() => setCommandDevice(device)}
                    className="flex-1 inline-flex items-center justify-center gap-1.5 px-2.5 py-1.5 text-xs font-medium bg-zinc-800 hover:bg-zinc-750 text-zinc-300 border border-zinc-700/80 rounded-lg transition-colors focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-zinc-400"
                    title="Gửi lệnh tùy chỉnh qua MQTT/BLE"
                  >
                    <CommandLineIcon className="w-3.5 h-3.5 text-zinc-400" />
                    <span>Lệnh</span>
                  </button>
                  <button
                    onClick={() => openEdit(device)}
                    className="p-1.5 text-zinc-400 hover:text-zinc-200 bg-zinc-800/50 hover:bg-zinc-800 border border-zinc-750 rounded-lg transition-colors focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-zinc-400"
                    title="Chỉnh sửa thông số thiết bị"
                    aria-label="Sửa thiết bị"
                  >
                    <PencilSquareIcon className="w-4 h-4" />
                  </button>
                  <button
                    onClick={() => setDeleteDevice(device)}
                    className="p-1.5 text-rose-400 hover:text-rose-300 bg-rose-500/10 hover:bg-rose-500/20 border border-rose-500/20 rounded-lg transition-colors focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-rose-500"
                    title="Xóa thiết bị"
                    aria-label="Xóa thiết bị"
                  >
                    <TrashIcon className="w-4 h-4" />
                  </button>
                </div>
              </div>
            )
          })}
        </div>
      )}

      {/* Add / Edit Device Modal */}
      {showModal && (
        <div className="fixed inset-0 bg-black/75 backdrop-blur-sm flex items-center justify-center z-50 p-4">
          <div className="bg-dark-900 border border-zinc-800 rounded-2xl p-6 w-full max-w-lg shadow-2xl text-zinc-100 max-h-[90vh] overflow-y-auto animate-in fade-in zoom-in-95 duration-150">
            <div className="flex items-center justify-between pb-3 border-b border-zinc-800 mb-4">
              <h2 className="text-base font-semibold text-zinc-100 flex items-center gap-2">
                <CpuChipIcon className="w-5 h-5 text-emerald-400" />
                {editDevice ? 'Chỉnh sửa thiết bị' : 'Đăng ký thiết bị mới'}
              </h2>
              <button
                onClick={() => {
                  setShowModal(false)
                  setEditDevice(null)
                }}
                className="text-zinc-400 hover:text-zinc-200 p-1 rounded-lg"
              >
                <XMarkIcon className="w-5 h-5" />
              </button>
            </div>

            <form onSubmit={handleSubmit} className="space-y-4">
              <div>
                <label className="block text-xs font-medium text-zinc-300 mb-1.5">
                  Device ID (Định danh duy nhất) *
                </label>
                <input
                  name="device_id"
                  type="text"
                  placeholder="ví dụ: temp_sensor_01 hoặc relay_light"
                  defaultValue={editDevice?.device_id || ''}
                  disabled={!!editDevice}
                  className="w-full px-3 py-2 text-xs bg-zinc-950 border border-zinc-800 rounded-lg text-zinc-100 font-mono placeholder-zinc-600 focus:outline-none focus:border-emerald-500 focus:ring-1 focus:ring-emerald-500 disabled:opacity-50"
                  required
                  spellCheck={false}
                />
              </div>

              <div>
                <label className="block text-xs font-medium text-zinc-300 mb-1.5">
                  Tên hiển thị *
                </label>
                <input
                  name="name"
                  type="text"
                  placeholder="ví dụ: Cảm biến Phòng khách"
                  defaultValue={editDevice?.name || ''}
                  className="w-full px-3 py-2 text-xs bg-zinc-950 border border-zinc-800 rounded-lg text-zinc-100 placeholder-zinc-600 focus:outline-none focus:border-emerald-500 focus:ring-1 focus:ring-emerald-500"
                  required
                />
              </div>

              <div className="grid grid-cols-2 gap-4">
                <div>
                  <label className="block text-xs font-medium text-zinc-300 mb-1.5">
                    Loại thiết bị
                  </label>
                  <select
                    name="device_type"
                    defaultValue={editDevice?.device_type || 'sensor'}
                    className="w-full px-3 py-2 text-xs bg-zinc-950 border border-zinc-800 rounded-lg text-zinc-100 focus:outline-none focus:border-emerald-500 focus:ring-1 focus:ring-emerald-500"
                  >
                    <option value="sensor">Cảm biến (Sensor)</option>
                    <option value="actuator">Chấp hành (Actuator)</option>
                  </select>
                </div>
                <div>
                  <label className="block text-xs font-medium text-zinc-300 mb-1.5">
                    Giao thức kết nối
                  </label>
                  <select
                    name="protocol"
                    defaultValue={editDevice?.protocol || 'ble'}
                    className="w-full px-3 py-2 text-xs bg-zinc-950 border border-zinc-800 rounded-lg text-zinc-100 focus:outline-none focus:border-emerald-500 focus:ring-1 focus:ring-emerald-500"
                  >
                    <option value="ble">Bluetooth Low Energy (BLE)</option>
                    <option value="mqtt">MQTT Sub/Pub</option>
                  </select>
                </div>
              </div>

              <div>
                <label className="block text-xs font-medium text-zinc-300 mb-1.5">
                  Địa chỉ MAC (Bắt buộc nếu dùng BLE)
                </label>
                <input
                  name="mac_address"
                  type="text"
                  placeholder="AA:BB:CC:DD:EE:FF"
                  defaultValue={editDevice?.mac_address || ''}
                  className="w-full px-3 py-2 text-xs bg-zinc-950 border border-zinc-800 rounded-lg text-zinc-100 font-mono placeholder-zinc-600 focus:outline-none focus:border-emerald-500 focus:ring-1 focus:ring-emerald-500"
                  spellCheck={false}
                />
              </div>

              <div>
                <label className="block text-xs font-medium text-zinc-300 mb-1.5">
                  MQTT Topic (Tùy chọn cho MQTT)
                </label>
                <input
                  name="mqtt_topic"
                  type="text"
                  placeholder="home/living/sensor"
                  defaultValue={editDevice?.mqtt_topic || ''}
                  className="w-full px-3 py-2 text-xs bg-zinc-950 border border-zinc-800 rounded-lg text-zinc-100 font-mono placeholder-zinc-600 focus:outline-none focus:border-emerald-500 focus:ring-1 focus:ring-emerald-500"
                  spellCheck={false}
                />
              </div>

              <div>
                <label className="block text-xs font-medium text-zinc-300 mb-1.5">
                  Khả năng / Capabilities (JSON array)
                </label>
                <input
                  name="capabilities"
                  type="text"
                  placeholder='["temperature", "humidity"]'
                  defaultValue={Array.isArray(editDevice?.capabilities) ? JSON.stringify(editDevice.capabilities) : (editDevice?.capabilities || '')}
                  className="w-full px-3 py-2 text-xs bg-zinc-950 border border-zinc-800 rounded-lg text-zinc-100 font-mono placeholder-zinc-600 focus:outline-none focus:border-emerald-500 focus:ring-1 focus:ring-emerald-500"
                  spellCheck={false}
                />
              </div>

              <div className="flex gap-3 pt-3 border-t border-zinc-800">
                <button
                  type="button"
                  onClick={() => {
                    setShowModal(false)
                    setEditDevice(null)
                  }}
                  className="flex-1 px-4 py-2 bg-zinc-800 hover:bg-zinc-750 text-zinc-300 rounded-lg text-xs font-medium transition-colors border border-zinc-700"
                >
                  Hủy
                </button>
                <button
                  type="submit"
                  className="flex-1 px-4 py-2 bg-emerald-500 hover:bg-emerald-400 text-dark-950 rounded-lg text-xs font-semibold transition-colors shadow-sm focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-emerald-400"
                >
                  {editDevice ? 'Lưu thay đổi' : 'Tạo thiết bị'}
                </button>
              </div>
            </form>
          </div>
        </div>
      )}

      {/* Custom Command Modal */}
      {commandDevice && (
        <div className="fixed inset-0 bg-black/75 backdrop-blur-sm flex items-center justify-center z-50 p-4">
          <div className="bg-dark-900 border border-zinc-800 rounded-2xl p-6 w-full max-w-sm shadow-2xl text-zinc-100 animate-in fade-in zoom-in-95 duration-150">
            <div className="flex items-center justify-between pb-3 border-b border-zinc-800 mb-3">
              <h2 className="text-sm font-semibold text-zinc-100 flex items-center gap-2">
                <CommandLineIcon className="w-4 h-4 text-emerald-400" />
                Gửi lệnh điều khiển
              </h2>
              <button
                onClick={() => setCommandDevice(null)}
                className="text-zinc-400 hover:text-zinc-200"
              >
                <XMarkIcon className="w-4 h-4" />
              </button>
            </div>

            <p className="text-xs text-zinc-400 mb-3">
              Tới thiết bị: <span className="font-semibold text-zinc-200">{commandDevice.name}</span>{' '}
              <span className="font-mono text-zinc-500">({commandDevice.device_id})</span>
            </p>

            {/* Quick Command Chips */}
            <div className="mb-3">
              <span className="text-[11px] text-zinc-500 block mb-1.5">Lệnh nhanh gợi ý:</span>
              <div className="flex flex-wrap gap-1.5">
                {['ON', 'OFF', 'TOGGLE', 'STATUS', 'SET:50'].map((cmd) => (
                  <button
                    key={cmd}
                    type="button"
                    onClick={() => setCommandText(cmd)}
                    className="px-2 py-0.5 text-[11px] font-mono bg-zinc-800 hover:bg-zinc-700 text-zinc-300 border border-zinc-700 rounded transition-colors"
                  >
                    {cmd}
                  </button>
                ))}
              </div>
            </div>

            <input
              type="text"
              placeholder="Nhập lệnh (vd: ON, OFF, SET:100)..."
              value={commandText}
              onChange={(e) => setCommandText(e.target.value)}
              className="w-full px-3 py-2 text-xs bg-zinc-950 border border-zinc-800 rounded-lg text-zinc-100 font-mono placeholder-zinc-600 focus:outline-none focus:border-emerald-500 focus:ring-1 focus:ring-emerald-500 mb-4"
              spellCheck={false}
            />

            <div className="flex gap-2.5">
              <button
                type="button"
                onClick={() => setCommandDevice(null)}
                className="flex-1 px-4 py-2 bg-zinc-800 hover:bg-zinc-750 text-zinc-300 rounded-lg text-xs font-medium transition-colors border border-zinc-700"
              >
                Hủy
              </button>
              <button
                type="button"
                onClick={handleCommand}
                disabled={!commandText.trim()}
                className="flex-1 px-4 py-2 bg-emerald-500 hover:bg-emerald-400 text-dark-950 rounded-lg text-xs font-semibold transition-colors disabled:opacity-50 disabled:cursor-not-allowed shadow-sm focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-emerald-400"
              >
                Phát lệnh
              </button>
            </div>
          </div>
        </div>
      )}

      {/* Sensor Telemetry Modal */}
      {telemetryDevice && (
        <div className="fixed inset-0 bg-black/75 backdrop-blur-sm flex items-center justify-center z-50 p-4">
          <div className="bg-dark-900 border border-zinc-800 rounded-2xl p-6 w-full max-w-2xl shadow-2xl text-zinc-100 animate-in fade-in zoom-in-95 duration-150">
            <div className="flex items-center justify-between pb-3 border-b border-zinc-800 mb-4">
              <div>
                <h3 className="text-base font-bold text-zinc-100 flex items-center gap-2">
                  <ChartBarIcon className="w-5 h-5 text-sky-400" />
                  {telemetryDevice.name} — Lịch Sử Telemetry
                </h3>
                <p className="text-xs text-zinc-400 font-mono mt-0.5">
                  ID: {telemetryDevice.device_id} • Giao thức: {telemetryDevice.protocol.toUpperCase()}
                </p>
              </div>
              <button
                onClick={() => setTelemetryDevice(null)}
                className="text-zinc-400 hover:text-zinc-200 p-1 rounded-lg"
              >
                <XMarkIcon className="w-5 h-5" />
              </button>
            </div>

            <TelemetryChart
              data={telemetryData}
              title={`Xu hướng cảm biến ${telemetryDevice.name}`}
              unit={telemetryDevice.capabilities?.includes('humidity') ? '%' : '°C'}
              color="blue"
              loading={loadingTelemetry}
              height={230}
            />

            <div className="mt-4 flex justify-end">
              <button
                onClick={() => setTelemetryDevice(null)}
                className="px-4 py-2 bg-zinc-800 hover:bg-zinc-750 text-zinc-300 text-xs font-medium rounded-lg transition-colors border border-zinc-700"
              >
                Đóng cửa sổ
              </button>
            </div>
          </div>
        </div>
      )}

      {/* Delete Confirmation */}
      <ConfirmDialog
        isOpen={!!deleteDevice}
        message={`Bạn có chắc muốn xóa thiết bị "${deleteDevice?.name}" (${deleteDevice?.device_id})? Thao tác này không thể hoàn tác.`}
        confirmText="Xóa thiết bị"
        onConfirm={handleDelete}
        onCancel={() => setDeleteDevice(null)}
      />
    </div>
  )
}

export default Devices
