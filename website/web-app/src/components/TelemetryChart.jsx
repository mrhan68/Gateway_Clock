import React from 'react'

/**
 * TelemetryChart component renders an SVG time-series line chart for sensor telemetry.
 * Supports Temperature (°C) and Humidity (%) with min, max, and current value badges.
 *
 * @param {Array} data - Array of telemetry records [{ value, unit, recorded_at }]
 * @param {string} title - Chart title (e.g. 'Temperature Trend')
 * @param {string} unit - Value unit (e.g. '°C' or '%')
 * @param {string} color - Primary stroke color ('blue' | 'purple' | 'emerald' | 'amber')
 * @param {boolean} loading - Loading indicator
 */
export default function TelemetryChart({
  data = [],
  title = 'Sensor Telemetry',
  unit = '°C',
  color = 'blue',
  loading = false,
  height = 180,
}) {
  // Sort data ascending by recorded_at for chronological plotting
  const sorted = [...data]
    .filter((d) => d && typeof d.value === 'number' && !isNaN(d.value))
    .sort((a, b) => new Date(a.recorded_at) - new Date(b.recorded_at))

  if (loading) {
    return (
      <div className="bg-white rounded-lg border border-gray-100 p-4 shadow-sm">
        <div className="animate-pulse space-y-3">
          <div className="h-4 bg-gray-200 rounded w-1/3"></div>
          <div className="h-32 bg-gray-100 rounded"></div>
        </div>
      </div>
    )
  }

  if (sorted.length === 0) {
    return (
      <div className="bg-white rounded-lg border border-gray-100 p-6 shadow-sm text-center">
        <h4 className="text-sm font-semibold text-gray-700">{title}</h4>
        <p className="text-xs text-gray-400 mt-2">No telemetry points recorded yet.</p>
      </div>
    )
  }

  const values = sorted.map((d) => d.value)
  const minVal = Math.min(...values)
  const maxVal = Math.max(...values)
  const currentVal = values[values.length - 1]
  const valRange = maxVal - minVal === 0 ? 1 : maxVal - minVal

  // Dimensions & padding for SVG coordinate space
  const svgWidth = 500
  const svgHeight = height
  const padX = 40
  const padY = 25
  const plotWidth = svgWidth - padX * 2
  const plotHeight = svgHeight - padY * 2

  const getX = (idx) => {
    if (sorted.length === 1) return padX + plotWidth / 2
    return padX + (idx / (sorted.length - 1)) * plotWidth
  }

  const getY = (val) => {
    const norm = (val - minVal) / valRange
    return padY + plotHeight - norm * plotHeight
  }

  // Generate SVG path points
  const points = sorted.map((d, i) => `${getX(i)},${getY(d.value)}`)
  const pathD = points.length > 0 ? `M ${points.join(' L ')}` : ''

  // Generate Area under curve
  const areaD =
    points.length > 0
      ? `M ${getX(0)},${padY + plotHeight} L ${points.join(' L ')} L ${getX(
          sorted.length - 1
        )},${padY + plotHeight} Z`
      : ''

  // Color schemes
  const colorMap = {
    blue: {
      stroke: '#3b82f6',
      fill: 'rgba(59, 130, 246, 0.12)',
      badge: 'bg-blue-50 text-blue-700 border-blue-200',
    },
    purple: {
      stroke: '#8b5cf6',
      fill: 'rgba(139, 92, 246, 0.12)',
      badge: 'bg-purple-50 text-purple-700 border-purple-200',
    },
    emerald: {
      stroke: '#10b981',
      fill: 'rgba(16, 185, 129, 0.12)',
      badge: 'bg-emerald-50 text-emerald-700 border-emerald-200',
    },
    amber: {
      stroke: '#f59e0b',
      fill: 'rgba(245, 158, 11, 0.12)',
      badge: 'bg-amber-50 text-amber-700 border-amber-200',
    },
  }
  const theme = colorMap[color] || colorMap.blue

  // Timestamps for labels (first, middle, last)
  const firstTime = new Date(sorted[0].recorded_at).toLocaleTimeString([], {
    hour: '2-digit',
    minute: '2-digit',
  })
  const lastTime = new Date(sorted[sorted.length - 1].recorded_at).toLocaleTimeString([], {
    hour: '2-digit',
    minute: '2-digit',
  })

  return (
    <div className="bg-white rounded-lg border border-gray-100 p-4 shadow-sm hover:shadow-md transition-shadow">
      {/* Header with Title & Stats */}
      <div className="flex items-center justify-between mb-3">
        <div>
          <h4 className="text-sm font-semibold text-gray-800">{title}</h4>
          <span className="text-xs text-gray-400">
            {sorted.length} reading{sorted.length > 1 ? 's' : ''} recorded
          </span>
        </div>
        <div className="flex items-center gap-2">
          <span
            className={`px-2 py-0.5 rounded border text-xs font-semibold ${theme.badge}`}
          >
            Current: {currentVal} {unit}
          </span>
        </div>
      </div>

      {/* Mini KPI Bar */}
      <div className="grid grid-cols-3 gap-2 text-center text-xs py-1 px-2 mb-2 bg-gray-50 rounded">
        <div>
          <span className="text-gray-400 block">Min</span>
          <span className="font-semibold text-gray-700">
            {minVal} {unit}
          </span>
        </div>
        <div>
          <span className="text-gray-400 block">Avg</span>
          <span className="font-semibold text-gray-700">
            {(values.reduce((a, b) => a + b, 0) / values.length).toFixed(1)} {unit}
          </span>
        </div>
        <div>
          <span className="text-gray-400 block">Max</span>
          <span className="font-semibold text-gray-700">
            {maxVal} {unit}
          </span>
        </div>
      </div>

      {/* SVG Chart */}
      <div className="w-full overflow-hidden">
        <svg
          viewBox={`0 0 ${svgWidth} ${svgHeight}`}
          className="w-full h-auto select-none"
        >
          {/* Horizontal Grid lines */}
          <line
            x1={padX}
            y1={padY}
            x2={svgWidth - padX}
            y2={padY}
            stroke="#f3f4f6"
            strokeDasharray="4 4"
          />
          <line
            x1={padX}
            y1={padY + plotHeight / 2}
            x2={svgWidth - padX}
            y2={padY + plotHeight / 2}
            stroke="#f3f4f6"
            strokeDasharray="4 4"
          />
          <line
            x1={padX}
            y1={padY + plotHeight}
            x2={svgWidth - padX}
            y2={padY + plotHeight}
            stroke="#e5e7eb"
          />

          {/* Area Fill */}
          <path d={areaD} fill={theme.fill} />

          {/* Line Path */}
          <path
            d={pathD}
            fill="none"
            stroke={theme.stroke}
            strokeWidth="2.5"
            strokeLinecap="round"
            strokeLinejoin="round"
          />

          {/* Data Points */}
          {sorted.map((d, i) => (
            <circle
              key={i}
              cx={getX(i)}
              cy={getY(d.value)}
              r="3.5"
              fill="#ffffff"
              stroke={theme.stroke}
              strokeWidth="2"
            >
              <title>{`${d.value} ${unit} at ${new Date(d.recorded_at).toLocaleTimeString()}`}</title>
            </circle>
          ))}

          {/* Y Axis min / max labels */}
          <text
            x={padX - 8}
            y={padY + 4}
            textAnchor="end"
            fontSize="10"
            fill="#9ca3af"
          >
            {maxVal}
          </text>
          <text
            x={padX - 8}
            y={padY + plotHeight + 3}
            textAnchor="end"
            fontSize="10"
            fill="#9ca3af"
          >
            {minVal}
          </text>

          {/* X Axis time labels */}
          <text
            x={padX}
            y={svgHeight - 6}
            textAnchor="start"
            fontSize="10"
            fill="#9ca3af"
          >
            {firstTime}
          </text>
          <text
            x={svgWidth - padX}
            y={svgHeight - 6}
            textAnchor="end"
            fontSize="10"
            fill="#9ca3af"
          >
            {lastTime}
          </text>
        </svg>
      </div>
    </div>
  )
}
