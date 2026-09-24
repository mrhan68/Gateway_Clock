import React from 'react'

/**
 * Status badge component for device/gateway status with dark theme glow.
 * @param {'online'|'offline'|'unknown'} status
 */
function StatusBadge({ status }) {
  const config = {
    online: {
      label: 'Online',
      badgeClass: 'bg-emerald-500/10 text-emerald-400 border-emerald-500/20',
      dotClass: 'bg-emerald-400 shadow-[0_0_8px_rgba(52,211,153,0.8)]',
    },
    offline: {
      label: 'Offline',
      badgeClass: 'bg-rose-500/10 text-rose-400 border-rose-500/20',
      dotClass: 'bg-rose-400 shadow-[0_0_8px_rgba(251,113,133,0.6)]',
    },
    unknown: {
      label: 'Unknown',
      badgeClass: 'bg-zinc-800/80 text-zinc-400 border-zinc-700/50',
      dotClass: 'bg-zinc-500',
    },
  }
  const cfg = config[status] || config.unknown

  return (
    <span
      className={`inline-flex items-center gap-1.5 px-2.5 py-0.5 rounded-full text-xs font-medium border ${cfg.badgeClass}`}
    >
      <span className={`w-1.5 h-1.5 rounded-full ${cfg.dotClass}`}></span>
      {cfg.label}
    </span>
  )
}

export default StatusBadge
