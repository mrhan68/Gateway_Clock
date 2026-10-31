import React, { useState, useEffect } from 'react'
import { Link } from 'react-router-dom'
import {
  ArrowPathIcon,
  BoltIcon,
  ClockIcon,
  CpuChipIcon,
  SignalIcon,
  BellAlertIcon,
  FireIcon,
  InformationCircleIcon,
} from '@heroicons/react/24/outline'
import StatusBadge from '../components/StatusBadge'
import TelemetryChart from '../components/TelemetryChart'
import {
  getActiveGateway,
  gatewaysAPI,
  devicesAPI,
  eventsAPI,
  schedulesAPI,
  telemetryAPI,
} from '../api/client'

// 4 Hardware Relay Channels on ESP32-S3 (GPIO 15..18)
const DEFAULT_RELAYS = [
  { id: 'relay_1', target: 'fan_01', name: 'Quạt Thông Gió', gpio: 'GPIO 15', icon: BoltIcon },
  { id: 'relay_2', target: 'heater_01', name: 'Sưởi Nhiệt', gpio: 'GPIO 16', icon: FireIcon },
  { id: 'relay_3', target: 'dehumidifier_01', name: 'Máy Hút Ẩm', gpio: 'GPIO 17', icon: ArrowPathIcon },
  { id: 'relay_4', target: 'aux_01', name: 'Còi Báo Động (Aux)', gpio: 'GPIO 18', icon: BellAlertIcon },
]

