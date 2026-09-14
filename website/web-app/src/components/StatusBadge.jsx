import React from 'react'

/**
 * Status badge component for device/gateway status.
 * @param {'online'|'offline'|'unknown'} status
 */
function StatusBadge({ status }) {
  const config = {
    online: { label: 'Online', className: 'bg-green-100 text-green-800' },
    offline: { label: 'Offline', className: 'bg-red-100 text-red-800' },
    unknown: { label: 'Unknown', className: 'bg-gray-100 text-gray-600' },
  }
  const cfg = config[status] || config.unknown

  return (
    <span className={`inline-flex items-center px-2.5 py-0.5 rounded-full text-xs font-medium ${cfg.className}`}>
      <span className="w-1.5 h-1.5 rounded-full bg-current mr-1.5"></span>
      {cfg.label}
    </span>
  )
}

export default StatusBadge
