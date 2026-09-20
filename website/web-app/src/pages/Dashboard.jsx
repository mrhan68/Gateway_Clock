import React, { useState, useEffect } from 'react'
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

function Dashboard() {
  const gatewayId = getActiveGateway()
  const [gateway, setGateway] = useState(null)
  const [devices, setDevices] = useState([])
  const [events, setEvents] = useState([])
  const [schedules, setSchedules] = useState([])
  const [telemetryData, setTelemetryData] = useState([])
  const [loading, setLoading] = useState(true)
  const [lastRefreshed, setLastRefreshed] = useState(new Date())

  useEffect(() => {
    if (gatewayId) {
      loadData()
      // Refresh every 30s
      const interval = setInterval(loadData, 30000)
      return () => clearInterval(interval)
    }
  }, [gatewayId])

  const loadData = async () => {
    if (!gatewayId) return
    setLoading(true)
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

      // Find sensor devices to fetch telemetry for real-time chart
      const sensorDevice = devList.find((d) => d.device_type === 'sensor')
      if (sensorDevice) {
        try {
          const telemRes = await telemetryAPI.list(gatewayId, sensorDevice.device_id, {
            limit: 25,
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
    }
  }

  const onlineCount = devices.filter((d) => d.status === 'online').length
  const offlineCount = devices.filter((d) => d.status === 'offline').length
  const activeSchedulesCount = schedules.filter((s) => s.enabled).length

  // Filter temperature & humidity if multiple readings exist
  const tempTelemetry = telemetryData.filter(
    (d) => d.unit === 'C' || d.unit === '°C' || (!d.unit && d.value < 60)
  )
  const humTelemetry = telemetryData.filter(
    (d) => d.unit === '%' || (!d.unit && d.value >= 60)
  )

  // Nearest upcoming schedules (up to 4)
  const upcomingSchedules = schedules.slice(0, 4)

  if (!gatewayId) {
    return (
      <div className="text-center py-12">
        <h2 className="text-xl font-semibold text-gray-700">No gateway selected</h2>
        <p className="text-gray-500 mt-2">Use the sidebar to select or claim a gateway.</p>
      </div>
    )
  }

  if (loading && !gateway) {
    return <div className="text-center py-12 text-gray-500">Loading dashboard…</div>
  }

  return (
    <div className="space-y-6">
      {/* Header */}
      <div className="flex flex-col sm:flex-row sm:items-center sm:justify-between gap-2">
        <div>
          <h1 className="text-2xl font-bold text-gray-800">
            {gateway?.name || gatewayId}
          </h1>
          <p className="text-sm text-gray-500 mt-0.5">
            Gateway ID: <span className="font-mono">{gatewayId}</span>
          </p>
        </div>
        <div className="flex items-center gap-3">
          <span className="text-xs text-gray-400">
            Refreshed: {lastRefreshed.toLocaleTimeString()}
          </span>
          <button
            onClick={loadData}
            className="px-3 py-1.5 text-xs bg-white border border-gray-200 hover:bg-gray-50 text-gray-700 rounded-md font-medium shadow-sm transition-colors"
          >
            ↻ Refresh
          </button>
        </div>
      </div>

      {/* Gateway status card */}
      <div className="bg-white rounded-lg shadow-sm border border-gray-100 p-6">
        <div className="flex items-center justify-between">
          <div>
            <div className="flex items-center gap-2">
              <h3 className="text-lg font-semibold text-gray-800">Smart Clock Gateway</h3>
              <StatusBadge status={gateway?.status || 'unknown'} />
            </div>
            <div className="mt-2 grid grid-cols-1 sm:grid-cols-3 gap-y-1 gap-x-6 text-sm text-gray-500">
              <p>
                Firmware: <span className="font-medium text-gray-700">{gateway?.firmware_version || '1.0.0'}</span>
              </p>
              <p>
                Timezone: <span className="font-medium text-gray-700">{gateway?.timezone || 'ICT-7'}</span>
              </p>
              <p>
                Last seen: <span className="font-medium text-gray-700">
                  {gateway?.last_seen ? new Date(gateway.last_seen).toLocaleString() : 'Just now'}
                </span>
              </p>
            </div>
          </div>
        </div>
      </div>

      {/* Device summary cards */}
      <div className="grid grid-cols-1 sm:grid-cols-3 gap-4">
        <div className="bg-white rounded-lg shadow-sm border border-gray-100 p-5 text-center">
          <div className="text-3xl font-bold text-green-600">{onlineCount}</div>
          <div className="text-sm text-gray-600 mt-1">Devices Online</div>
          <div className="text-xs text-gray-400 mt-1">
            of {devices.length} registered
          </div>
        </div>
        <div className="bg-white rounded-lg shadow-sm border border-gray-100 p-5 text-center">
          <div className="text-3xl font-bold text-red-600">{offlineCount}</div>
          <div className="text-sm text-gray-600 mt-1">Devices Offline</div>
          <div className="text-xs text-gray-400 mt-1">
            requires attention
          </div>
        </div>
        <div className="bg-white rounded-lg shadow-sm border border-gray-100 p-5 text-center">
          <div className="text-3xl font-bold text-blue-600">{activeSchedulesCount}</div>
          <div className="text-sm text-gray-600 mt-1">Active Schedules</div>
          <div className="text-xs text-gray-400 mt-1">
            of {schedules.length} configured
          </div>
        </div>
      </div>

      {/* Real-time Telemetry Section */}
      <div className="bg-white rounded-lg shadow-sm border border-gray-100 p-6">
        <div className="flex items-center justify-between mb-4">
          <div>
            <h3 className="text-lg font-semibold text-gray-800">
              Real-time Environmental Telemetry
            </h3>
            <p className="text-xs text-gray-500">
              Live sensor telemetry transmitted via BLE / MQTT to Clock Gateway
            </p>
          </div>
          <a
            href="/devices"
            className="text-xs text-blue-600 hover:text-blue-800 font-medium"
          >
            Manage Sensors →
          </a>
        </div>

        <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
          <TelemetryChart
            data={tempTelemetry.length > 0 ? tempTelemetry : telemetryData}
            title="Temperature Readings"
            unit="°C"
            color="blue"
            loading={loading && telemetryData.length === 0}
            height={190}
          />
          <TelemetryChart
            data={humTelemetry.length > 0 ? humTelemetry : []}
            title="Humidity Readings"
            unit="%"
            color="emerald"
            loading={loading && telemetryData.length === 0}
            height={190}
          />
        </div>
      </div>

      {/* Upcoming Schedules (FSD §6.2.1) */}
      <div className="bg-white rounded-lg shadow-sm border border-gray-100">
        <div className="px-6 py-4 border-b flex items-center justify-between">
          <div>
            <h3 className="text-lg font-semibold text-gray-800">Upcoming Schedules</h3>
            <p className="text-xs text-gray-400">Next scheduled alarms and automation triggers</p>
          </div>
          <a
            href="/schedules"
            className="text-sm text-blue-600 hover:text-blue-800 font-medium"
          >
            View all ({schedules.length}) →
          </a>
        </div>
        <div className="p-4">
          {upcomingSchedules.length === 0 ? (
            <div className="text-center py-6 text-gray-400 text-sm">
              No schedules active. Create alarms or automations in Schedules.
            </div>
          ) : (
            <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-4 gap-3">
              {upcomingSchedules.map((s) => (
                <div
                  key={s.id}
                  className={`p-3 rounded-lg border text-left transition-all ${
                    s.enabled
                      ? 'border-blue-100 bg-blue-50/40'
                      : 'border-gray-100 bg-gray-50 text-gray-400'
                  }`}
                >
                  <div className="flex items-center justify-between">
                    <span className="text-xs font-semibold uppercase tracking-wider text-blue-800">
                      {s.type}
                    </span>
                    <span
                      className={`text-[10px] px-1.5 py-0.5 rounded font-medium ${
                        s.enabled ? 'bg-green-100 text-green-700' : 'bg-gray-200 text-gray-600'
                      }`}
                    >
                      {s.enabled ? 'Enabled' : 'Disabled'}
                    </span>
                  </div>
                  <div className="text-xl font-bold text-gray-800 mt-2 font-mono">
                    {String(s.hour).padStart(2, '0')}:{String(s.minute).padStart(2, '0')}
                  </div>
                  <div className="text-xs text-gray-600 truncate mt-1">
                    {s.title || (s.action_command ? `Action: ${s.action_command}` : 'Alarm')}
                  </div>
                </div>
              ))}
            </div>
          )}
        </div>
      </div>

      {/* Device list preview */}
      <div className="bg-white rounded-lg shadow-sm border border-gray-100">
        <div className="px-6 py-4 border-b flex items-center justify-between">
          <h3 className="text-lg font-semibold text-gray-800">Connected Devices</h3>
          <a
            href="/devices"
            className="text-sm text-blue-600 hover:text-blue-800 font-medium"
          >
            View all →
          </a>
        </div>
        <div className="overflow-x-auto">
          {devices.length === 0 ? (
            <div className="p-6 text-center text-gray-500">No devices registered.</div>
          ) : (
            <table className="min-w-full divide-y divide-gray-200">
              <thead>
                <tr>
                  <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase">Device</th>
                  <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase">ID</th>
                  <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase">Protocol</th>
                  <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase">Last Value</th>
                  <th className="px-6 py-3 text-center text-xs font-medium text-gray-500 uppercase">Status</th>
                </tr>
              </thead>
              <tbody className="divide-y divide-gray-200">
                {devices.slice(0, 5).map((device) => (
                  <tr key={device.id}>
                    <td className="px-6 py-3">
                      <div className="font-medium text-gray-800">{device.name}</div>
                    </td>
                    <td className="px-6 py-3 text-sm text-gray-500 font-mono">{device.device_id}</td>
                    <td className="px-6 py-3 text-sm text-gray-500 uppercase">{device.protocol}</td>
                    <td className="px-6 py-3 text-sm text-gray-600 font-medium">
                      {device.last_value !== null && device.last_value !== undefined
                        ? `${device.last_value}`
                        : '—'}
                    </td>
                    <td className="px-6 py-3 text-center">
                      <StatusBadge status={device.status} />
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          )}
        </div>
      </div>

      {/* Recent events */}
      <div className="bg-white rounded-lg shadow-sm border border-gray-100">
        <div className="px-6 py-4 border-b flex items-center justify-between">
          <h3 className="text-lg font-semibold text-gray-800">Recent Events</h3>
          <a
            href="/events"
            className="text-sm text-blue-600 hover:text-blue-800 font-medium"
          >
            View all →
          </a>
        </div>
        <div className="p-6">
          {events.length === 0 ? (
            <div className="text-center text-gray-500">No recent events.</div>
          ) : (
            <ul className="space-y-3">
              {events.map((event) => (
                <li key={event.id} className="flex items-start gap-3">
                  <span className="text-xs text-gray-400 mt-0.5 min-w-[80px]">
                    {new Date(event.occurred_at).toLocaleTimeString()}
                  </span>
                  <div className="flex-1">
                    <span className="font-medium text-sm text-gray-800">{event.event_type}</span>
                    {event.device_id && (
                      <span className="text-sm text-gray-500"> — device: {event.device_id}</span>
                    )}
                  </div>
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
