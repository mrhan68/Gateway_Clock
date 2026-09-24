import React from 'react'
import { ExclamationTriangleIcon, ArrowPathIcon } from '@heroicons/react/24/outline'

class ErrorBoundary extends React.Component {
  constructor(props) {
    super(props)
    this.state = { hasError: false, error: null }
  }

  static getDerivedStateFromError(error) {
    return { hasError: true, error }
  }

  componentDidCatch(error, info) {
    console.error('Error boundary caught:', error, info)
  }

  render() {
    if (this.state.hasError) {
      return (
        <div className="min-h-[50vh] flex items-center justify-center p-6">
          <div className="bg-dark-900 border border-zinc-800 rounded-2xl p-8 max-w-md w-full text-center shadow-2xl">
            <div className="w-12 h-12 rounded-xl bg-rose-500/10 border border-rose-500/20 text-rose-400 flex items-center justify-center mx-auto mb-4">
              <ExclamationTriangleIcon className="w-6 h-6" />
            </div>
            <h1 className="text-base font-semibold text-zinc-100 mb-2">Đã xảy ra sự cố</h1>
            <p className="text-xs text-zinc-400 mb-6 font-mono leading-relaxed bg-zinc-950 p-3 rounded-lg border border-zinc-800 break-words">
              {this.state.error?.message || 'Lỗi không xác định khi hiển thị giao diện.'}
            </p>
            <button
              onClick={() => window.location.reload()}
              className="inline-flex items-center gap-2 px-4 py-2 bg-emerald-500 hover:bg-emerald-400 text-dark-950 font-semibold text-xs rounded-lg transition-colors shadow-sm focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-emerald-400"
            >
              <ArrowPathIcon className="w-4 h-4" />
              <span>Tải lại trang</span>
            </button>
          </div>
        </div>
      )
    }
    return this.props.children
  }
}

export default ErrorBoundary
