import React, { useState, useEffect } from 'react'
import StatusBadge from '../components/StatusBadge'
import { getActiveGateway, gatewaysAPI, devicesAPI, eventsAPI, telemetryAPI } from '../api/client'

function Dashboard() {
  const gatewayId = getActiveGateway()
  const [gateway, setGateway] = useState(null)
  const [devices, setDevices] = useState([])
  const [events, setEvents] = useState([])
  const [loading, setLoading] = useState(true)

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
      const [gwRes, devRes, eventRes] = await Promise.all([
        gatewaysAPI.get(gatewayId),
        devicesAPI.list(gatewayId),
        eventsAPI.list(gatewayId, { limit: 10 }),
      ])
      setGateway(gwRes.data)
      setDevices(devRes.data || [])
      setEvents(eventRes.data || [])
    } catch (err) {
      console.error('Failed to load dashboard data:', err)
    } finally {
      setLoading(false)
    }
  }

  const onlineCount = devices.filter(d => d.status === 'online').length
  const offlineCount = devices.filter(d => d.status === 'offline').length
  const unknownCount = devices.filter(d => d.status === 'unknown').length

  if (!gatewayId) {
    return (
      <div className="text-center py-12">
        <h2 className="text-xl font-semibold text-gray-700">No gateway selected</h2>
        <p className="text-gray-500 mt-2">Use the sidebar to select or add a gateway.</p>
      </div>
    )
  }

  if (loading) {
    return <div className="text-center py-12 text-gray-500">Loading dashboard…</div>
  }

  return (
    <div className="space-y-6">
      {/* Header */}
      <div>
        <h1 className="text-2xl font-bold text-gray-800">
          {gateway?.name || gatewayId}
        </h1>
        <p className="text-sm text-gray-500 mt-1">
          Gateway ID: <span className="font-mono">{gatewayId}</span>
        </p>
      </div>

      {/* Gateway status card */}
      <div className="bg-white rounded-lg shadow p-6">
        <div className="flex items-center justify-between">
          <div>
            <h3 className="text-lg font-semibold text-gray-800">Gateway Status</h3>
            <p className="text-sm text-gray-500 mt-1">
              Firmware: {gateway?.firmware_version || 'Unknown'}
            </p>
            <p className="text-sm text-gray-500">
              Last seen: {gateway?.last_seen ? new Date(gateway.last_seen).toLocaleString() : 'Never'}
            </p>
            <p className="text-sm text-gray-500">
              Timezone: {gateway?.timezone || 'Not set'}
            </p>
          </div>
          <StatusBadge status={gateway?.status || 'unknown'} />
        </div>
      </div>

      {/* Device summary cards */}
      <div className="grid grid-cols-1 md:grid-cols-3 gap-4">
        <div className="bg-white rounded-lg shadow p-6 text-center">
          <div className="text-3xl font-bold text-green-600">{onlineCount}</div>
          <div className="text-sm text-gray-500 mt-1">Devices Online</div>
          <div className="text-xs text-gray-400 mt-2">
            of {devices.length} total devices
          </div>
        </div>
        <div className="bg-white rounded-lg shadow p-6 text-center">
          <div className="text-3xl font-bold text-red-600">{offlineCount}</div>
          <div className="text-sm text-gray-500 mt-1">Devices Offline</div>
        </div>
        <div className="bg-white rounded-lg shadow p-6 text-center">
          <div className="text-3xl font-bold text-gray-400">{unknownCount}</div>
          <div className="text-sm text-gray-500 mt-1">Status Unknown</div>
        </div>
      </div>

      {/* Device list preview */}
      <div className="bg-white rounded-lg shadow">
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
                  <th className="px-6 py-3 text-center text-xs font-medium text-gray-505 uppercase">Status</th>
                </tr>
              </thead>
              <tbody className="divide-y divide-gray-200">
                {devices.slice(0, 5).map((device) => (
                  <tr key={device.id}>
                    <td className="px-6 py-3">
                      <div className="font-medium text-gray-800">{device.name}</div>
                    </td>
                    <td className="px-6 py-3 text-sm text-gray-500 font-mono">{device.device_id}</td>
                    <td className="px-6 py-3 text-sm text-gray-500">{device.protocol}</td>
                    <td className="px-6 py-3 text-sm text-gray-500">
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
      <div className="bg-white rounded-lg shadow">
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
