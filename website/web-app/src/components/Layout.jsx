import React, { useState, useEffect } from 'react'
import { NavLink, useNavigate } from 'react-router-dom'
import {
  ChartBarIcon,
  CpuChipIcon,
  ClockIcon,
  AdjustmentsHorizontalIcon,
  QueueListIcon,
  Cog6ToothIcon,
  ChevronUpDownIcon,
  XMarkIcon,
  Bars3Icon,
  SparklesIcon,
  UserCircleIcon,
  ArrowRightOnRectangleIcon,
} from '@heroicons/react/24/outline'
import { getActiveGateway, setActiveGateway, healthCheck, gatewaysAPI, authAPI } from '../api/client'

const navItems = [
  { name: 'Dashboard', icon: ChartBarIcon, path: '/dashboard' },
  { name: 'Devices', icon: CpuChipIcon, path: '/devices' },
  { name: 'Schedules', icon: ClockIcon, path: '/schedules' },
  { name: 'Rules', icon: AdjustmentsHorizontalIcon, path: '/rules' },
  { name: 'Events', icon: QueueListIcon, path: '/events' },
  { name: 'Settings', icon: Cog6ToothIcon, path: '/settings' },
]

function Layout({ children }) {
  const navigate = useNavigate()
  const currentUser = authAPI.getUser()
  const [backendHealthy, setBackendHealthy] = useState(false)
  const [gateways, setGateways] = useState([])
  const [showGatewayModal, setShowGatewayModal] = useState(false)
  const [mobileMenuOpen, setMobileMenuOpen] = useState(false)

  const currentGateway = getActiveGateway()

  const handleLogout = () => {
    authAPI.logout()
    navigate('/login', { replace: true })
  }

  useEffect(() => {
    checkHealth()
    loadGateways()
    const interval = setInterval(checkHealth, 30000)
    return () => clearInterval(interval)
  }, [])

  const checkHealth = async () => {
    try {
      await healthCheck()
      setBackendHealthy(true)
    } catch {
      setBackendHealthy(false)
    }
  }

  const loadGateways = async () => {
    try {
      const res = await gatewaysAPI.list()
      const gwList = res.data || []
      setGateways(gwList)
      if (gwList.length > 0 && !currentGateway) {
        setActiveGateway(gwList[0].id)
        window.location.reload()
      }
    } catch (err) {
      console.error('Failed to load gateways:', err)
    }
  }

  const handleGatewaySelect = (gatewayId) => {
    setActiveGateway(gatewayId)
    setShowGatewayModal(false)
    window.location.reload()
  }

  const [modalTab, setModalTab] = useState('claim') // 'claim' | 'direct'
  const [claimLoading, setClaimLoading] = useState(false)
  const [claimError, setClaimError] = useState('')

  const handleClaimGateway = async (e) => {
    e.preventDefault()
    setClaimError('')
    const claimCode = e.target.claim_code.value.trim().toUpperCase()
    const name = e.target.gateway_name.value.trim()
    if (!claimCode || !name) return
    setClaimLoading(true)
    try {
      const res = await gatewaysAPI.claim({ claim_code: claimCode, name })
      const newGwId = res.data?.gateway_id
      setShowGatewayModal(false)
      if (newGwId) {
        setActiveGateway(newGwId)
      }
      e.target.reset()
      await loadGateways()
      window.location.reload()
    } catch (err) {
      setClaimError(err.message || 'Failed to claim gateway. Verify the 6-character code from OLED.')
    } finally {
      setClaimLoading(false)
    }
  }

  const handleAddGateway = async (e) => {
    e.preventDefault()
    setClaimError('')
    const id = e.target.gateway_id.value.trim()
    const name = e.target.gateway_name.value.trim()
    if (!id || !name) return
    setClaimLoading(true)
    try {
      await gatewaysAPI.create({ id, name })
      setShowGatewayModal(false)
      setActiveGateway(id)
      e.target.reset()
      await loadGateways()
      window.location.reload()
    } catch (err) {
      setClaimError(err.message || 'Failed to create gateway')
    } finally {
      setClaimLoading(false)
    }
  }

  return (
    <div className="flex h-screen bg-dark-950 text-zinc-100 overflow-hidden font-sans">
      {/* Mobile Menu Backdrop */}
      {mobileMenuOpen && (
        <div
          className="fixed inset-0 bg-black/70 backdrop-blur-sm z-40 md:hidden"
          onClick={() => setMobileMenuOpen(false)}
        />
      )}

      {/* Sidebar Navigation */}
      <aside
        className={`fixed md:static inset-y-0 left-0 z-50 w-64 bg-dark-900 border-r border-zinc-800 flex flex-col transform transition-transform duration-200 ease-in-out md:translate-x-0 ${
          mobileMenuOpen ? 'translate-x-0' : '-translate-x-full'
        }`}
      >
        {/* App Brand Header */}
        <div className="p-4 border-b border-zinc-800">
          <div className="flex items-center justify-between">
            <div className="flex items-center gap-2.5">
              <div className="w-8 h-8 rounded-lg bg-emerald-500/10 border border-emerald-500/30 flex items-center justify-center text-emerald-400">
                <SparklesIcon className="w-4 h-4" />
              </div>
              <div>
                <h1 className="text-sm font-semibold tracking-tight text-zinc-100">
                  Smart Clock
                </h1>
                <p className="text-[11px] text-zinc-500 font-mono tracking-wider uppercase">
                  Edge Gateway
                </p>
              </div>
            </div>
            <button
              onClick={() => setMobileMenuOpen(false)}
              className="md:hidden text-zinc-400 hover:text-zinc-200 p-1"
              aria-label="Close menu"
            >
              <XMarkIcon className="w-5 h-5" />
            </button>
          </div>

          {/* Gateway Selector Widget */}
          <div className="mt-4">
            <button
              onClick={() => setShowGatewayModal(true)}
              className="w-full px-3 py-2 text-xs bg-zinc-950/80 hover:bg-zinc-850 border border-zinc-800 rounded-lg flex items-center justify-between transition-colors focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-emerald-500/50"
              aria-label="Select Gateway"
            >
              <div className="flex items-center gap-2 truncate">
                <span className="w-2 h-2 rounded-full bg-emerald-400 shadow-[0_0_6px_rgba(52,211,153,0.8)]" />
                <span className="truncate font-medium text-zinc-300">
                  {currentGateway || 'Select Gateway'}
                </span>
              </div>
              <ChevronUpDownIcon className="w-4 h-4 text-zinc-500 flex-shrink-0" />
            </button>
          </div>
        </div>

        {/* Navigation Items */}
        <nav className="flex-1 p-3 space-y-1 overflow-y-auto">
          {navItems.map((item) => {
            const Icon = item.icon
            return (
              <NavLink
                key={item.path}
                to={item.path}
                onClick={() => setMobileMenuOpen(false)}
                className={({ isActive }) =>
                  `flex items-center gap-3 px-3 py-2 rounded-lg text-xs font-medium transition-all ${
                    isActive
                      ? 'bg-zinc-800/90 text-emerald-400 border border-zinc-700/60 shadow-sm'
                      : 'text-zinc-400 hover:text-zinc-200 hover:bg-zinc-800/40'
                  } focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-emerald-500/50`
                }
              >
                <Icon className="w-4 h-4 flex-shrink-0" />
                <span>{item.name}</span>
              </NavLink>
            )
          })}
        </nav>

        {/* Sidebar Footer: User Account & Health Status */}
        <div className="p-3 border-t border-zinc-800/80 bg-zinc-950/40 space-y-2">
          {currentUser && (
            <div className="flex items-center justify-between px-1 text-xs">
              <div className="flex items-center gap-2 truncate">
                <UserCircleIcon className="w-4 h-4 text-emerald-400 flex-shrink-0" />
                <span className="truncate font-medium text-zinc-300">
                  {currentUser.username || 'Admin'}
                </span>
              </div>
              <button
                onClick={handleLogout}
                className="text-zinc-500 hover:text-rose-400 transition-colors p-1 rounded"
                title="Đăng xuất"
                aria-label="Đăng xuất"
              >
                <ArrowRightOnRectangleIcon className="w-4 h-4" />
              </button>
            </div>
          )}

          <div className="flex items-center justify-between text-[11px] px-1 pt-1 border-t border-zinc-800/40">
            <div className="flex items-center gap-2">
              <span
                className={`w-2 h-2 rounded-full ${
                  backendHealthy
                    ? 'bg-emerald-400 shadow-[0_0_8px_rgba(52,211,153,0.8)]'
                    : 'bg-rose-500 shadow-[0_0_8px_rgba(244,63,94,0.8)]'
                }`}
              />
              <span className="text-zinc-400">
                {backendHealthy ? 'Cloud Server OK' : 'Server Disconnected'}
              </span>
            </div>
            <span className="text-zinc-600 font-mono text-[10px]">v1.6</span>
          </div>
        </div>
      </aside>

      {/* Main Content Area */}
      <div className="flex-1 flex flex-col min-w-0 overflow-hidden">
        {/* Top bar on Mobile */}
        <header className="md:hidden flex items-center justify-between px-4 py-3 border-b border-zinc-800 bg-dark-900">
          <div className="flex items-center gap-2">
            <button
              onClick={() => setMobileMenuOpen(true)}
              className="p-1.5 rounded-md text-zinc-400 hover:text-zinc-200 hover:bg-zinc-800"
              aria-label="Open navigation"
            >
              <Bars3Icon className="w-5 h-5" />
            </button>
            <span className="text-sm font-semibold text-zinc-100">Smart Clock</span>
          </div>
          <div className="flex items-center gap-2">
            <button
              onClick={() => setShowGatewayModal(true)}
              className="text-xs px-2.5 py-1 rounded bg-zinc-800 border border-zinc-700 text-zinc-300 font-mono"
            >
              {currentGateway ? currentGateway.slice(0, 10) : 'No GW'}
            </button>
            {currentUser && (
              <button
                onClick={handleLogout}
                className="p-1 text-zinc-400 hover:text-rose-400 transition-colors"
                title="Đăng xuất"
                aria-label="Đăng xuất"
              >
                <ArrowRightOnRectangleIcon className="w-4 h-4" />
              </button>
            )}
          </div>
        </header>

        {/* Backend disconnected warning banner */}
        {!backendHealthy && (
          <div className="bg-amber-500/10 border-b border-amber-500/20 px-4 py-2 flex flex-wrap items-center justify-between gap-2 text-xs text-amber-300">
            <div className="flex items-center gap-2">
              <span className="w-2 h-2 rounded-full bg-amber-500 animate-pulse flex-shrink-0" />
              <span>
                Chưa kết nối Backend API. Vui lòng thiết lập URL Render của bạn.
              </span>
            </div>
            <button
              onClick={() => {
                const current = localStorage.getItem('apiBaseUrl') || 'https://smartclock-backend.onrender.com'
                const url = window.prompt('Nhập URL Backend Render của bạn (vd: https://smartclock-backend.onrender.com):', current)
                if (url !== null) {
                  localStorage.setItem('apiBaseUrl', url.trim().replace(/\/+$/, ''))
                  window.location.reload()
                }
              }}
              className="px-2.5 py-1 bg-amber-500/20 hover:bg-amber-500/30 rounded border border-amber-500/40 text-[11px] font-medium transition-colors"
            >
              Cấu hình Backend URL
            </button>
          </div>
        )}

        {/* Dynamic Page Content */}
        <main className="flex-1 overflow-y-auto p-4 sm:p-6 lg:p-8 bg-dark-950">
          {children}
        </main>
      </div>

      {/* Gateway Management Modal */}
      {showGatewayModal && (
        <div className="fixed inset-0 bg-black/70 backdrop-blur-sm flex items-center justify-center z-50 p-4">
          <div className="bg-dark-900 border border-zinc-800 rounded-xl p-6 w-full max-w-md shadow-2xl text-zinc-100 animate-in fade-in zoom-in-95 duration-150">
            <div className="flex items-center justify-between pb-3 border-b border-zinc-800">
              <div>
                <h2 className="text-base font-semibold text-zinc-100">Gateway Manager</h2>
                <p className="text-xs text-zinc-400 mt-0.5">Pair or select your IoT edge hub</p>
              </div>
              <button
                onClick={() => setShowGatewayModal(false)}
                className="text-zinc-500 hover:text-zinc-300 p-1 rounded-md transition-colors"
                aria-label="Close dialog"
              >
                <XMarkIcon className="w-5 h-5" />
              </button>
            </div>

            {/* Gateway list */}
            <div className="mt-4">
              <h3 className="text-[11px] font-semibold text-zinc-400 uppercase tracking-wider mb-2">
                Available Gateways
              </h3>
              {gateways.length > 0 ? (
                <div className="space-y-1.5 max-h-40 overflow-y-auto pr-1">
                  {gateways.map((gw) => (
                    <button
                      key={gw.id}
                      onClick={() => handleGatewaySelect(gw.id)}
                      className={`w-full text-left p-2.5 rounded-lg border transition-all flex items-center justify-between ${
                        gw.id === currentGateway
                          ? 'border-emerald-500/50 bg-emerald-500/10 text-emerald-300'
                          : 'border-zinc-800/80 bg-zinc-950/60 hover:bg-zinc-800/60 text-zinc-300'
                      }`}
                    >
                      <div className="truncate">
                        <div className="font-medium text-xs text-zinc-200">{gw.name}</div>
                        <div className="text-[11px] text-zinc-500 font-mono">{gw.id}</div>
                      </div>
                      <div className="flex items-center gap-2 flex-shrink-0">
                        <span
                          className={`text-[10px] px-2 py-0.5 rounded-full font-medium border ${
                            gw.status === 'online'
                              ? 'bg-emerald-500/10 text-emerald-400 border-emerald-500/30'
                              : 'bg-rose-500/10 text-rose-400 border-rose-500/30'
                          }`}
                        >
                          {gw.status === 'online' ? 'Online' : 'Offline'}
                        </span>
                        {gw.id === currentGateway && (
                          <span className="text-[11px] text-emerald-400 font-semibold">Active</span>
                        )}
                      </div>
                    </button>
                  ))}
                </div>
              ) : (
                <p className="text-xs text-zinc-500 italic mb-2">No gateways paired yet.</p>
              )}
            </div>

            {/* Error Message */}
            {claimError && (
              <div className="mt-3 p-2.5 text-xs bg-rose-500/10 text-rose-400 border border-rose-500/20 rounded-lg">
                {claimError}
              </div>
            )}

            {/* Tabs for Adding / Claiming */}
            <div className="mt-5 border-t border-zinc-800 pt-4">
              <div className="flex border-b border-zinc-800 mb-3 gap-4">
                <button
                  type="button"
                  onClick={() => setModalTab('claim')}
                  className={`pb-2 text-xs font-medium transition-colors border-b-2 ${
                    modalTab === 'claim'
                      ? 'border-emerald-400 text-emerald-400'
                      : 'border-transparent text-zinc-400 hover:text-zinc-200'
                  }`}
                >
                  Claim via Code (OLED)
                </button>
                <button
                  type="button"
                  onClick={() => setModalTab('direct')}
                  className={`pb-2 text-xs font-medium transition-colors border-b-2 ${
                    modalTab === 'direct'
                      ? 'border-emerald-400 text-emerald-400'
                      : 'border-transparent text-zinc-400 hover:text-zinc-200'
                  }`}
                >
                  Direct Register
                </button>
              </div>

              {modalTab === 'claim' ? (
                /* FSD §8.2 Claim Form */
                <form onSubmit={handleClaimGateway} className="space-y-3">
                  <div className="bg-zinc-950 border border-zinc-800 rounded-lg p-2.5 text-[11px] text-zinc-400">
                    <span className="text-emerald-400 font-semibold">💡 OLED Handshake:</span> Read the 6-character code from Screen 3 on your Clock (e.g.{' '}
                    <span className="font-mono text-zinc-200 font-bold">GW-9821A0</span>).
                  </div>
                  <div>
                    <label className="block text-xs font-medium text-zinc-400 mb-1">
                      Claim Code
                    </label>
                    <input
                      name="claim_code"
                      type="text"
                      placeholder="e.g. GW-9821A0"
                      spellCheck={false}
                      className="w-full px-3 py-2 bg-zinc-950 border border-zinc-800 rounded-lg text-xs uppercase tracking-wider font-mono text-zinc-100 focus:outline-none focus:ring-1 focus:ring-emerald-500"
                      required
                    />
                  </div>
                  <div>
                    <label className="block text-xs font-medium text-zinc-400 mb-1">
                      Gateway Friendly Name
                    </label>
                    <input
                      name="gateway_name"
                      type="text"
                      placeholder="Living Room Clock"
                      className="w-full px-3 py-2 bg-zinc-950 border border-zinc-800 rounded-lg text-xs text-zinc-100 focus:outline-none focus:ring-1 focus:ring-emerald-500"
                      required
                    />
                  </div>
                  <button
                    type="submit"
                    disabled={claimLoading}
                    className="w-full py-2 bg-emerald-600 hover:bg-emerald-500 text-zinc-950 font-semibold rounded-lg text-xs disabled:opacity-50 transition-colors shadow-sm"
                  >
                    {claimLoading ? 'Claiming Gateway…' : 'Pair Gateway'}
                  </button>
                </form>
              ) : (
                /* Direct Register Form */
                <form onSubmit={handleAddGateway} className="space-y-3">
                  <div>
                    <label className="block text-xs font-medium text-zinc-400 mb-1">
                      Gateway ID
                    </label>
                    <input
                      name="gateway_id"
                      type="text"
                      placeholder="gateway_001"
                      spellCheck={false}
                      className="w-full px-3 py-2 bg-zinc-950 border border-zinc-800 rounded-lg text-xs font-mono text-zinc-100 focus:outline-none focus:ring-1 focus:ring-emerald-500"
                      required
                    />
                  </div>
                  <div>
                    <label className="block text-xs font-medium text-zinc-400 mb-1">
                      Friendly Name
                    </label>
                    <input
                      name="gateway_name"
                      type="text"
                      placeholder="Living Room Clock"
                      className="w-full px-3 py-2 bg-zinc-950 border border-zinc-800 rounded-lg text-xs text-zinc-100 focus:outline-none focus:ring-1 focus:ring-emerald-500"
                      required
                    />
                  </div>
                  <button
                    type="submit"
                    disabled={claimLoading}
                    className="w-full py-2 bg-zinc-800 hover:bg-zinc-700 text-zinc-100 font-semibold rounded-lg text-xs disabled:opacity-50 transition-colors border border-zinc-700"
                  >
                    {claimLoading ? 'Registering…' : 'Register Gateway'}
                  </button>
                </form>
              )}
            </div>
          </div>
        </div>
      )}
    </div>
  )
}

export default Layout
