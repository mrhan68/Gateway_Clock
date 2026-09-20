import React, { useState, useEffect, useCallback, useRef } from 'react'
import { getActiveGateway, setActiveGateway, eventsAPI, gatewaysAPI } from '../api/client'

const EVENT_TYPE_CONFIG = {
  command_ack: { color: 'bg-emerald-100 text-emerald-800 border-emerald-200', label: 'Command ACK', icon: '✅' },
  device_command: { color: 'bg-indigo-100 text-indigo-800 border-indigo-200', label: 'Device Command', icon: '📤' },
  rule_executed: { color: 'bg-purple-100 text-purple-800 border-purple-200', label: 'Rule Executed', icon: '⚡' },
  rule_trigger: { color: 'bg-purple-100 text-purple-800 border-purple-200', label: 'Rule Trigger', icon: '⚡' },
  schedule_triggered: { color: 'bg-blue-100 text-blue-800 border-blue-200', label: 'Schedule', icon: '⏰' },
  alarm_trigger: { color: 'bg-blue-100 text-blue-800 border-blue-200', label: 'Alarm Trigger', icon: '🔔' },
  device_connected: { color: 'bg-green-100 text-green-800 border-green-200', label: 'Connected', icon: '🟢' },
  device_disconnected: { color: 'bg-amber-100 text-amber-800 border-amber-200', label: 'Disconnected', icon: '🟡' },
  error: { color: 'bg-red-100 text-red-800 border-red-200', label: 'Error', icon: '⚠️' },
  fail_safe_cutoff: { color: 'bg-rose-100 text-rose-800 border-rose-200', label: 'Safety Cutoff', icon: '🚨' },
  telemetry_backfill: { color: 'bg-teal-100 text-teal-800 border-teal-200', label: 'Backfill', icon: '💾' },
}

function formatPayloadSummary(payload) {
  if (!payload) return null
  if (typeof payload === 'string') {
    try {
      payload = JSON.parse(payload)
    } catch {
      return payload
    }
  }
  if (typeof payload === 'object') {
    if (payload.message) return payload.message
    if (payload.command) return `Command: ${payload.command}`
    if (payload.action_command) return `Action: ${payload.action_command}`
    if (payload.rule_id) return `Rule #${payload.rule_id}`
    if (payload.target_device_id) return `Target: ${payload.target_device_id}`
    return Object.keys(payload).map(k => `${k}: ${payload[k]}`).slice(0, 2).join(' | ')
  }
  return String(payload)
}

function formatPayloadJson(payload) {
  if (!payload) return ''
  if (typeof payload === 'object') {
    return JSON.stringify(payload, null, 2)
  }
  try {
    const parsed = JSON.parse(payload)
    return JSON.stringify(parsed, null, 2)
  } catch {
    return String(payload)
  }
}

