import React, { useEffect } from 'react'
import {
  CheckCircleIcon,
  ExclamationCircleIcon,
  InformationCircleIcon,
  XMarkIcon,
} from '@heroicons/react/24/outline'

export default function Toast({ message, type = 'success', onClose, duration = 4000 }) {
  useEffect(() => {
    if (!message || !duration) return
    const timer = setTimeout(() => {
      onClose?.()
    }, duration)
    return () => clearTimeout(timer)
  }, [message, duration, onClose])

  if (!message) return null

  const isSuccess = type === 'success'
  const isError = type === 'error'

  return (
    <div className="fixed bottom-6 right-6 z-50 max-w-sm animate-in fade-in slide-in-from-bottom-3 duration-200">
      <div
        className={`flex items-start gap-3 p-3.5 rounded-xl shadow-2xl border backdrop-blur-md ${
          isSuccess
            ? 'bg-dark-900/95 border-emerald-500/30 text-emerald-300 shadow-[0_0_20px_rgba(16,185,129,0.15)]'
            : isError
            ? 'bg-dark-900/95 border-rose-500/30 text-rose-300 shadow-[0_0_20px_rgba(244,63,94,0.15)]'
            : 'bg-dark-900/95 border-sky-500/30 text-sky-300 shadow-[0_0_20px_rgba(14,165,233,0.15)]'
        }`}
      >
        <div className="flex-shrink-0 mt-0.5">
          {isSuccess && <CheckCircleIcon className="w-5 h-5 text-emerald-400" />}
          {isError && <ExclamationCircleIcon className="w-5 h-5 text-rose-400" />}
          {!isSuccess && !isError && <InformationCircleIcon className="w-5 h-5 text-sky-400" />}
        </div>
        <div className="flex-1 text-xs leading-relaxed text-zinc-200 font-medium">
          {message}
        </div>
        <button
          onClick={onClose}
          className="flex-shrink-0 text-zinc-400 hover:text-zinc-200 p-0.5 rounded transition-colors focus-visible:outline-none focus-visible:ring-1 focus-visible:ring-zinc-400"
          aria-label="Đóng thông báo"
        >
          <XMarkIcon className="w-4 h-4" />
        </button>
      </div>
    </div>
  )
}
