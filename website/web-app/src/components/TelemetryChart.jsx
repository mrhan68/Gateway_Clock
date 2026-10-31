import React from 'react'

/**
 * TelemetryChart component renders an SVG time-series line chart for sensor telemetry.
 * Supports Temperature (°C) and Humidity (%) with min, max, and current value badges.
 * Formatted for Linear-style dark aesthetic.
 *
 * @param {Array} data - Array of telemetry records [{ value, unit, recorded_at }]
 * @param {string} title - Chart title (e.g. 'Temperature Trend')
 * @param {string} unit - Value unit (e.g. '°C' or '%')
 * @param {string} color - Primary stroke color ('blue' | 'indigo' | 'emerald' | 'amber')
 * @param {boolean} loading - Loading indicator
 */
export default function TelemetryChart({
  data = [],
  title = 'Sensor Telemetry',
  unit = '°C',
  color = 'emerald',
  loading = false,
  height = 190,
}) {
  // Sort data ascending by recorded_at for chronological plotting
  const sorted = [...data]
    .filter((d) => d && typeof d.value === 'number' && !isNaN(d.value))
    .sort((a, b) => new Date(a.recorded_at) - new Date(b.recorded_at))

  if (loading) {
    return (
      <div className="bg-dark-900 rounded-xl border border-zinc-800 p-5 shadow-sm">
        <div className="animate-pulse space-y-4">
          <div className="h-4 bg-zinc-800 rounded w-1/3"></div>
          <div className="h-32 bg-zinc-950/60 rounded"></div>
        </div>
      </div>
    )
  }

  if (sorted.length === 0) {
    return (
      <div className="bg-dark-900 rounded-xl border border-zinc-800 p-6 shadow-sm text-center">
        <h4 className="text-sm font-semibold text-zinc-300">{title}</h4>
        <p className="text-xs text-zinc-500 mt-2 font-mono">No telemetry points recorded yet.</p>
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

  // Color schemes (Linear dark theme)
  const colorMap = {
    blue: {
      stroke: '#38bdf8',
      fill: 'rgba(56, 189, 248, 0.15)',
      badge: 'bg-sky-500/10 text-sky-400 border-sky-500/20',
      dot: '#38bdf8',
    },
    cyan: {
      stroke: '#06b6d4',
      fill: 'rgba(6, 182, 212, 0.15)',
      badge: 'bg-cyan-500/10 text-cyan-400 border-cyan-500/20',
      dot: '#06b6d4',
    },
    emerald: {
      stroke: '#34d399',
      fill: 'rgba(52, 211, 153, 0.15)',
      badge: 'bg-emerald-500/10 text-emerald-400 border-emerald-500/20',
      dot: '#34d399',
    },
    amber: {
      stroke: '#fbbf24',
      fill: 'rgba(251, 191, 36, 0.15)',
      badge: 'bg-amber-500/10 text-amber-400 border-amber-500/20',
      dot: '#fbbf24',
    },
  }
  const theme = colorMap[color] || colorMap.emerald

  // Timestamps for labels
  const firstTime = new Date(sorted[0].recorded_at).toLocaleTimeString([], {
    hour: '2-digit',
    minute: '2-digit',
  })
  const lastTime = new Date(sorted[sorted.length - 1].recorded_at).toLocaleTimeString([], {
    hour: '2-digit',
    minute: '2-digit',
  })

  return (
    <div className="bg-dark-900 rounded-xl border border-zinc-800 p-5 shadow-sm hover:border-zinc-700 transition-colors">
      {/* Header with Title & Stats */}
      <div className="flex items-center justify-between mb-3">
        <div>
          <h4 className="text-sm font-semibold text-zinc-100">{title}</h4>
          <span className="text-[11px] text-zinc-500 font-mono">
            {sorted.length} reading{sorted.length > 1 ? 's' : ''} captured
          </span>
        </div>
        <div className="flex items-center gap-2">
          <span
            className={`px-2.5 py-0.5 rounded-full border text-xs font-semibold tabular-nums ${theme.badge}`}
          >
            {currentVal} {unit}
          </span>
        </div>
      </div>

      {/* Mini KPI Bar */}
      <div className="grid grid-cols-3 gap-2 text-center text-xs py-2 px-3 mb-3 bg-zinc-950/70 border border-zinc-800/80 rounded-lg">
        <div>
          <span className="text-[11px] text-zinc-500 block uppercase font-mono tracking-wider">Min</span>
          <span className="font-semibold text-zinc-200 tabular-nums">
            {minVal} {unit}
          </span>
        </div>
        <div>
          <span className="text-[11px] text-zinc-500 block uppercase font-mono tracking-wider">Avg</span>
          <span className="font-semibold text-zinc-200 tabular-nums">
            {(values.reduce((a, b) => a + b, 0) / values.length).toFixed(1)} {unit}
          </span>
        </div>
        <div>
          <span className="text-[11px] text-zinc-500 block uppercase font-mono tracking-wider">Max</span>
          <span className="font-semibold text-zinc-200 tabular-nums">
            {maxVal} {unit}
          </span>
        </div>
      </div>

      {/* SVG Time-Series Chart */}
      <div className="relative">
        <svg
          viewBox={`0 0 ${svgWidth} ${svgHeight}`}
          className="w-full h-auto overflow-visible select-none"
        >
          {/* Subtle Grid Lines */}
          <line
            x1={padX}
            y1={padY}
            x2={svgWidth - padX}
            y2={padY}
            stroke="#27272a"
            strokeDasharray="3 3"
            strokeWidth="1"
          />
          <line
            x1={padX}
            y1={padY + plotHeight / 2}
            x2={svgWidth - padX}
            y2={padY + plotHeight / 2}
            stroke="#27272a"
            strokeDasharray="3 3"
            strokeWidth="1"
          />
          <line
            x1={padX}
            y1={padY + plotHeight}
            x2={svgWidth - padX}
            y2={padY + plotHeight}
            stroke="#27272a"
            strokeWidth="1"
          />

          {/* Area Fill */}
          {areaD && <path d={areaD} fill={theme.fill} />}

          {/* Value Line */}
          {pathD && (
            <path
              d={pathD}
              fill="none"
              stroke={theme.stroke}
              strokeWidth="2.5"
              strokeLinecap="round"
              strokeLinejoin="round"
            />
          )}

          {/* Data Points */}
          {sorted.map((d, i) => (
            <circle
              key={i}
              cx={getX(i)}
              cy={getY(d.value)}
              r={i === sorted.length - 1 ? '4' : '2'}
              fill={theme.dot}
              stroke="#09090b"
              strokeWidth="1.5"
            />
          ))}

          {/* Y-Axis Value Labels */}
          <text
            x={padX - 8}
            y={padY + 4}
            textAnchor="end"
            fontSize="10"
            fill="#71717a"
            fontFamily="monospace"
          >
            {maxVal}
          </text>
          <text
            x={padX - 8}
            y={padY + plotHeight}
            textAnchor="end"
            fontSize="10"
            fill="#71717a"
            fontFamily="monospace"
          >
            {minVal}
          </text>

          {/* X-Axis Time Labels */}
          <text
            x={padX}
            y={svgHeight - 4}
            textAnchor="start"
            fontSize="10"
            fill="#71717a"
            fontFamily="monospace"
          >
            {firstTime}
          </text>
          <text
            x={svgWidth - padX}
            y={svgHeight - 4}
            textAnchor="end"
            fontSize="10"
            fill="#71717a"
            fontFamily="monospace"
          >
            {lastTime}
          </text>
        </svg>
      </div>
    </div>
  )
}
