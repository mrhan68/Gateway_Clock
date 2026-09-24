import React from 'react'

/**
 * Modern dark-mode confirmation dialog.
 * Usage: <ConfirmDialog message="..." onConfirm={...} onCancel={...} isOpen={...} />
 */
function ConfirmDialog({ isOpen, message, confirmText = 'Delete', onConfirm, onCancel }) {
  if (!isOpen) return null

  return (
    <div className="fixed inset-0 bg-black/70 backdrop-blur-sm flex items-center justify-center z-50 p-4">
      <div className="bg-dark-900 border border-zinc-800 rounded-xl p-6 w-full max-w-sm shadow-2xl text-zinc-100 animate-in fade-in zoom-in-95 duration-150">
        <h2 className="text-base font-semibold text-zinc-100 mb-2">Xác nhận thao tác</h2>
        <p className="text-xs text-zinc-400 mb-5 leading-relaxed">{message}</p>
        <div className="flex gap-2.5">
          <button
            onClick={onCancel}
            className="flex-1 px-4 py-2 bg-zinc-800 hover:bg-zinc-750 text-zinc-300 rounded-lg text-xs font-medium transition-colors border border-zinc-700"
          >
            Hủy bỏ
          </button>
          <button
            onClick={onConfirm}
            className="flex-1 px-4 py-2 bg-rose-600 hover:bg-rose-500 text-white rounded-lg text-xs font-semibold transition-colors shadow-sm focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-rose-500"
          >
            {confirmText}
          </button>
        </div>
      </div>
    </div>
  )
}

export default ConfirmDialog
