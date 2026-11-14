import React, { useState, useEffect, useCallback, useRef } from 'react'
import {
  QueueListIcon,
  CheckCircleIcon,
  CommandLineIcon,
  BoltIcon,
  ClockIcon,
  BellAlertIcon,
  SignalIcon,
  SignalSlashIcon,
  ExclamationCircleIcon,
  ShieldExclamationIcon,
  CircleStackIcon,
  ArrowPathIcon,
  FunnelIcon,
  ClipboardDocumentCheckIcon,
  ClipboardDocumentIcon,
} from '@heroicons/react/24/outline'
import { getActiveGateway, setActiveGateway, eventsAPI, gatewaysAPI } from '../api/client'

const EVENT_TYPE_CONFIG = {
  command_ack: {
    color: 'bg-emerald-500/10 text-emerald-400 border-emerald-500/20',
    label: 'Command ACK',
    icon: CheckCircleIcon,
  },
  device_command: {
    color: 'bg-sky-500/10 text-sky-400 border-sky-500/20',
    label: 'Device Command',
    icon: CommandLineIcon,
  },
  rule_executed: {
    color: 'bg-cyan-500/10 text-cyan-400 border-cyan-500/20',
    label: 'Rule Executed',
    icon: BoltIcon,
  },
  rule_action: {
    color: 'bg-cyan-500/10 text-cyan-400 border-cyan-500/20',
    label: 'Rule Action',
    icon: BoltIcon,
  },
  rule_trigger: {
    color: 'bg-cyan-500/10 text-cyan-400 border-cyan-500/20',
    label: 'Rule Trigger',
    icon: BoltIcon,
  },
  safety_interlock: {
    color: 'bg-rose-500/10 text-rose-400 border-rose-500/20',
    label: 'Safety Interlock',
    icon: ShieldExclamationIcon,
  },
  compressor_dwell_lock: {
    color: 'bg-amber-500/10 text-amber-400 border-amber-500/20',
    label: 'Dwell Lock',
    icon: ClockIcon,
  },
  command_rejected: {
    color: 'bg-rose-500/10 text-rose-400 border-rose-500/20',
    label: 'Command Rejected',
    icon: ExclamationCircleIcon,
  },
  schedule_triggered: {
    color: 'bg-blue-500/10 text-blue-400 border-blue-500/20',
    label: 'Schedule Trigger',
    icon: ClockIcon,
  },
  alarm_trigger: {
    color: 'bg-amber-500/10 text-amber-400 border-amber-500/20',
    label: 'Alarm Trigger',
    icon: BellAlertIcon,
  },
  alarm_triggered: {
    color: 'bg-amber-500/10 text-amber-400 border-amber-500/20',
    label: 'Báo Thức (Alarm)',
    icon: BellAlertIcon,
  },
  device_connected: {
    color: 'bg-emerald-500/10 text-emerald-400 border-emerald-500/20',
    label: 'Connected',
    icon: SignalIcon,
  },
  device_disconnected: {
    color: 'bg-amber-500/10 text-amber-400 border-amber-500/20',
    label: 'Disconnected',
    icon: SignalSlashIcon,
  },
  error: {
    color: 'bg-rose-500/10 text-rose-400 border-rose-500/20',
    label: 'Error',
    icon: ExclamationCircleIcon,
  },
  fail_safe_cutoff: {
    color: 'bg-rose-500/10 text-rose-400 border-rose-500/20',
    label: 'Safety Cutoff',
    icon: ShieldExclamationIcon,
  },
  telemetry_backfill: {
    color: 'bg-teal-500/10 text-teal-400 border-teal-500/20',
    label: 'Telemetry Backfill',
    icon: CircleStackIcon,
  },
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
    if (payload.command) return `Lệnh: ${payload.command}`
    if (payload.action_command) return `Hành động: ${payload.action_command}`
    if (payload.rule_id) return `Quy tắc #${payload.rule_id}`
    if (payload.target_device_id) return `Mục tiêu: ${payload.target_device_id}`
    return Object.keys(payload)
      .map((k) => `${k}: ${payload[k]}`)
      .slice(0, 2)
      .join(' | ')
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
  const [copiedId, setCopiedId] = useState(null)

  const [filters, setFilters] = useState({
    type: '',
    from: '',
    to: '',
    limit: 50,
  })

  const activeGatewayRef = useRef(gatewayId)
  activeGatewayRef.current = gatewayId

  useEffect(() => {
    gatewaysAPI
      .list()
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

  const fetchEvents = useCallback(
    async (targetGatewayId, currentFilters, offset = 0, isAppend = false) => {
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
        setError(err?.message || 'Không thể tải nhật ký sự kiện từ máy chủ')
      } finally {
        setLoading(false)
      }
    },
    []
  )

  useEffect(() => {
    if (gatewayId) {
      fetchEvents(gatewayId, filters, 0, false)
    }
  }, [gatewayId, fetchEvents])

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

  const handleCopyJson = (eventId, jsonText) => {
    if (typeof window !== 'undefined' && window.navigator?.clipboard) {
      window.navigator.clipboard.writeText(jsonText)
      setCopiedId(eventId)
      setTimeout(() => setCopiedId(null), 2000)
    }
  }

  return (
    <div className="space-y-6">
      {/* Header Bar */}
      <div className="flex flex-col sm:flex-row sm:items-center justify-between gap-4">
        <div>
          <div className="flex items-center gap-2.5">
            <h1 className="text-xl font-bold tracking-tight text-zinc-100">
              Nhật Ký Sự Kiện & Audit Trail
            </h1>
            <span className="text-xs font-mono px-2 py-0.5 rounded-full bg-zinc-800 text-zinc-300 border border-zinc-700">
              {events.length} sự kiện
            </span>
          </div>
          <p className="text-xs text-zinc-400 mt-1">
            Giám sát thời gian thực các sự kiện MQTT, kích hoạt rơ-le, lịch hẹn và cảnh báo hệ thống.
          </p>
        </div>

        <div className="flex items-center gap-3">
          {availableGateways.length > 1 && (
            <select
              value={gatewayId || ''}
              onChange={(e) => handleGatewayChange(e.target.value)}
              className="px-3 py-1.5 bg-dark-900 border border-zinc-800 rounded-lg text-xs text-zinc-100 font-mono focus:outline-none focus:border-emerald-500"
            >
              {availableGateways.map((gw) => (
                <option key={gw.id} value={gw.id}>
                  {gw.name || gw.id}
                </option>
              ))}
            </select>
          )}

          {/* Auto Refresh Toggle */}
          <button
            type="button"
            onClick={() => setAutoRefresh(!autoRefresh)}
            className={`inline-flex items-center gap-2 px-3 py-1.5 rounded-lg text-xs font-medium border transition-colors ${
              autoRefresh
                ? 'bg-emerald-500/10 text-emerald-400 border-emerald-500/30'
                : 'bg-dark-900 text-zinc-400 border-zinc-800 hover:text-zinc-200'
            }`}
          >
            <span
              className={`w-2 h-2 rounded-full ${
                autoRefresh ? 'bg-emerald-400 animate-ping' : 'bg-zinc-600'
              }`}
            />
            <span>Tự làm mới (10s)</span>
          </button>

          {/* Manual Refresh */}
          <button
            onClick={() => fetchEvents(gatewayId, filters, 0, false)}
            disabled={loading}
            className="p-2 rounded-lg bg-dark-900 border border-zinc-800 text-zinc-400 hover:text-zinc-200 hover:bg-zinc-800/80 transition-colors focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-emerald-500"
            title="Làm mới danh sách"
          >
            <ArrowPathIcon className={`w-4 h-4 ${loading ? 'animate-spin text-emerald-400' : ''}`} />
          </button>
        </div>
      </div>

      {/* Quick Filter Chips */}
      <div className="flex flex-wrap items-center gap-2 bg-dark-900 border border-zinc-800 p-2.5 rounded-xl">
        <span className="text-xs text-zinc-500 font-medium pl-1 flex items-center gap-1.5">
          <FunnelIcon className="w-3.5 h-3.5" /> Lọc nhanh:
        </span>
        {[
          { id: '', label: 'Tất cả' },
          { id: 'command_ack', label: 'Command ACK', color: 'emerald' },
          { id: 'rule_executed', label: 'Rule Executions', color: 'cyan' },
          { id: 'schedule_triggered', label: 'Schedules', color: 'blue' },
          { id: 'alarm_triggered', label: 'Alarms', color: 'amber' },
          { id: 'error', label: 'Errors', color: 'rose' },
        ].map((item) => {
          const isActive = filters.type === item.id
          return (
            <button
              key={item.id}
              onClick={() => handleQuickFilter(item.id)}
              className={`px-3 py-1 rounded-lg text-xs font-medium transition-colors ${
                isActive
                  ? 'bg-zinc-800 text-zinc-100 border border-zinc-700 shadow-sm'
                  : 'text-zinc-400 hover:text-zinc-200 hover:bg-zinc-850'
              }`}
            >
              {item.label}
            </button>
          )
        })}
      </div>

      {/* Advanced Filter Panel */}
      <div className="bg-dark-900 border border-zinc-800 rounded-xl p-4">
        <div className="grid grid-cols-1 sm:grid-cols-2 md:grid-cols-5 gap-3 items-end">
          <div>
            <label className="block text-[11px] font-medium text-zinc-400 mb-1">
              Loại sự kiện (Type Filter)
            </label>
            <input
              name="type"
              type="text"
              placeholder="ví dụ: command_ack"
              value={filters.type}
              onChange={handleFilterChange}
              onKeyDown={(e) => e.key === 'Enter' && handleApplyFilters()}
              className="w-full px-3 py-1.5 bg-zinc-950 border border-zinc-800 rounded-lg text-xs text-zinc-100 placeholder-zinc-600 focus:outline-none focus:border-emerald-500"
              spellCheck={false}
            />
          </div>
          <div>
            <label className="block text-[11px] font-medium text-zinc-400 mb-1">
              Từ mốc thời gian
            </label>
            <input
              name="from"
              type="datetime-local"
              value={filters.from}
              onChange={handleFilterChange}
              className="w-full px-3 py-1.5 bg-zinc-950 border border-zinc-800 rounded-lg text-xs text-zinc-100 focus:outline-none focus:border-emerald-500"
            />
          </div>
          <div>
            <label className="block text-[11px] font-medium text-zinc-400 mb-1">
              Đến mốc thời gian
            </label>
            <input
              name="to"
              type="datetime-local"
              value={filters.to}
              onChange={handleFilterChange}
              className="w-full px-3 py-1.5 bg-zinc-950 border border-zinc-800 rounded-lg text-xs text-zinc-100 focus:outline-none focus:border-emerald-500"
            />
          </div>
          <div>
            <label className="block text-[11px] font-medium text-zinc-400 mb-1">
              Số lượng bản ghi
            </label>
            <select
              name="limit"
              value={filters.limit}
              onChange={handleFilterChange}
              className="w-full px-3 py-1.5 bg-zinc-950 border border-zinc-800 rounded-lg text-xs text-zinc-100 focus:outline-none focus:border-emerald-500"
            >
              <option value="25">25 bản ghi</option>
              <option value="50">50 bản ghi</option>
              <option value="100">100 bản ghi</option>
              <option value="200">200 bản ghi</option>
            </select>
          </div>
          <div className="flex gap-2">
            <button
              onClick={handleApplyFilters}
              disabled={loading}
              className="flex-1 px-4 py-2 bg-emerald-500 hover:bg-emerald-400 text-dark-950 font-semibold rounded-lg text-xs transition-colors shadow-sm disabled:opacity-50"
            >
              Áp dụng
            </button>
            <button
              onClick={handleClearFilters}
              disabled={loading}
              className="px-3 py-2 bg-zinc-800 hover:bg-zinc-750 text-zinc-300 font-medium rounded-lg text-xs transition-colors border border-zinc-750"
              title="Đặt lại bộ lọc"
            >
              Đặt lại
            </button>
          </div>
        </div>
      </div>

      {/* Error alert */}
      {error && (
        <div className="p-4 bg-rose-500/10 border border-rose-500/20 rounded-xl flex items-center justify-between text-xs text-rose-300">
          <div className="flex items-center gap-2">
            <ExclamationCircleIcon className="w-5 h-5 text-rose-400 flex-shrink-0" />
            <span>{error}</span>
          </div>
          <button
            onClick={() => fetchEvents(gatewayId, filters, 0, false)}
            className="underline font-semibold hover:text-rose-200 ml-3"
          >
            Thử lại
          </button>
        </div>
      )}

      {/* Events Table Container */}
      <div className="bg-dark-900 border border-zinc-800 rounded-xl overflow-hidden shadow-sm">
        <div className="px-5 py-3.5 border-b border-zinc-800/80 flex items-center justify-between bg-zinc-950/60">
          <div className="flex items-center gap-2">
            <QueueListIcon className="w-4 h-4 text-emerald-400" />
            <h3 className="text-xs font-semibold text-zinc-200 uppercase tracking-wider">
              Danh Sách Sự Kiện ({events.length} đã tải)
            </h3>
            {gatewayId && (
              <span className="text-[10px] bg-zinc-800 text-zinc-400 px-2 py-0.5 rounded font-mono border border-zinc-700">
                {gatewayId}
              </span>
            )}
          </div>
          <span className="text-[11px] text-zinc-500 font-mono">Sắp xếp: Mới nhất trước</span>
        </div>

        <div className="overflow-x-auto">
          {loading && events.length === 0 ? (
            <div className="p-16 text-center text-zinc-500 flex flex-col items-center justify-center gap-3">
              <div className="w-8 h-8 border-2 border-emerald-500 border-t-transparent rounded-full animate-spin" />
              <span className="text-xs text-zinc-400">Đang tải nhật ký sự kiện từ Gateway...</span>
            </div>
          ) : events.length === 0 ? (
            <div className="p-16 text-center text-zinc-500 flex flex-col items-center justify-center gap-2">
              <QueueListIcon className="w-8 h-8 text-zinc-600 mb-1" />
              <p className="text-sm font-semibold text-zinc-300">Không tìm thấy sự kiện nào</p>
              <p className="text-xs text-zinc-500 max-w-sm">
                {!gatewayId
                  ? 'Chưa chọn Gateway nào.'
                  : 'Không có bản ghi sự kiện nào phù hợp với bộ lọc hiện tại hoặc Gateway chưa phát sinh sự kiện mới.'}
              </p>
            </div>
          ) : (
            <table className="min-w-full divide-y divide-zinc-800/80 text-left">
              <thead className="bg-zinc-950/80 text-[11px] font-mono text-zinc-400 uppercase tracking-wider">
                <tr>
                  <th className="px-5 py-3 w-44">Thời gian</th>
                  <th className="px-5 py-3 w-40">Loại sự kiện</th>
                  <th className="px-5 py-3 w-36">Thiết bị</th>
                  <th className="px-5 py-3">Chi tiết & Dữ liệu Payload</th>
                </tr>
              </thead>
              <tbody className="divide-y divide-zinc-800/60">
                {events.map((event) => {
                  const cfg = EVENT_TYPE_CONFIG[event.event_type] || {
                    color: 'bg-zinc-800 text-zinc-300 border-zinc-700',
                    label: event.event_type,
                    icon: QueueListIcon,
                  }
                  const Icon = cfg.icon
                  const summary = formatPayloadSummary(event.payload)
                  const jsonStr = formatPayloadJson(event.payload)
                  const isCopied = copiedId === event.id

                  return (
                    <tr
                      key={event.id}
                      className="hover:bg-zinc-850/50 transition-colors duration-100"
                    >
                      {/* Timestamp */}
                      <td className="px-5 py-3.5 text-xs font-mono text-zinc-300 whitespace-nowrap tabular-nums">
                        <div>
                          {new Date(event.occurred_at).toLocaleDateString('vi-VN', {
                            year: 'numeric',
                            month: '2-digit',
                            day: '2-digit',
                          })}
                        </div>
                        <div className="text-zinc-500 text-[11px]">
                          {new Date(event.occurred_at).toLocaleTimeString('vi-VN', {
                            hour: '2-digit',
                            minute: '2-digit',
                            second: '2-digit',
                          })}
                        </div>
                      </td>

                      {/* Event Type Badge */}
                      <td className="px-5 py-3.5 whitespace-nowrap">
                        <span
                          className={`inline-flex items-center gap-1.5 px-2.5 py-1 rounded-full text-xs font-medium border ${cfg.color}`}
                        >
                          <Icon className="w-3.5 h-3.5" />
                          <span>{cfg.label}</span>
                        </span>
                      </td>

                      {/* Target Device */}
                      <td className="px-5 py-3.5 text-xs">
                        {event.device_id ? (
                          <span className="font-mono bg-zinc-950 text-zinc-300 px-2 py-0.5 rounded border border-zinc-800">
                            {event.device_id}
                          </span>
                        ) : (
                          <span className="text-zinc-600 italic">Toàn hệ thống</span>
                        )}
                      </td>

                      {/* Details & Payload */}
                      <td className="px-5 py-3.5 text-xs">
                        {summary ? (
                          <div className="space-y-1.5">
                            <div className="text-zinc-200 font-medium leading-relaxed">
                              {summary}
                            </div>
                            {jsonStr && (
                              <details className="group">
                                <summary className="cursor-pointer text-emerald-400 hover:text-emerald-300 text-[11px] select-none inline-flex items-center gap-1 font-mono transition-colors">
                                  <span>Xem chi tiết JSON</span>
                                  <span className="group-open:rotate-180 transition-transform text-[9px]">
                                    ▼
                                  </span>
                                </summary>
                                <div className="relative mt-2">
                                  <pre className="text-[11px] bg-zinc-950 text-emerald-400 p-3 rounded-lg font-mono overflow-x-auto max-h-48 border border-zinc-800 shadow-inner">
                                    {jsonStr}
                                  </pre>
                                  <button
                                    type="button"
                                    onClick={() => handleCopyJson(event.id, jsonStr)}
                                    className="absolute top-2 right-2 p-1.5 rounded-md bg-zinc-900 border border-zinc-750 text-zinc-400 hover:text-zinc-200 transition-colors"
                                    title="Sao chép JSON"
                                    aria-label="Sao chép JSON"
                                  >
                                    {isCopied ? (
                                      <ClipboardDocumentCheckIcon className="w-4 h-4 text-emerald-400" />
                                    ) : (
                                      <ClipboardDocumentIcon className="w-4 h-4" />
                                    )}
                                  </button>
                                </div>
                              </details>
                            )}
                          </div>
                        ) : (
                          <span className="text-zinc-600 italic">—</span>
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
          <div className="px-6 py-4 bg-zinc-950/60 border-t border-zinc-800/80 text-center">
            <button
              onClick={handleLoadMore}
              disabled={loading}
              className="px-5 py-2 bg-dark-900 border border-zinc-800 rounded-lg text-xs font-semibold text-zinc-300 hover:text-zinc-100 hover:bg-zinc-800 transition-colors shadow-sm disabled:opacity-50"
            >
              {loading ? 'Đang nạp dữ liệu...' : `Tải thêm sự kiện (+${filters.limit})`}
            </button>
          </div>
        )}
      </div>
    </div>
  )
}

export default Events