import React, { useState, useEffect } from 'react'
import { NavLink, useLocation, useNavigate } from 'react-router-dom'
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
  const location = useLocation()

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

  const handleAddGateway = async (e) => {
    e.preventDefault()
    const id = e.target.gateway_id.value.trim()
    const name = e.target.gateway_name.value.trim()
    if (!id || !name) return
    try {
      await gatewaysAPI.create({ id, name })
      setShowGatewayModal(false)
      e.target.reset()
      loadGateways()
    } catch (err) {
      alert(err.message || 'Failed to create gateway')
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

      {/* Gateway selection modal */}
      {showGatewayModal && (
        <div className="fixed inset-0 bg-black/40 flex items-center justify-center z-50">
          <div className="bg-white rounded-lg p-6 w-80">
            <h2 className="text-lg font-semibold mb-4">Select Gateway</h2>
            {gateways.length > 0 ? (
              <div className="space-y-2 mb-4">
                {gateways.map((gw) => (
                  <button
                    key={gw.id}
                    onClick={() => handleGatewaySelect(gw.id)}
                    className="w-full text-left p-3 border rounded hover:bg-gray-50"
                  >
                    <div className="font-medium">{gw.name}</div>
                    <div className="text-sm text-gray-500">{gw.id}</div>
                    <div className={`text-xs mt-1 ${gw.status === 'online' ? 'text-green-600' : 'text-red-600'}`}>
                      {gw.status === 'online' ? '● Online' : '● Offline'}
                    </div>
                  </button>
                ))}
              </div>
            ) : (
              <p className="text-sm text-gray-500 mb-4">No gateways registered yet.</p>
            )}
            <div className="border-t pt-4">
              <h3 className="text-sm font-medium mb-2">Or add a new gateway:</h3>
              <form onSubmit={handleAddGateway} className="space-y-3">
                <div>
                  <label className="block text-xs text-gray-600 mb-1">Gateway ID</label>
                  <input
                    name="gateway_id"
                    type="text"
                    placeholder="gateway_001"
                    className="w-full px-3 py-2 border rounded-md text-sm"
                    required
                  />
                </div>
                <div>
                  <label className="block text-xs text-gray-600 mb-1">Name</label>
                  <input
                    name="gateway_name"
                    type="text"
                    placeholder="Living Room Clock"
                    className="w-full px-3 py-2 border rounded-md text-sm"
                    required
                  />
                </div>
                <button type="submit" className="w-full px-3 py-2 bg-blue-600 text-white rounded-md text-sm hover:bg-blue-700">
                  Add Gateway
                </button>
              </form>
            </div>
            <button
              onClick={() => setShowGatewayModal(false)}
              className="w-full mt-3 px-3 py-2 text-gray-600 hover:bg-gray-100 rounded-md text-sm"
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
