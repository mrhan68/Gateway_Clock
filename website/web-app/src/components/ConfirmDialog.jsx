import React from 'react'

/**
 * Simple confirmation dialog.
 * Usage: <ConfirmDialog message="..." onConfirm={...} onCancel={...} isOpen={...} />
 */
function ConfirmDialog({ isOpen, message, confirmText = 'Delete', onConfirm, onCancel }) {
  if (!isOpen) return null

  return (
    <div className="fixed inset-0 bg-black/40 flex items-center justify-center z-50">
      <div className="bg-white rounded-lg p-6 w-80">
        <h2 className="text-lg font-semibold mb-3">Confirm</h2>
        <p className="text-sm text-gray-600 mb-4">{message}</p>
        <div className="flex gap-3">
          <button
            onClick={onConfirm}
            className="flex-1 px-4 py-2 bg-red-600 text-white rounded-md text-sm hover:bg-red-700"
          >
            {confirmText}
          </button>
          <button
            onClick={onCancel}
            className="flex-1 px-4 py-2 bg-gray-200 text-gray-800 rounded-md text-sm hover:bg-gray-300"
          >
            Cancel
          </button>
        </div>
      </div>
    </div>
  )
}

export default ConfirmDialog
