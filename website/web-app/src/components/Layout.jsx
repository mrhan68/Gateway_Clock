import React, { useState, useEffect } from 'react'
import { NavLink, useNavigate } from 'react-router-dom'
import { getActiveGateway, setActiveGateway, healthCheck, gatewaysAPI } from '../api/client'

const navItems = [
  { name: 'Dashboard', icon: '📊', path: '/dashboard' },
  { name: 'Devices', icon: '🌡️', path: '/devices' },
  { name: 'Schedules', icon: '⏰', path: '/schedules' },
  { name: 'Rules', icon: '⚙️', path: '/rules' },
  { name: 'Events', icon: '📜', path: '/events' },
  { name: 'Settings', icon: '⚙️', path: '/settings' },
]

function Layout({ children }) {
  const [backendHealthy, setBackendHealthy] = useState(false)
  const [gateways, setGateways] = useState([])
  const [showGatewayModal, setShowGatewayModal] = useState(false)
  const navigate = useNavigate()

  const currentGateway = getActiveGateway()

  useEffect(() => {
    checkHealth()
    loadGateways()
    // Periodically recheck
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
    <div className="flex h-screen bg-gray-50 overflow-hidden">
      {/* Sidebar */}
      <aside className="w-64 bg-white shadow-md flex flex-col">
        <div className="p-4 border-b">
          <h1 className="text-xl font-bold text-gray-800">Smart Clock</h1>
          <p className="text-sm text-gray-500">Gateway Manager</p>
          {/* Gateway selector */}
          <div className="mt-3">
            <button
              onClick={() => setShowGatewayModal(true)}
              className="w-full px-3 py-2 text-sm bg-gray-100 hover:bg-gray-200 rounded-md flex items-center justify-between"
            >
              <span className="truncate">
                {currentGateway || 'Select gateway'}
              </span>
              <span>▼</span>
            </button>
            {currentGateway && (
              <button
                onClick={() => {
                  const gw = gateways.find(g => g.id === currentGateway)
                  if (gw) navigate('/settings')
                }}
                className="w-full mt-1 px-3 py-1 text-xs text-blue-600 hover:text-blue-800 text-right"
              >
                Settings →
              </button>
            )}
          </div>
        </div>
        <nav className="flex-1 p-2">
          {navItems.map((item) => (
            <NavLink
              key={item.path}
              to={item.path}
              className={({ isActive }) =>
                `flex items-center gap-3 px-3 py-2 rounded-md text-sm font-medium transition-colors ${
                  isActive
                    ? 'bg-blue-100 text-blue-700'
                    : 'text-gray-600 hover:bg-gray-100'
                }`
              }
            >
              <span className="text-lg">{item.icon}</span>
              {item.name}
            </NavLink>
          ))}
        </nav>
        <div className="p-4 border-t">
          <div className={`flex items-center gap-2 text-xs ${backendHealthy ? 'text-green-600' : 'text-red-600'}`}>
            <span className={`w-2 h-2 rounded-full ${backendHealthy ? 'bg-green-500' : 'bg-red-500'}`}></span>
            <span>{backendHealthy ? 'Backend Online' : 'Backend Offline'}</span>
          </div>
        </div>
      </aside>

      {/* Main content */}
      <main className="flex-1 overflow-y-auto p-6">
        {children}
      </main>

      {/* Gateway selection & claim modal */}
      {showGatewayModal && (
        <div className="fixed inset-0 bg-black/40 flex items-center justify-center z-50 p-4">
          <div className="bg-white rounded-lg p-6 w-full max-w-md shadow-xl">
            <div className="flex items-center justify-between pb-3 border-b">
              <h2 className="text-lg font-bold text-gray-800">Gateway Management</h2>
              <button
                onClick={() => setShowGatewayModal(false)}
                className="text-gray-400 hover:text-gray-600 text-lg font-bold"
              >
                ✕
              </button>
            </div>

            {/* Existing Gateways */}
            <div className="mt-4">
              <h3 className="text-xs font-semibold text-gray-500 uppercase tracking-wider mb-2">
                Your Gateways
              </h3>
              {gateways.length > 0 ? (
                <div className="space-y-2 max-h-40 overflow-y-auto pr-1">
                  {gateways.map((gw) => (
                    <button
                      key={gw.id}
                      onClick={() => handleGatewaySelect(gw.id)}
                      className={`w-full text-left p-2.5 border rounded-md transition-colors flex items-center justify-between ${
                        gw.id === currentGateway
                          ? 'border-blue-500 bg-blue-50/50'
                          : 'border-gray-200 hover:bg-gray-50'
                      }`}
                    >
                      <div className="truncate">
                        <div className="font-medium text-sm text-gray-800">{gw.name}</div>
                        <div className="text-xs text-gray-500 font-mono">{gw.id}</div>
                      </div>
                      <div className="flex items-center gap-2 flex-shrink-0">
                        <span
                          className={`text-xs px-2 py-0.5 rounded-full font-medium ${
                            gw.status === 'online'
                              ? 'bg-green-100 text-green-700'
                              : 'bg-red-100 text-red-700'
                          }`}
                        >
                          {gw.status === 'online' ? 'Online' : 'Offline'}
                        </span>
                        {gw.id === currentGateway && (
                          <span className="text-xs text-blue-600 font-semibold">Active</span>
                        )}
                      </div>
                    </button>
                  ))}
                </div>
              ) : (
                <p className="text-xs text-gray-500 italic mb-2">No gateways registered yet.</p>
              )}
            </div>

            {/* Error Message */}
            {claimError && (
              <div className="mt-3 p-2 text-xs bg-red-50 text-red-700 border border-red-200 rounded">
                {claimError}
              </div>
            )}

            {/* Tabs for Adding / Claiming */}
            <div className="mt-5 border-t pt-4">
              <div className="flex border-b border-gray-200 mb-3">
                <button
                  type="button"
                  onClick={() => setModalTab('claim')}
                  className={`pb-2 px-3 text-xs font-semibold transition-colors border-b-2 ${
                    modalTab === 'claim'
                      ? 'border-blue-600 text-blue-600'
                      : 'border-transparent text-gray-500 hover:text-gray-700'
                  }`}
                >
                  Claim via Code (OLED)
                </button>
                <button
                  type="button"
                  onClick={() => setModalTab('direct')}
                  className={`pb-2 px-3 text-xs font-semibold transition-colors border-b-2 ${
                    modalTab === 'direct'
                      ? 'border-blue-600 text-blue-600'
                      : 'border-transparent text-gray-500 hover:text-gray-700'
                  }`}
                >
                  Direct Register
                </button>
              </div>

              {modalTab === 'claim' ? (
                /* FSD §8.2 Claim Form */
                <form onSubmit={handleClaimGateway} className="space-y-3">
                  <div className="bg-amber-50 border border-amber-200 rounded p-2.5 text-xs text-amber-800">
                    <span className="font-semibold">💡 FSD Claim Flow:</span> Check Screen 3 on your Smart Clock OLED display for the 6-character claim code (e.g. <span className="font-mono font-bold">GW-9821A0</span>).
                  </div>
                  <div>
                    <label className="block text-xs font-medium text-gray-700 mb-1">
                      Claim Code
                    </label>
                    <input
                      name="claim_code"
                      type="text"
                      placeholder="e.g. GW-9821A0"
                      className="w-full px-3 py-2 border rounded-md text-sm uppercase tracking-wider font-mono"
                      required
                    />
                  </div>
                  <div>
                    <label className="block text-xs font-medium text-gray-700 mb-1">
                      Gateway Friendly Name
                    </label>
                    <input
                      name="gateway_name"
                      type="text"
                      placeholder="Living Room Clock"
                      className="w-full px-3 py-2 border rounded-md text-sm"
                      required
                    />
                  </div>
                  <button
                    type="submit"
                    disabled={claimLoading}
                    className="w-full py-2 bg-blue-600 text-white rounded-md text-sm font-medium hover:bg-blue-700 disabled:bg-blue-300 transition-colors shadow-sm"
                  >
                    {claimLoading ? 'Claiming Gateway…' : 'Claim Gateway'}
                  </button>
                </form>
              ) : (
                /* Direct Register Form */
                <form onSubmit={handleAddGateway} className="space-y-3">
                  <div>
                    <label className="block text-xs font-medium text-gray-700 mb-1">
                      Gateway ID
                    </label>
                    <input
                      name="gateway_id"
                      type="text"
                      placeholder="gateway_001"
                      className="w-full px-3 py-2 border rounded-md text-sm font-mono"
                      required
                    />
                  </div>
                  <div>
                    <label className="block text-xs font-medium text-gray-700 mb-1">
                      Friendly Name
                    </label>
                    <input
                      name="gateway_name"
                      type="text"
                      placeholder="Living Room Clock"
                      className="w-full px-3 py-2 border rounded-md text-sm"
                      required
                    />
                  </div>
                  <button
                    type="submit"
                    disabled={claimLoading}
                    className="w-full py-2 bg-gray-800 text-white rounded-md text-sm font-medium hover:bg-gray-900 disabled:bg-gray-400 transition-colors shadow-sm"
                  >
                    {claimLoading ? 'Registering…' : 'Register Gateway'}
                  </button>
                </form>
              )}
            </div>

            <button
              onClick={() => setShowGatewayModal(false)}
              className="w-full mt-4 py-1.5 text-xs text-gray-500 hover:text-gray-800 transition-colors"
            >
              Close
            </button>
          </div>
        </div>
      )}
    </div>
  )
}

export default Layout