function Dashboard() {
  const gatewayId = getActiveGateway()
  const [gateway, setGateway] = useState(null)
  const [devices, setDevices] = useState([])
  const [events, setEvents] = useState([])
  const [schedules, setSchedules] = useState([])
  const [telemetryData, setTelemetryData] = useState([])
  const [loading, setLoading] = useState(true)
  const [refreshing, setRefreshing] = useState(false)
  const [lastRefreshed, setLastRefreshed] = useState(new Date())

  // Real-time ticking clock
  const [currentTime, setCurrentTime] = useState(new Date())

  // Relay states & pending toggles
  const [relayStates, setRelayStates] = useState({
    relay_1: false,
    relay_2: false,
    relay_3: false,
    relay_4: false,
  })
  const [relayPending, setRelayPending] = useState({})
  const [actionMessage, setActionMessage] = useState('')

  useEffect(() => {
    const timer = setInterval(() => setCurrentTime(new Date()), 1000)
    return () => clearInterval(timer)
  }, [])

  useEffect(() => {
    if (gatewayId) {
      loadData()
      const interval = setInterval(loadData, 20000)
      return () => clearInterval(interval)
    }
  }, [gatewayId])

  const loadData = async () => {
    if (!gatewayId) return
    setRefreshing(true)
    try {
      const [gwRes, devRes, eventRes, schedRes] = await Promise.all([
        gatewaysAPI.get(gatewayId).catch(() => ({ data: null })),
        devicesAPI.list(gatewayId).catch(() => ({ data: [] })),
        eventsAPI.list(gatewayId, { limit: 8 }).catch(() => ({ data: [] })),
        schedulesAPI.list(gatewayId).catch(() => ({ data: [] })),
      ])

      const devList = devRes.data || []
      setGateway(gwRes.data)
      setDevices(devList)
      setEvents(eventRes.data || [])
      setSchedules(schedRes.data || [])
      setLastRefreshed(new Date())

      // Deduce relay states from device list if available
      const newRelayStates = { ...relayStates }
      devList.forEach((d) => {
        if (d.device_id in newRelayStates) {
          newRelayStates[d.device_id] = d.status === 'online' || d.last_value === 1
        }
      })
      setRelayStates(newRelayStates)

      // Find sensor devices to fetch telemetry for real-time chart
      const sensorDevice = devList.find((d) => d.device_type === 'sensor')
      if (sensorDevice) {
        try {
          const telemRes = await telemetryAPI.list(gatewayId, sensorDevice.device_id, {
            limit: 30,
          })
          setTelemetryData(telemRes.data || [])
        } catch {
          setTelemetryData([])
        }
      }
    } catch (err) {
      console.error('Failed to load dashboard data:', err)
    } finally {
      setLoading(false)
      setRefreshing(false)
    }
  }

  // Handle tactile relay switch with optimistic UI
  const handleToggleRelay = async (relayId, target) => {
    const currentState = relayStates[relayId] || false
    const nextState = !currentState
    const command = nextState ? 'ON' : 'OFF'

    // Optimistic UI state
    setRelayStates((prev) => ({ ...prev, [relayId]: nextState }))
    setRelayPending((prev) => ({ ...prev, [relayId]: true }))
    setActionMessage(`Đang gửi lệnh ${command} tới ${target}…`)

    try {
      await devicesAPI.sendCommand(gatewayId, target, command)
      setActionMessage(`Lệnh ${command} gửi thành công tới ${target}`)
      setTimeout(() => setActionMessage(''), 3500)
    } catch (err) {
      // Revert state on failure
      setRelayStates((prev) => ({ ...prev, [relayId]: currentState }))
      setActionMessage(`Lỗi gửi lệnh: ${err.message || 'Không thể điều khiển'}`)
      setTimeout(() => setActionMessage(''), 4000)
    } finally {
      setRelayPending((prev) => ({ ...prev, [relayId]: false }))
    }
  }

  const onlineCount = devices.filter((d) => d.status === 'online').length
  const offlineCount = devices.filter((d) => d.status === 'offline').length
  const activeSchedulesCount = schedules.filter((s) => s.enabled).length

  const tempTelemetry = telemetryData.filter(
    (d) => d.unit === 'C' || d.unit === '°C' || (!d.unit && d.value < 60)
  )
  const humTelemetry = telemetryData.filter(
    (d) => d.unit === '%' || (!d.unit && d.value >= 60)
  )

  const latestTemp = tempTelemetry.length > 0 ? tempTelemetry[tempTelemetry.length - 1].value : null
  const latestHum = humTelemetry.length > 0 ? humTelemetry[humTelemetry.length - 1].value : null
  const upcomingSchedules = schedules.slice(0, 4)

  if (!gatewayId) {
    return (
      <div className="max-w-md mx-auto text-center py-20">
        <div className="w-12 h-12 rounded-xl bg-zinc-900 border border-zinc-800 text-zinc-400 flex items-center justify-center mx-auto mb-4">
          <InformationCircleIcon className="w-6 h-6" />
        </div>
        <h2 className="text-base font-semibold text-zinc-100">Chưa chọn Gateway</h2>
        <p className="text-xs text-zinc-400 mt-1.5">
          Vui lòng chọn hoặc ghép nối Gateway qua menu bên trái để bắt đầu giám sát.
        </p>
      </div>
    )
  }

  // Shimmer skeleton when loading initially
  if (loading && !gateway) {
    return (
      <div className="space-y-6 animate-pulse">
        <div className="h-28 bg-dark-900 border border-zinc-800 rounded-xl" />
        <div className="grid grid-cols-2 sm:grid-cols-4 gap-4">
          {[...Array(4)].map((_, i) => (
            <div key={i} className="h-24 bg-dark-900 border border-zinc-800 rounded-xl" />
          ))}
        </div>
        <div className="h-64 bg-dark-900 border border-zinc-800 rounded-xl" />
      </div>
    )
  }

  return (
    <div className="space-y-6">
      {/* Top Banner / RTC Clock Widget */}
      <div className="bg-dark-900 border border-zinc-800 rounded-xl p-5 shadow-sm">
        <div className="flex flex-col lg:flex-row lg:items-center lg:justify-between gap-4">
          <div>
            <div className="flex items-center gap-3">
              <h1 className="text-lg font-semibold tracking-tight text-zinc-100">
                {gateway?.name || gatewayId}
              </h1>
              <StatusBadge status={gateway?.status || 'online'} />
            </div>
            <div className="mt-2 flex flex-wrap items-center gap-x-4 gap-y-1 text-xs text-zinc-400 font-mono">
              <span>ID: {gatewayId}</span>
              <span className="text-zinc-600">•</span>
              <span>Firmware: v{gateway?.firmware_version || '1.6.0'}</span>
              <span className="text-zinc-600">•</span>
              <span>Timezone: {gateway?.timezone || 'ICT-7'}</span>
              <span className="text-zinc-600">•</span>
              <span className="text-zinc-500">Đồng bộ: {lastRefreshed.toLocaleTimeString('vi-VN')}</span>
            </div>
          </div>

          {/* OLED Digital Clock Panel */}
          <div className="flex items-center gap-4 bg-zinc-950/80 border border-zinc-800 px-4 py-2.5 rounded-xl">
            <div className="text-right">
              <div className="text-2xl font-bold font-mono text-emerald-400 tabular-nums tracking-wider shadow-[0_0_12px_rgba(52,211,153,0.3)]">
                {currentTime.toLocaleTimeString('vi-VN', { hour12: false })}
              </div>
              <div className="text-[11px] text-zinc-500 font-mono uppercase tracking-wider">
                {currentTime.toLocaleDateString('vi-VN', {
                  weekday: 'short',
                  year: 'numeric',
                  month: '2-digit',
                  day: '2-digit',
                })}
              </div>
            </div>
            <button
              onClick={loadData}
              disabled={refreshing}
              className="p-2 rounded-lg bg-zinc-900 border border-zinc-800 text-zinc-400 hover:text-zinc-200 hover:bg-zinc-800 transition-colors focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-emerald-500"
              aria-label="Refresh data"
              title="Làm mới tức thì"
            >
              <ArrowPathIcon className={`w-4 h-4 ${refreshing ? 'animate-spin text-emerald-400' : ''}`} />
            </button>
          </div>
        </div>

        {/* Action toast notification */}
        {actionMessage && (
          <div className="mt-3 p-2 text-xs bg-emerald-500/10 text-emerald-400 border border-emerald-500/20 rounded-lg flex items-center gap-2 animate-in fade-in">
            <span className="w-1.5 h-1.5 rounded-full bg-emerald-400 animate-ping" />
            <span>{actionMessage}</span>
          </div>
        )}
      </div>

      {/* 4-Channel Relay Actuator Controls */}
      <div>
        <div className="flex items-center justify-between mb-3 px-0.5">
          <div className="flex items-center gap-2">
            <BoltIcon className="w-4 h-4 text-emerald-400" />
            <h2 className="text-xs font-semibold text-zinc-400 uppercase tracking-wider">
              Bộ Điều Khiển Rơ-Le Phần Cứng (ESP32 GPIO 15..18)
            </h2>
          </div>
          <span className="text-[11px] text-zinc-500 font-mono">Active HIGH / 10A 250VAC</span>
        </div>

        <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-4 gap-3.5">
          {DEFAULT_RELAYS.map((relay) => {
            const Icon = relay.icon
            const isOn = relayStates[relay.id] || false
            const isPending = relayPending[relay.id] || false

            return (
              <div
                key={relay.id}
                className={`p-4 rounded-xl border transition-all duration-200 relative overflow-hidden ${
                  isOn
                    ? 'bg-zinc-900/90 border-emerald-500/40 shadow-[0_0_20px_-5px_rgba(16,185,129,0.2)]'
                    : 'bg-dark-900 border-zinc-800 hover:border-zinc-700'
                }`}
              >
                <div className="flex items-start justify-between">
                  <div className="flex items-center gap-2.5">
                    <div
                      className={`w-8 h-8 rounded-lg flex items-center justify-center border transition-colors ${
                        isOn
                          ? 'bg-emerald-500/20 border-emerald-500/40 text-emerald-400 shadow-[0_0_10px_rgba(52,211,153,0.5)]'
                          : 'bg-zinc-950 border-zinc-800 text-zinc-500'
                      }`}
                    >
                      <Icon className="w-4 h-4" />
                    </div>
                    <div>
                      <h3 className="text-xs font-semibold text-zinc-100">{relay.name}</h3>
                      <p className="text-[10px] text-zinc-500 font-mono">{relay.gpio}</p>
                    </div>
                  </div>

                  {/* Pulsing Status LED */}
                  <div className="flex items-center gap-1.5">
                    <span
                      className={`w-2 h-2 rounded-full transition-all ${
                        isOn
                          ? 'bg-emerald-400 shadow-[0_0_8px_rgba(52,211,153,0.9)] animate-pulse'
                          : 'bg-zinc-700'
                      }`}
                    />
                    <span className="text-[11px] font-mono text-zinc-400">
                      {isOn ? 'ON' : 'OFF'}
                    </span>
                  </div>
                </div>

                {/* Tactile Toggle Button */}
                <div className="mt-4 pt-3 border-t border-zinc-800/80 flex items-center justify-between">
                  <span className="text-[11px] text-zinc-500 font-mono">
                    ID: {relay.target}
                  </span>
                  <button
                    onClick={() => handleToggleRelay(relay.id, relay.target)}
                    disabled={isPending}
                    className={`px-3 py-1 rounded-md text-xs font-semibold transition-all focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-emerald-500 ${
                      isOn
                        ? 'bg-rose-500/20 text-rose-300 hover:bg-rose-500/30 border border-rose-500/40'
                        : 'bg-emerald-500/20 text-emerald-300 hover:bg-emerald-500/30 border border-emerald-500/40'
                    } disabled:opacity-50`}
                  >
                    {isPending ? 'Đang gửi…' : isOn ? 'Tắt Ngay' : 'Bật Ngay'}
                  </button>
                </div>
              </div>
            )
          })}
        </div>
      </div>

      {/* KPI Stats Overview */}
      <div className="grid grid-cols-2 lg:grid-cols-4 gap-3.5">
        <div className="bg-dark-900 border border-zinc-800 rounded-xl p-4">
          <div className="flex items-center justify-between">
            <span className="text-xs text-zinc-400">Thiết Bị Online</span>
            <CpuChipIcon className="w-4 h-4 text-emerald-400" />
          </div>
          <div className="text-2xl font-bold font-mono text-zinc-100 mt-2 tabular-nums">
            {onlineCount}
            <span className="text-xs text-zinc-500 font-normal ml-1">/ {devices.length}</span>
          </div>
          <div className="text-[10px] text-zinc-500 font-mono mt-1">
            {offlineCount > 0 ? `${offlineCount} ngoại tuyến` : 'Toàn bộ trực tuyến'}
          </div>
        </div>

        <div className="bg-dark-900 border border-zinc-800 rounded-xl p-4">
          <div className="flex items-center justify-between">
            <span className="text-xs text-zinc-400">Nhiệt Độ Live</span>
            <SignalIcon className="w-4 h-4 text-sky-400" />
          </div>
          <div className="text-2xl font-bold font-mono text-sky-400 mt-2 tabular-nums">
            {latestTemp !== null ? `${latestTemp} °C` : '—'}
          </div>
          <div className="text-[10px] text-zinc-500 font-mono mt-1">Sensor BLE 0x181A</div>
        </div>

        <div className="bg-dark-900 border border-zinc-800 rounded-xl p-4">
          <div className="flex items-center justify-between">
            <span className="text-xs text-zinc-400">Độ Ẩm Không Khí</span>
            <ArrowPathIcon className="w-4 h-4 text-emerald-400" />
          </div>
          <div className="text-2xl font-bold font-mono text-emerald-400 mt-2 tabular-nums">
            {latestHum !== null ? `${latestHum} %` : '—'}
          </div>
          <div className="text-[10px] text-zinc-500 font-mono mt-1">BTHome V2 Protocol</div>
        </div>

        <div className="bg-dark-900 border border-zinc-800 rounded-xl p-4">
          <div className="flex items-center justify-between">
            <span className="text-xs text-zinc-400">Lịch Biểu Kích Hoạt</span>
            <ClockIcon className="w-4 h-4 text-amber-400" />
          </div>
          <div className="text-2xl font-bold font-mono text-amber-400 mt-2 tabular-nums">
            {activeSchedulesCount}
            <span className="text-xs text-zinc-500 font-normal ml-1">/ {schedules.length}</span>
          </div>
          <div className="text-[10px] text-zinc-500 font-mono mt-1">Local RTC Guard</div>
        </div>
      </div>

      {/* Real-time Environmental Telemetry Charts */}
      <div className="space-y-3">
        <div className="flex items-center justify-between px-0.5">
          <h2 className="text-xs font-semibold text-zinc-400 uppercase tracking-wider">
            Biểu Đồ Đo Đạc Môi Trường Theo Thời Gian
          </h2>
          <Link
            to="/devices"
            className="text-xs text-emerald-400 hover:text-emerald-300 font-medium transition-colors"
          >
            Quản lý cảm biến →
          </Link>
        </div>

        <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
          <TelemetryChart
            data={tempTelemetry.length > 0 ? tempTelemetry : telemetryData}
            title="Biểu Đồ Nhiệt Độ (°C)"
            unit="°C"
            color="blue"
            loading={loading && telemetryData.length === 0}
            height={190}
          />
          <TelemetryChart
            data={humTelemetry.length > 0 ? humTelemetry : []}
            title="Biểu Đồ Độ Ẩm (%RH)"
            unit="%"
            color="emerald"
            loading={loading && telemetryData.length === 0}
            height={190}
          />
        </div>
      </div>

      {/* Bottom Row: Upcoming Alarms & Recent Audit Trail */}
      <div className="grid grid-cols-1 lg:grid-cols-2 gap-4">
        {/* Upcoming Schedules (FSD §6.2.1) */}
        <div className="bg-dark-900 border border-zinc-800 rounded-xl p-5 shadow-sm">
          <div className="flex items-center justify-between pb-3 border-b border-zinc-800 mb-3">
            <div>
              <h3 className="text-sm font-semibold text-zinc-100">Lịch Biểu & Báo Thức Sắp Đến</h3>
              <p className="text-[11px] text-zinc-500">Đồng bộ tự động từ bộ nhớ Flash LittleFS</p>
            </div>
            <Link
              to="/schedules"
              className="text-xs text-emerald-400 hover:text-emerald-300 font-medium"
            >
              Tất cả ({schedules.length}) →
            </Link>
          </div>

          {upcomingSchedules.length === 0 ? (
            <p className="text-xs text-zinc-500 py-6 text-center italic font-mono">
              Chưa có lịch biểu nào được kích hoạt.
            </p>
          ) : (
            <div className="grid grid-cols-1 sm:grid-cols-2 gap-2.5">
              {upcomingSchedules.map((s) => (
                <div
                  key={s.id}
                  className="p-3 rounded-lg bg-zinc-950/70 border border-zinc-800/80 flex items-center justify-between"
                >
                  <div>
                    <span className="text-[10px] font-semibold uppercase tracking-wider text-emerald-400 font-mono">
                      {s.type}
                    </span>
                    <div className="text-lg font-bold font-mono text-zinc-100 tabular-nums">
                      {String(s.hour).padStart(2, '0')}:{String(s.minute).padStart(2, '0')}
                    </div>
                    <div className="text-xs text-zinc-400 truncate max-w-[140px]">
                      {s.title || s.action_command || 'Báo thức'}
                    </div>
                  </div>
                  <span
                    className={`text-[10px] px-2 py-0.5 rounded-full font-medium border ${
                      s.enabled
                        ? 'bg-emerald-500/10 text-emerald-400 border-emerald-500/20'
                        : 'bg-zinc-800 text-zinc-500 border-zinc-700'
                    }`}
                  >
                    {s.enabled ? 'Active' : 'Off'}
                  </span>
                </div>
              ))}
            </div>
          )}
        </div>

        {/* Recent Events Audit Trail */}
        <div className="bg-dark-900 border border-zinc-800 rounded-xl p-5 shadow-sm">
          <div className="flex items-center justify-between pb-3 border-b border-zinc-800 mb-3">
            <div>
              <h3 className="text-sm font-semibold text-zinc-100">Nhật Ký Sự Kiện Gần Nhất</h3>
              <p className="text-[11px] text-zinc-500">Bản tin ACK & Cảnh báo an toàn</p>
            </div>
            <Link
              to="/events"
              className="text-xs text-emerald-400 hover:text-emerald-300 font-medium"
            >
              Chi tiết →
            </Link>
          </div>

          {events.length === 0 ? (
            <p className="text-xs text-zinc-500 py-6 text-center italic font-mono">
              Chưa có bản ghi sự kiện mới.
            </p>
          ) : (
            <ul className="space-y-2 max-h-56 overflow-y-auto pr-1">
              {events.map((event) => (
                <li
                  key={event.id}
                  className="p-2 rounded-lg bg-zinc-950/60 border border-zinc-800/60 flex items-center justify-between text-xs"
                >
                  <div className="flex items-center gap-2 truncate">
                    <span className="w-1.5 h-1.5 rounded-full bg-emerald-400 flex-shrink-0" />
                    <span className="font-medium text-zinc-200 truncate">
                      {event.event_type}
                    </span>
                    {event.device_id && (
                      <span className="text-[11px] font-mono text-zinc-500 truncate">
                        [{event.device_id}]
                      </span>
                    )}
                  </div>
                  <span className="text-[10px] text-zinc-500 font-mono tabular-nums flex-shrink-0 ml-2">
                    {new Date(event.occurred_at).toLocaleTimeString([], {
                      hour: '2-digit',
                      minute: '2-digit',
                      second: '2-digit',
                    })}
                  </span>
                </li>
              ))}
            </ul>
          )}
        </div>
      </div>
    </div>
  )
}

export default Dashboard
