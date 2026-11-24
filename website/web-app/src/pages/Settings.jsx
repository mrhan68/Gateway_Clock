import React, { useState, useEffect } from 'react'
import {
  Cog6ToothIcon,
  CpuChipIcon,
  CloudArrowUpIcon,
  ShieldCheckIcon,
  LockClosedIcon,
  ArrowPathIcon,
  CheckCircleIcon,
  InformationCircleIcon,
} from '@heroicons/react/24/outline'
import StatusBadge from '../components/StatusBadge'
import Toast from '../components/Toast'
import { getActiveGateway, gatewaysAPI } from '../api/client'

const COMMON_TIMEZONES = [
  'Asia/Ho_Chi_Minh',
  'Asia/Bangkok',
  'Asia/Singapore',
  'Asia/Tokyo',
  'UTC',
  'Europe/London',
  'America/New_York',
]

function Settings() {
  const gatewayId = getActiveGateway()
  const [gateway, setGateway] = useState(null)
  const [loading, setLoading] = useState(true)
  const [saving, setSaving] = useState(false)
  const [notification, setNotification] = useState(null)
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
        timezone: res.data.timezone || 'Asia/Ho_Chi_Minh',
        firmware_version: res.data.firmware_version || res.data.firmwareVersion || '1.6.0',
      })
    } catch (err) {
      console.error('Failed to load gateway:', err)
      setNotification({ type: 'error', message: err.message || 'Không thể tải cấu hình Gateway' })
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
      const res = await gatewaysAPI.update(gatewayId, {
        name: form.name.trim(),
        timezone: form.timezone.trim(),
      })
      setGateway(res.data)
      setNotification({ type: 'success', message: 'Đã lưu cấu hình Gateway thành công!' })
    } catch (err) {
      setNotification({ type: 'error', message: err.message || 'Không thể lưu cài đặt Gateway' })
    } finally {
      setSaving(false)
    }
  }

  const [backendUrl, setBackendUrl] = useState(localStorage.getItem('apiBaseUrl') || 'https://smartclock-backend.onrender.com')

  const handleSaveBackendUrl = (e) => {
    e.preventDefault()
    localStorage.setItem('apiBaseUrl', backendUrl.trim().replace(/\/+$/, ''))
    setNotification({ type: 'success', message: 'Đã lưu Backend API URL! Đang kết nối lại...' })
    setTimeout(() => {
      window.location.reload()
    }, 700)
  }

  const renderBackendCard = () => (
    <div className="bg-dark-900 border border-zinc-800 rounded-xl p-5 shadow-sm space-y-4">
      <div className="flex items-center gap-3">
        <div className="w-9 h-9 rounded-lg bg-blue-500/10 border border-blue-500/30 flex items-center justify-center text-blue-400">
          <CloudArrowUpIcon className="w-5 h-5" />
        </div>
        <div>
          <h2 className="text-sm font-semibold text-zinc-100">Backend API URL (Render / Cloud)</h2>
          <p className="text-xs text-zinc-400">
            Địa chỉ REST API backend của bạn trên Render.com hoặc VPS để Web App kết nối.
          </p>
        </div>
      </div>
      <form onSubmit={handleSaveBackendUrl} className="flex flex-col sm:flex-row gap-3 pt-2">
        <input
          type="url"
          value={backendUrl}
          onChange={(e) => setBackendUrl(e.target.value)}
          placeholder="https://smartclock-backend.onrender.com"
          className="flex-1 bg-zinc-950 border border-zinc-800 rounded-lg px-3 py-2 text-xs text-zinc-200 placeholder-zinc-500 focus:outline-none focus:border-emerald-500/60 font-mono"
        />
        <button
          type="submit"
          className="px-4 py-2 bg-emerald-600 hover:bg-emerald-500 text-white rounded-lg text-xs font-medium transition-colors whitespace-nowrap"
        >
          Lưu & Kết Nối
        </button>
      </form>
    </div>
  )

  if (!gatewayId) {
    return (
      <div className="space-y-6 max-w-4xl">
        {notification && (
          <Toast
            message={notification.message}
            type={notification.type}
            onClose={() => setNotification(null)}
          />
        )}
        {renderBackendCard()}
        <div className="max-w-md mx-auto text-center py-12">
          <div className="w-12 h-12 rounded-xl bg-zinc-900 border border-zinc-800 text-zinc-400 flex items-center justify-center mx-auto mb-4">
            <Cog6ToothIcon className="w-6 h-6" />
          </div>
          <h2 className="text-base font-semibold text-zinc-100">Chưa chọn Gateway</h2>
          <p className="text-xs text-zinc-400 mt-1.5">
            Vui lòng nhập Backend URL ở trên hoặc chọn Gateway trên thanh điều hướng để cấu hình.
          </p>
        </div>
      </div>
    )
  }

  return (
    <div className="space-y-6 max-w-4xl">
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
            <h1 className="text-xl font-bold tracking-tight text-zinc-100">Cài Đặt Hệ Thống</h1>
            <span className="text-xs font-mono px-2 py-0.5 rounded-full bg-zinc-800 text-zinc-300 border border-zinc-700">
              Gateway ID: {gatewayId}
            </span>
          </div>
          <p className="text-xs text-zinc-400 mt-1">
            Quản lý tên hiển thị, múi giờ RTC, thông số phần cứng ESP32-S3 và chuẩn bị cập nhật OTA.
          </p>
        </div>

        <button
          onClick={loadGateway}
          disabled={loading}
          className="self-start sm:self-auto p-2 rounded-lg bg-dark-900 border border-zinc-800 text-zinc-400 hover:text-zinc-200 hover:bg-zinc-800/80 transition-colors focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-emerald-500"
          title="Tải lại cài đặt"
        >
          <ArrowPathIcon className={`w-4 h-4 ${loading ? 'animate-spin text-emerald-400' : ''}`} />
        </button>
      </div>

      {renderBackendCard()}

      {loading && !gateway ? (
        <div className="space-y-4">
          <div className="h-40 bg-dark-900 border border-zinc-800 rounded-xl animate-pulse" />
          <div className="h-64 bg-dark-900 border border-zinc-800 rounded-xl animate-pulse" />
        </div>
      ) : (
        <div className="space-y-6">
          {/* Gateway Status Overview Card */}
          <div className="bg-dark-900 border border-zinc-800 rounded-xl p-5 shadow-sm">
            <div className="flex flex-col sm:flex-row sm:items-start justify-between gap-4">
              <div className="space-y-3">
                <div className="flex items-center gap-3">
                  <div className="p-2 rounded-lg bg-zinc-950 border border-zinc-800 text-emerald-400">
                    <CpuChipIcon className="w-5 h-5" />
                  </div>
                  <div>
                    <h2 className="text-base font-semibold text-zinc-100">
                      {gateway?.name || gatewayId}
                    </h2>
                    <p className="text-xs font-mono text-zinc-400">
                      Định danh phần cứng: <span className="text-zinc-200">{gateway?.id}</span>
                    </p>
                  </div>
                </div>

                <div className="grid grid-cols-1 sm:grid-cols-3 gap-3 pt-2">
                  <div className="p-3 rounded-lg bg-zinc-950/70 border border-zinc-800/80">
                    <span className="text-[11px] text-zinc-500 block">Phiên bản Firmware</span>
                    <span className="text-xs font-mono font-semibold text-emerald-400">
                      v{gateway?.firmware_version || gateway?.firmwareVersion || '1.6.0'}
                    </span>
                  </div>

                  <div className="p-3 rounded-lg bg-zinc-950/70 border border-zinc-800/80">
                    <span className="text-[11px] text-zinc-500 block">Múi giờ đồng hồ RTC</span>
                    <span className="text-xs font-mono font-semibold text-zinc-200">
                      {gateway?.timezone || 'ICT-7'}
                    </span>
                  </div>

                  <div className="p-3 rounded-lg bg-zinc-950/70 border border-zinc-800/80">
                    <span className="text-[11px] text-zinc-500 block">Lần tương tác cuối</span>
                    <span className="text-xs font-mono font-medium text-zinc-300 tabular-nums">
                      {(gateway?.last_seen || gateway?.lastSeen)
                        ? new Date(gateway.last_seen || gateway.lastSeen).toLocaleString('vi-VN')
                        : 'Không rõ'}
                    </span>
                  </div>
                </div>
              </div>

              <div className="self-start">
                <StatusBadge status={gateway?.status || 'online'} />
              </div>
            </div>
          </div>

          {/* Edit Gateway Parameters Card */}
          <div className="bg-dark-900 border border-zinc-800 rounded-xl p-6 shadow-sm">
            <h3 className="text-sm font-semibold text-zinc-100 flex items-center gap-2 mb-4 pb-3 border-b border-zinc-800/80">
              <Cog6ToothIcon className="w-4 h-4 text-emerald-400" />
              Cấu hình Thông số Gateway
            </h3>

            <form onSubmit={handleSave} className="space-y-4">
              <div>
                <label className="block text-xs font-medium text-zinc-300 mb-1.5">
                  Tên hiển thị Gateway *
                </label>
                <input
                  name="name"
                  type="text"
                  value={form.name}
                  onChange={handleChange}
                  placeholder="ví dụ: ESP32 Gateway Phòng Ngủ"
                  className="w-full px-3.5 py-2 text-xs bg-zinc-950 border border-zinc-800 rounded-lg text-zinc-100 placeholder-zinc-600 focus:outline-none focus:border-emerald-500 focus:ring-1 focus:ring-emerald-500 transition-colors"
                  required
                />
              </div>

              <div>
                <div className="flex items-center justify-between mb-1.5">
                  <label className="text-xs font-medium text-zinc-300">
                    Múi giờ hệ thống (Timezone)
                  </label>
                  <div className="flex flex-wrap gap-2 text-[11px]">
                    {COMMON_TIMEZONES.slice(0, 5).map((tz) => (
                      <button
                        key={tz}
                        type="button"
                        onClick={() => setForm({ ...form, timezone: tz })}
                        className="text-emerald-400 hover:underline font-mono"
                      >
                        {tz.split('/')[1] || tz}
                      </button>
                    ))}
                  </div>
                </div>
                <input
                  name="timezone"
                  type="text"
                  value={form.timezone}
                  onChange={handleChange}
                  placeholder="Asia/Ho_Chi_Minh"
                  className="w-full px-3.5 py-2 text-xs bg-zinc-950 border border-zinc-800 rounded-lg text-zinc-100 font-mono placeholder-zinc-600 focus:outline-none focus:border-emerald-500 focus:ring-1 focus:ring-emerald-500 transition-colors"
                />
                <p className="text-[11px] text-zinc-500 mt-1">
                  Múi giờ được dùng để đồng bộ thời gian SNTP lên chip RTC nội bộ và hiển thị trên màn hình OLED.
                </p>
              </div>

              <div>
                <label className="block text-xs font-medium text-zinc-400 mb-1.5 flex items-center gap-1.5">
                  <LockClosedIcon className="w-3.5 h-3.5 text-zinc-500" />
                  <span>Phiên bản Firmware đang nạp (Chỉ đọc)</span>
                </label>
                <input
                  name="firmware_version"
                  type="text"
                  value={form.firmware_version}
                  disabled
                  className="w-full px-3.5 py-2 text-xs bg-zinc-950/60 border border-zinc-850 rounded-lg text-zinc-400 font-mono cursor-not-allowed"
                />
                <p className="text-[11px] text-zinc-500 mt-1">
                  Được đồng bộ tự động từ firmware ESP32-S3 khi kết nối MQTT Handshake; không thể chỉnh sửa thủ công.
                </p>
              </div>

              <div className="pt-2">
                <button
                  type="submit"
                  disabled={saving}
                  className="inline-flex items-center gap-2 px-5 py-2.5 bg-emerald-500 hover:bg-emerald-400 text-dark-950 rounded-lg text-xs font-semibold transition-colors shadow-sm disabled:opacity-50 focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-emerald-400"
                >
                  {saving ? (
                    <>
                      <ArrowPathIcon className="w-4 h-4 animate-spin" />
                      <span>Đang lưu...</span>
                    </>
                  ) : (
                    <>
                      <CheckCircleIcon className="w-4 h-4" />
                      <span>Lưu thay đổi</span>
                    </>
                  )}
                </button>
              </div>
            </form>
          </div>

          {/* Hardware & Memory Architecture Card */}
          <div className="bg-dark-900 border border-zinc-800 rounded-xl p-6 shadow-sm">
            <h3 className="text-sm font-semibold text-zinc-100 flex items-center gap-2 mb-4 pb-3 border-b border-zinc-800/80">
              <ShieldCheckIcon className="w-4 h-4 text-sky-400" />
              Kiến Trúc Phần Cứng & Phân Vùng Bộ Nhớ (ESP32-S3)
            </h3>

            <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-4 gap-3 text-xs">
              <div className="p-3 bg-zinc-950 border border-zinc-800 rounded-lg">
                <span className="text-[11px] text-zinc-500 block">Vi điều khiển</span>
                <span className="font-semibold text-zinc-200">ESP32-S3 Xtensa LX7</span>
                <span className="text-[10px] text-zinc-500 block mt-0.5">Dual-core @ 240MHz</span>
              </div>
              <div className="p-3 bg-zinc-950 border border-zinc-800 rounded-lg">
                <span className="text-[11px] text-zinc-500 block">Phân vùng Flash</span>
                <span className="font-semibold text-zinc-200">Dual OTA (2MB x 2)</span>
                <span className="text-[10px] text-emerald-400 block mt-0.5">Hỗ trợ Rollback an toàn</span>
              </div>
              <div className="p-3 bg-zinc-950 border border-zinc-800 rounded-lg">
                <span className="text-[11px] text-zinc-500 block">Giao tiếp Ngoại vi</span>
                <span className="font-semibold text-zinc-200">Wi-Fi 802.11 b/g/n & BLE 5.0</span>
                <span className="text-[10px] text-zinc-500 block mt-0.5">MQTT QoS 1 + BLE GATT</span>
              </div>
              <div className="p-3 bg-zinc-950 border border-zinc-800 rounded-lg">
                <span className="text-[11px] text-zinc-500 block">Lưu trữ Cục bộ</span>
                <span className="font-semibold text-zinc-200">NVS + SPIFFS Storage</span>
                <span className="text-[10px] text-zinc-500 block mt-0.5">Lưu lịch trình khi mất điện</span>
              </div>
            </div>
          </div>

          {/* OTA Firmware Update Card */}
          <div className="bg-dark-900 border border-dashed border-zinc-800 rounded-xl p-6 shadow-sm">
            <div className="flex items-start gap-4">
              <div className="p-2.5 rounded-xl bg-zinc-950 border border-zinc-800 text-sky-400">
                <CloudArrowUpIcon className="w-6 h-6" />
              </div>
              <div className="space-y-2">
                <div className="flex items-center gap-2">
                  <h3 className="text-sm font-semibold text-zinc-100">
                    Cập Nhật Firmware Từ Xa (OTA Updates)
                  </h3>
                  <span className="text-[10px] px-2 py-0.5 rounded-full bg-sky-500/10 text-sky-400 border border-sky-500/20 font-medium">
                    Kênh Sẵn Sàng
                  </span>
                </div>
                <p className="text-xs text-zinc-400 leading-relaxed">
                  Thiết bị ESP32-S3 được trang bị bảng phân vùng <code className="text-zinc-300 font-mono">partitions.csv</code> với 2 khe ứng dụng độc lập (<code className="text-zinc-300 font-mono">app0</code> và <code className="text-zinc-300 font-mono">app1</code>). Khi nạp firmware mới qua kênh OTA MQTT, nếu khởi động thất bại hoặc lỗi watchdog, gateway sẽ tự động khôi phục (rollback) về bản firmware ổn định trước đó.
                </p>
                <div className="pt-1 flex items-center gap-2 text-xs text-zinc-500">
                  <InformationCircleIcon className="w-4 h-4 text-zinc-500" />
                  <span>Kênh OTA sẽ kích hoạt khi có tệp nhị phân <code className="text-zinc-400 font-mono">firmware.bin</code> ký điện tử.</span>
                </div>
              </div>
            </div>
          </div>
        </div>
      )}
    </div>
  )
}

export default Settings