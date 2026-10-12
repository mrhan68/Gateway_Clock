import React, { useState } from 'react'
import { useNavigate, useLocation } from 'react-router-dom'
import {
  LockClosedIcon,
  UserIcon,
  KeyIcon,
  EnvelopeIcon,
  CpuChipIcon,
  ArrowRightIcon,
  Cog6ToothIcon,
  ShieldCheckIcon,
} from '@heroicons/react/24/outline'
import { authAPI, getBaseURL } from '../api/client'
import Toast from '../components/Toast'

export default function Login() {
  const navigate = useNavigate()
  const location = useLocation()
  const [isRegister, setIsRegister] = useState(false)
  const [loading, setLoading] = useState(false)
  const [notification, setNotification] = useState(null)
  const [showConfig, setShowConfig] = useState(false)

  const [backendUrl, setBackendUrl] = useState(
    localStorage.getItem('apiBaseUrl') || 'https://smartclock-backend.onrender.com'
  )

  const [form, setForm] = useState({
    username: '',
    password: '',
    email: '',
  })

  const handleSaveBackendUrl = (e) => {
    e.preventDefault()
    const cleanUrl = backendUrl.trim().replace(/\/+$/, '')
    localStorage.setItem('apiBaseUrl', cleanUrl)
    setNotification({ type: 'success', message: 'Đã cập nhật Backend API URL!' })
    setShowConfig(false)
  }

  const handleChange = (e) => {
    setForm({ ...form, [e.target.name]: e.target.value })
  }

  const handleSubmit = async (e) => {
    e.preventDefault()
    setLoading(true)
    setNotification(null)

    try {
      if (isRegister) {
        await authAPI.register(form.username, form.password, form.email)
        setNotification({ type: 'success', message: 'Tạo tài khoản thành công! Đang chuyển hướng...' })
      } else {
        await authAPI.login(form.username, form.password)
        setNotification({ type: 'success', message: 'Đăng nhập thành công! Đang vào hệ thống...' })
      }

      setTimeout(() => {
        const dest = location.state?.from?.pathname || '/dashboard'
        navigate(dest, { replace: true })
      }, 500)
    } catch (err) {
      setNotification({
        type: 'error',
        message: err.message || (isRegister ? 'Đăng ký thất bại' : 'Đăng nhập thất bại. Vui lòng kiểm tra lại tài khoản.'),
      })
    } finally {
      setLoading(false)
    }
  }

  return (
    <div className="min-h-screen bg-zinc-950 flex flex-col justify-center items-center px-4 py-8 relative selection:bg-emerald-500/20 selection:text-emerald-300">
      {/* Background ambient gradient glow */}
      <div className="absolute top-1/4 left-1/2 -translate-x-1/2 -translate-y-1/2 w-96 h-96 bg-emerald-500/5 rounded-full blur-3xl pointer-events-none" />

      {notification && (
        <Toast
          message={notification.message}
          type={notification.type}
          onClose={() => setNotification(null)}
        />
      )}

      <div className="w-full max-w-md space-y-6 relative z-10">
        {/* Brand / Logo Header */}
        <div className="text-center space-y-2">
          <div className="inline-flex p-3 rounded-2xl bg-dark-900 border border-zinc-800 shadow-inner text-emerald-400 mb-1">
            <CpuChipIcon className="w-8 h-8" />
          </div>
          <h1 className="text-2xl font-bold tracking-tight text-zinc-100">
            Smart Clock Gateway
          </h1>
          <p className="text-xs text-zinc-400">
            Bảng Điều Khiển IoT Edge & Giám Sát Cảm Biến Không Dây
          </p>
        </div>

        {/* Auth Card */}
        <div className="bg-dark-900 border border-zinc-800/90 rounded-2xl p-6 sm:p-7 shadow-xl shadow-black/40 space-y-6">
          {/* Tabs: Login / Register */}
          <div className="grid grid-cols-2 p-1 bg-zinc-950/80 border border-zinc-800/80 rounded-xl text-xs font-semibold">
            <button
              type="button"
              onClick={() => {
                setIsRegister(false)
                setNotification(null)
              }}
              className={`py-2 rounded-lg transition-all ${
                !isRegister
                  ? 'bg-zinc-800 text-emerald-400 shadow-sm'
                  : 'text-zinc-400 hover:text-zinc-200'
              }`}
            >
              Đăng Nhập
            </button>
            <button
              type="button"
              onClick={() => {
                setIsRegister(true)
                setNotification(null)
              }}
              className={`py-2 rounded-lg transition-all ${
                isRegister
                  ? 'bg-zinc-800 text-emerald-400 shadow-sm'
                  : 'text-zinc-400 hover:text-zinc-200'
              }`}
            >
              Tạo Tài Khoản
            </button>
          </div>

          <form onSubmit={handleSubmit} className="space-y-4">
            {/* Username Input */}
            <div className="space-y-1.5">
              <label className="block text-xs font-medium text-zinc-300">
                Tên đăng nhập
              </label>
              <div className="relative">
                <div className="absolute inset-y-0 left-0 pl-3 flex items-center pointer-events-none text-zinc-500">
                  <UserIcon className="w-4 h-4" />
                </div>
                <input
                  type="text"
                  name="username"
                  required
                  value={form.username}
                  onChange={handleChange}
                  placeholder="admin"
                  autoComplete="username"
                  className="w-full pl-9 pr-3 py-2.5 bg-zinc-950 border border-zinc-800 rounded-xl text-xs text-zinc-100 placeholder-zinc-500 focus:outline-none focus:border-emerald-500 focus:ring-1 focus:ring-emerald-500/40 font-mono transition-colors"
                />
              </div>
            </div>

            {/* Email Input (Optional for Register) */}
            {isRegister && (
              <div className="space-y-1.5 animate-in fade-in duration-150">
                <label className="block text-xs font-medium text-zinc-300">
                  Email (tùy chọn)
                </label>
                <div className="relative">
                  <div className="absolute inset-y-0 left-0 pl-3 flex items-center pointer-events-none text-zinc-500">
                    <EnvelopeIcon className="w-4 h-4" />
                  </div>
                  <input
                    type="email"
                    name="email"
                    value={form.email}
                    onChange={handleChange}
                    placeholder="user@example.com"
                    autoComplete="email"
                    className="w-full pl-9 pr-3 py-2.5 bg-zinc-950 border border-zinc-800 rounded-xl text-xs text-zinc-100 placeholder-zinc-500 focus:outline-none focus:border-emerald-500 focus:ring-1 focus:ring-emerald-500/40 transition-colors"
                  />
                </div>
              </div>
            )}

            {/* Password Input */}
            <div className="space-y-1.5">
              <label className="block text-xs font-medium text-zinc-300">
                Mật khẩu
              </label>
              <div className="relative">
                <div className="absolute inset-y-0 left-0 pl-3 flex items-center pointer-events-none text-zinc-500">
                  <KeyIcon className="w-4 h-4" />
                </div>
                <input
                  type="password"
                  name="password"
                  required
                  value={form.password}
                  onChange={handleChange}
                  placeholder="••••••••"
                  autoComplete={isRegister ? 'new-password' : 'current-password'}
                  className="w-full pl-9 pr-3 py-2.5 bg-zinc-950 border border-zinc-800 rounded-xl text-xs text-zinc-100 placeholder-zinc-500 focus:outline-none focus:border-emerald-500 focus:ring-1 focus:ring-emerald-500/40 font-mono transition-colors"
                />
              </div>
              {isRegister && (
                <p className="text-[11px] text-zinc-500">
                  Tối thiểu 6 ký tự. Được bảo vệ bằng mã hóa salted scrypt an toàn.
                </p>
              )}
            </div>

            {/* Submit Action */}
            <button
              type="submit"
              disabled={loading}
              className="w-full mt-2 py-2.5 px-4 bg-emerald-600 hover:bg-emerald-500 disabled:opacity-50 text-zinc-950 font-semibold rounded-xl text-xs transition-colors flex items-center justify-center gap-2 shadow-sm"
            >
              {loading ? (
                <span className="inline-block animate-pulse">Đang xử lý...</span>
              ) : (
                <>
                  <span>{isRegister ? 'Tạo Tài Khoản' : 'Đăng Nhập'}</span>
                  <ArrowRightIcon className="w-3.5 h-3.5" />
                </>
              )}
            </button>
          </form>

          {/* Backend Connection Drawer Toggle */}
          <div className="pt-2 border-t border-zinc-800/80">
            <button
              type="button"
              onClick={() => setShowConfig(!showConfig)}
              className="w-full flex items-center justify-between text-[11px] text-zinc-400 hover:text-zinc-200 transition-colors py-1"
            >
              <span className="flex items-center gap-1.5">
                <Cog6ToothIcon className="w-3.5 h-3.5" />
                <span>Cấu hình Backend API URL</span>
              </span>
              <span className="text-zinc-600 font-mono">
                {showConfig ? '▲ Đóng' : '▼ Cài đặt'}
              </span>
            </button>

            {showConfig && (
              <form onSubmit={handleSaveBackendUrl} className="mt-3 space-y-2 text-xs">
                <input
                  type="url"
                  value={backendUrl}
                  onChange={(e) => setBackendUrl(e.target.value)}
                  placeholder="https://smartclock-backend.onrender.com"
                  className="w-full px-3 py-2 bg-zinc-950 border border-zinc-800 rounded-lg text-zinc-200 text-xs font-mono focus:outline-none focus:border-emerald-500"
                />
                <button
                  type="submit"
                  className="w-full py-1.5 bg-zinc-800 hover:bg-zinc-700 text-zinc-200 rounded-lg text-[11px] font-medium transition-colors"
                >
                  Lưu Endpoint Backend
                </button>
              </form>
            )}
          </div>
        </div>

        {/* Security badge footer */}
        <div className="flex items-center justify-center gap-1.5 text-zinc-500 text-[11px]">
          <ShieldCheckIcon className="w-4 h-4 text-emerald-500/70" />
          <span>FSD FR-SEC-01 & FR-SEC-02 · Mã hóa JWT & MQTTS TLS 8883</span>
        </div>
      </div>
    </div>
  )
}
