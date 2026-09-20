import React, { useState, useEffect } from 'react'
import { getActiveGateway, eventsAPI } from '../api/client'

function Events() {
  const gatewayId = getActiveGateway()
  const [events, setEvents] = useState([])
  const [loading, setLoading] = useState(true)
  const [filters, setFilters] = useState({
    type: '',
    from: '',
    to: '',
    limit: 50,
    offset: 0,
  })

  useEffect(() => {
    if (gatewayId) loadEvents()
  }, [gatewayId])

  const loadEvents = async () => {
    if (!gatewayId) return
    setLoading(true)
    try {
      const params = {}
      if (filters.type) params.type = filters.type
      if (filters.from) params.from = filters.from
      if (filters.to) params.to = filters.to
      params.limit = filters.limit
      params.offset = filters.offset

      const res = await eventsAPI.list(gatewayId, params)
      setEvents(res.data || [])
    } catch (err) {
      console.error('Failed to load events:', err)
    } finally {
      setLoading(false)
    }
  }

  const applyFilters = () => {
    setFilters((f) => ({ ...f, offset: 0 }))
    loadEvents()
  }

  const clearFilters = () => {
    setFilters({ type: '', from: '', to: '', limit: 50, offset: 0 })
    loadEvents()
  }

  const loadMore = () => {
    setFilters((f) => ({ ...f, offset: f.offset + f.limit }))
    loadEvents()
  }

  const handleFilterChange = (e) => {
    setFilters({ ...filters, [e.target.name]: e.target.value })
  }

  const EVENT_TYPE_COLORS = {
    rule_executed: 'bg-purple-100 text-purple-800',
    schedule_triggered: 'bg-blue-100 text-blue-800',
    device_connected: 'bg-green-100 text-green-800',
    device_disconnected: 'bg-yellow-100 text-yellow-800',
    device_command: 'bg-indigo-100 text-indigo-800',
    error: 'bg-red-100 text-red-800',
  }

  if (!gatewayId) {
    return <div className="text-center py-12">No gateway selected.</div>
  }

  return (
    <div className="space-y-4">
      <h1 className="text-2xl font-bold text-gray-800">Event Log</h1>

      {/* Filters */}
      <div className="bg-white rounded-lg shadow p-4">
        <div className="grid grid-cols-1 md:grid-cols-5 gap-4">
          <div>
            <label className="block text-sm font-medium text-gray-700 mb-1">Event Type</label>
            <input
              name="type"
              type="text"
              placeholder="e.g. rule_executed"
              value={filters.type}
              onChange={handleFilterChange}
              className="w-full px-3 py-2 border rounded-md text-sm"
            />
          </div>
          <div>
            <label className="block text-sm font-medium text-gray-700 mb-1">From</label>
            <input
              name="from"
              type="datetime-local"
              value={filters.from}
              onChange={handleFilterChange}
              className="w-full px-3 py-2 border rounded-md text-sm"
            />
          </div>
          <div>
            <label className="block text-sm font-medium text-gray-700 mb-1">To</label>
            <input
              name="to"
              type="datetime-local"
              value={filters.to}
              onChange={handleFilterChange}
              className="w-full px-3 py-2 border rounded-md text-sm"
            />
          </div>
          <div>
            <label className="block text-sm font-medium text-gray-700 mb-1">Limit</label>
            <input
              name="limit"
              type="number"
              min="1"
              max="200"
              value={filters.limit}
              onChange={handleFilterChange}
              className="w-full px-3 py-2 border rounded-md text-sm"
            />
          </div>
          <div className="flex items-end gap-2">
            <button
              onClick={applyFilters}
              className="flex-1 px-4 py-2 bg-blue-600 text-white rounded-md hover:bg-blue-700 text-sm"
            >
              Apply
            </button>
            <button
              onClick={clearFilters}
              className="flex-1 px-4 py-2 bg-gray-200 text-gray-800 rounded-md hover:bg-gray-300 text-sm"
            >
              Clear
            </button>
          </div>
        </div>
      </div>

      {/* Events list */}
      <div className="bg-white rounded-lg shadow">
        <div className="px-6 py-4 border-b">
          <h3 className="text-lg font-semibold text-gray-800">
            Events ({events.length} {filters.offset > 0 ? `+${filters.offset}` : ''} loaded)
          </h3>
        </div>
        <div className="overflow-x-auto">
          {loading && events.length === 0 ? (
            <div className="p-6 text-center text-gray-500">Loading events…</div>
          ) : events.length === 0 ? (
            <div className="p-6 text-center text-gray-500">No events match your filters.</div>
          ) : (
            <table className="min-w-full divide-y divide-gray-200">
              <thead>
                <tr>
                  <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase">Time</th>
                  <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase">Type</th>
                  <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase">Device</th>
                  <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase">Payload</th>
                </tr>
              </thead>
              <tbody className="divide-y divide-gray-200">
                {events.map((event) => (
                  <tr key={event.id}>
                    <td className="px-6 py-3 text-sm text-gray-500">
                      {new Date(event.occurred_at).toLocaleString()}
                    </td>
                    <td className="px-6 py-3">
                      <span className={`inline-flex px-2 py-0.5 rounded-full text-xs font-medium ${
                        EVENT_TYPE_COLORS[event.event_type] || 'bg-gray-100 text-gray-800'
                      }`}>
                        {event.event_type}
                      </span>
                    </td>
                    <td className="px-6 py-3 text-sm">
                      {event.device_id || <span className="text-gray-400">—</span>}
                    </td>
                    <td className="px-6 py-3 text-sm text-gray-600 font-mono">
                      {event.payload ? (
                        <details>
                          <summary className="cursor-pointer text-blue-600">Show</summary>
                          <pre className="mt-1 text-xs bg-gray-50 p-2 rounded overflow-x-auto">
                            {event.payload}
                          </pre>
                        </details>
                      ) : (
                        <span className="text-gray-400">—</span>
                      )}
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          )}
        </div>
      </div>

      {/* Pagination */}
      {events.length >= filters.limit && (
        <div className="text-center py-4">
          <button
            onClick={loadMore}
            disabled={loading}
            className="px-6 py-2 bg-blue-600 text-white rounded-md hover:bg-blue-700 disabled:bg-gray-300"
          >
            {loading ? 'Loading…' : 'Load More'}
          </button>
        </div>
      )}
    </div>
  )
}

export default Events