function Events() {
  const [gatewayId, setGatewayId] = useState(getActiveGateway())
  const [availableGateways, setAvailableGateways] = useState([])
  const [events, setEvents] = useState([])
  const [loading, setLoading] = useState(true)
  const [error, setError] = useState(null)
  const [autoRefresh, setAutoRefresh] = useState(false)
  const [hasMore, setHasMore] = useState(true)

  const [filters, setFilters] = useState({
    type: '',
    from: '',
    to: '',
    limit: 50,
  })

  // Keep a ref to active gatewayId for interval callbacks
  const activeGatewayRef = useRef(gatewayId)
  activeGatewayRef.current = gatewayId

  // Initial gateway detection
  useEffect(() => {
    gatewaysAPI.list()
      .then((res) => {
        const list = res.data || []
        setAvailableGateways(list)
        if (list.length > 0 && !getActiveGateway()) {
          setActiveGateway(list[0].id)
          setGatewayId(list[0].id)
        }
      })
      .catch(console.error)
  }, [])

  // Core event fetching function
  const fetchEvents = useCallback(async (targetGatewayId, currentFilters, offset = 0, isAppend = false) => {
    if (!targetGatewayId) return
    setLoading(true)
    setError(null)

    try {
      const params = {
        limit: currentFilters.limit || 50,
        offset,
      }
      if (currentFilters.type && currentFilters.type.trim()) {
        params.type = currentFilters.type.trim()
      }
      if (currentFilters.from) {
        // Convert datetime-local string to ISO format
        const fromDate = new Date(currentFilters.from)
        if (!isNaN(fromDate.getTime())) {
          params.from = fromDate.toISOString()
        }
      }
      if (currentFilters.to) {
        const toDate = new Date(currentFilters.to)
        if (!isNaN(toDate.getTime())) {
          params.to = toDate.toISOString()
        }
      }

      const res = await eventsAPI.list(targetGatewayId, params)
      const data = res.data || []

      if (isAppend) {
        setEvents((prev) => [...prev, ...data])
      } else {
        setEvents(data)
      }

      setHasMore(data.length >= (currentFilters.limit || 50))
    } catch (err) {
      console.error('Failed to load events:', err)
      setError(err?.message || 'Failed to load event logs from server')
    } finally {
      setLoading(false)
    }
  }, [])

  // Fetch when gatewayId changes
  useEffect(() => {
    if (gatewayId) {
      fetchEvents(gatewayId, filters, 0, false)
    }
  }, [gatewayId, fetchEvents])

  // Auto-refresh timer
  useEffect(() => {
    if (!autoRefresh || !gatewayId) return
    const interval = setInterval(() => {
      fetchEvents(activeGatewayRef.current, filters, 0, false)
    }, 10000)
    return () => clearInterval(interval)
  }, [autoRefresh, gatewayId, filters, fetchEvents])

  const handleGatewayChange = (newId) => {
    setActiveGateway(newId)
    setGatewayId(newId)
  }

  const handleFilterChange = (e) => {
    setFilters((prev) => ({ ...prev, [e.target.name]: e.target.value }))
  }

  const handleApplyFilters = () => {
    fetchEvents(gatewayId, filters, 0, false)
  }

  const handleQuickFilter = (type) => {
    const updated = { ...filters, type }
    setFilters(updated)
    fetchEvents(gatewayId, updated, 0, false)
  }

  const handleClearFilters = () => {
    const reset = { type: '', from: '', to: '', limit: 50 }
    setFilters(reset)
    fetchEvents(gatewayId, reset, 0, false)
  }

  const handleLoadMore = () => {
    fetchEvents(gatewayId, filters, events.length, true)
  }

  return (
    <div className="space-y-4">
      {/* Header */}
      <div className="flex flex-col sm:flex-row sm:items-center justify-between gap-4">
        <div>
          <h1 className="text-2xl font-bold text-gray-800">Event Logs & Audit Trail</h1>
          <p className="text-sm text-gray-500">
            Real-time system events, rule executions, command acknowledgements, and hardware alerts.
          </p>
        </div>

        <div className="flex items-center gap-3">
          {/* Gateway selector if multiple */}
          {availableGateways.length > 1 && (
            <select
              value={gatewayId || ''}
              onChange={(e) => handleGatewayChange(e.target.value)}
              className="px-3 py-1.5 bg-white border border-gray-300 rounded-md text-sm font-medium shadow-sm"
            >
              {availableGateways.map((gw) => (
                <option key={gw.id} value={gw.id}>
                  {gw.name || gw.id}
                </option>
              ))}
            </select>
          )}

          {/* Auto Refresh Toggle */}
          <label className="flex items-center gap-1.5 text-xs text-gray-600 cursor-pointer bg-white px-3 py-2 border rounded-md shadow-sm select-none">
            <input
              type="checkbox"
              checked={autoRefresh}
              onChange={(e) => setAutoRefresh(e.target.checked)}
              className="rounded text-blue-600 focus:ring-blue-500"
            />
            Auto-refresh (10s)
          </label>

          {/* Manual Refresh */}
          <button
            onClick={() => fetchEvents(gatewayId, filters, 0, false)}
            disabled={loading}
            className="flex items-center gap-1.5 px-3 py-2 bg-white border border-gray-300 rounded-md text-xs font-medium text-gray-700 hover:bg-gray-50 shadow-sm disabled:opacity-50"
          >
            <span className={loading ? 'animate-spin' : ''}>🔄</span>
            Refresh
          </button>
        </div>
      </div>

      {/* Quick Filter Chips */}
      <div className="flex flex-wrap items-center gap-2 pt-1">
        <span className="text-xs font-medium text-gray-500 mr-1">Quick Filters:</span>
        <button
          onClick={() => handleQuickFilter('')}
          className={`px-2.5 py-1 rounded-full text-xs font-medium transition-colors ${
            !filters.type ? 'bg-gray-800 text-white' : 'bg-gray-100 text-gray-700 hover:bg-gray-200'
          }`}
        >
          All
        </button>
        <button
          onClick={() => handleQuickFilter('command_ack')}
          className={`px-2.5 py-1 rounded-full text-xs font-medium transition-colors ${
            filters.type === 'command_ack' ? 'bg-emerald-600 text-white' : 'bg-emerald-50 text-emerald-700 hover:bg-emerald-100'
          }`}
        >
          ✅ Command ACKs
        </button>
        <button
          onClick={() => handleQuickFilter('rule_executed')}
          className={`px-2.5 py-1 rounded-full text-xs font-medium transition-colors ${
            filters.type === 'rule_executed' ? 'bg-purple-600 text-white' : 'bg-purple-50 text-purple-700 hover:bg-purple-100'
          }`}
        >
          ⚡ Rule Executions
        </button>
        <button
          onClick={() => handleQuickFilter('schedule_triggered')}
          className={`px-2.5 py-1 rounded-full text-xs font-medium transition-colors ${
            filters.type === 'schedule_triggered' ? 'bg-blue-600 text-white' : 'bg-blue-50 text-blue-700 hover:bg-blue-100'
          }`}
        >
          ⏰ Schedules
        </button>
        <button
          onClick={() => handleQuickFilter('error')}
          className={`px-2.5 py-1 rounded-full text-xs font-medium transition-colors ${
            filters.type === 'error' ? 'bg-red-600 text-white' : 'bg-red-50 text-red-700 hover:bg-red-100'
          }`}
        >
          ⚠️ Errors
        </button>
      </div>

      {/* Filter Panel */}
      <div className="bg-white rounded-lg shadow-sm border border-gray-200 p-4">
        <div className="grid grid-cols-1 sm:grid-cols-2 md:grid-cols-5 gap-3 items-end">
          <div>
            <label className="block text-xs font-medium text-gray-700 mb-1">Event Type Filter</label>
            <input
              name="type"
              type="text"
              placeholder="e.g. command_ack"
              value={filters.type}
              onChange={handleFilterChange}
              onKeyDown={(e) => e.key === 'Enter' && handleApplyFilters()}
              className="w-full px-3 py-2 border border-gray-300 rounded-md text-sm focus:ring-blue-500 focus:border-blue-500"
            />
          </div>
          <div>
            <label className="block text-xs font-medium text-gray-700 mb-1">From Timestamp</label>
            <input
              name="from"
              type="datetime-local"
              value={filters.from}
              onChange={handleFilterChange}
              className="w-full px-3 py-2 border border-gray-300 rounded-md text-sm focus:ring-blue-500 focus:border-blue-500"
            />
          </div>
          <div>
            <label className="block text-xs font-medium text-gray-700 mb-1">To Timestamp</label>
            <input
              name="to"
              type="datetime-local"
              value={filters.to}
              onChange={handleFilterChange}
              className="w-full px-3 py-2 border border-gray-300 rounded-md text-sm focus:ring-blue-500 focus:border-blue-500"
            />
          </div>
          <div>
            <label className="block text-xs font-medium text-gray-700 mb-1">Batch Limit</label>
            <select
              name="limit"
              value={filters.limit}
              onChange={handleFilterChange}
              className="w-full px-3 py-2 border border-gray-300 rounded-md text-sm focus:ring-blue-500 focus:border-blue-500 bg-white"
            >
              <option value="25">25 records</option>
              <option value="50">50 records</option>
              <option value="100">100 records</option>
              <option value="200">200 records</option>
            </select>
          </div>
          <div className="flex gap-2">
            <button
              onClick={handleApplyFilters}
              disabled={loading}
              className="flex-1 px-4 py-2 bg-blue-600 text-white font-medium rounded-md hover:bg-blue-700 text-sm shadow-sm transition-colors disabled:opacity-50"
            >
              Apply Filter
            </button>
            <button
              onClick={handleClearFilters}
              disabled={loading}
              className="px-3 py-2 bg-gray-100 text-gray-700 font-medium rounded-md hover:bg-gray-200 text-sm transition-colors"
              title="Reset all filters"
            >
              Clear
            </button>
          </div>
        </div>
      </div>

      {/* Error Alert */}
      {error && (
        <div className="p-4 bg-red-50 border border-red-200 rounded-lg flex items-center justify-between text-sm text-red-700">
          <span>⚠️ {error}</span>
          <button
            onClick={() => fetchEvents(gatewayId, filters, 0, false)}
            className="underline font-semibold hover:text-red-900 ml-3"
          >
            Retry
          </button>
        </div>
      )}

      {/* Events Table Container */}
      <div className="bg-white rounded-lg shadow-sm border border-gray-200 overflow-hidden">
        <div className="px-6 py-4 border-b border-gray-200 flex items-center justify-between bg-gray-50">
          <div className="flex items-center gap-2">
            <h3 className="text-base font-semibold text-gray-800">
              Logged Events ({events.length} loaded)
            </h3>
            {gatewayId && (
              <span className="text-xs bg-gray-200 text-gray-700 px-2 py-0.5 rounded font-mono">
                {gatewayId}
              </span>
            )}
          </div>
          <span className="text-xs text-gray-500">
            Ordered by newest first
          </span>
        </div>

        <div className="overflow-x-auto">
          {loading && events.length === 0 ? (
            <div className="p-12 text-center text-gray-500 flex flex-col items-center justify-center gap-3">
              <div className="w-8 h-8 border-4 border-blue-500 border-t-transparent rounded-full animate-spin"></div>
              <span>Fetching event logs from gateway...</span>
            </div>
          ) : events.length === 0 ? (
            <div className="p-12 text-center text-gray-500 flex flex-col items-center justify-center gap-2">
              <span className="text-4xl">📜</span>
              <p className="font-medium text-gray-700">No events found</p>
              <p className="text-xs text-gray-400">
                {!gatewayId
                  ? 'No active gateway selected.'
                  : 'No event records matched your filter criteria or no events have been logged yet.'}
              </p>
            </div>
          ) : (
            <table className="min-w-full divide-y divide-gray-200">
              <thead className="bg-gray-50">
                <tr>
                  <th className="px-6 py-3 text-left text-xs font-semibold text-gray-500 uppercase tracking-wider w-44">
                    Time (Local)
                  </th>
                  <th className="px-6 py-3 text-left text-xs font-semibold text-gray-500 uppercase tracking-wider w-40">
                    Event Type
                  </th>
                  <th className="px-6 py-3 text-left text-xs font-semibold text-gray-500 uppercase tracking-wider w-36">
                    Target Device
                  </th>
                  <th className="px-6 py-3 text-left text-xs font-semibold text-gray-500 uppercase tracking-wider">
                    Details & Payload
                  </th>
                </tr>
              </thead>
              <tbody className="divide-y divide-gray-100 bg-white">
                {events.map((event) => {
                  const cfg = EVENT_TYPE_CONFIG[event.event_type] || {
                    color: 'bg-gray-100 text-gray-800 border-gray-200',
                    label: event.event_type,
                    icon: '📌',
                  }
                  const summary = formatPayloadSummary(event.payload)
                  const jsonStr = formatPayloadJson(event.payload)

                  return (
                    <tr key={event.id} className="hover:bg-gray-50/80 transition-colors">
                      {/* Timestamp */}
                      <td className="px-6 py-3.5 text-xs text-gray-600 whitespace-nowrap font-mono">
                        <div>
                          {new Date(event.occurred_at).toLocaleDateString()}
                        </div>
                        <div className="text-gray-400 text-[11px]">
                          {new Date(event.occurred_at).toLocaleTimeString()}
                        </div>
                      </td>

                      {/* Event Type Badge */}
                      <td className="px-6 py-3.5 whitespace-nowrap">
                        <span className={`inline-flex items-center gap-1.5 px-2.5 py-1 rounded-full text-xs font-medium border ${cfg.color}`}>
                          <span>{cfg.icon}</span>
                          <span>{cfg.label}</span>
                        </span>
                      </td>

                      {/* Target Device */}
                      <td className="px-6 py-3.5 text-xs">
                        {event.device_id ? (
                          <span className="font-mono bg-gray-100 text-gray-700 px-2 py-0.5 rounded border border-gray-200">
                            {event.device_id}
                          </span>
                        ) : (
                          <span className="text-gray-400 italic">System-wide</span>
                        )}
                      </td>

                      {/* Payload & Details */}
                      <td className="px-6 py-3.5 text-xs">
                        {summary ? (
                          <div className="space-y-1">
                            <div className="text-gray-800 font-medium">
                              {summary}
                            </div>
                            {jsonStr && (
                              <details className="group">
                                <summary className="cursor-pointer text-blue-600 hover:text-blue-800 text-[11px] select-none inline-flex items-center gap-1">
                                  <span>View Raw JSON</span>
                                  <span className="group-open:rotate-180 transition-transform text-[9px]">▼</span>
                                </summary>
                                <pre className="mt-1.5 text-[11px] bg-slate-900 text-emerald-400 p-2.5 rounded font-mono overflow-x-auto max-h-48 border border-slate-700">
                                  {jsonStr}
                                </pre>
                              </details>
                            )}
                          </div>
                        ) : (
                          <span className="text-gray-400 italic">—</span>
                        )}
                      </td>
                    </tr>
                  )
                })}
              </tbody>
            </table>
          )}
        </div>

        {/* Load More Footer */}
        {hasMore && events.length > 0 && (
          <div className="px-6 py-4 bg-gray-50 border-t border-gray-200 text-center">
            <button
              onClick={handleLoadMore}
              disabled={loading}
              className="px-5 py-2 bg-white border border-gray-300 rounded-md text-xs font-semibold text-gray-700 hover:bg-gray-100 shadow-sm transition-colors disabled:opacity-50"
            >
              {loading ? 'Fetching More Records...' : `Load More Events (+${filters.limit})`}
            </button>
          </div>
        )}
      </div>
    </div>
  )
}

export default Events