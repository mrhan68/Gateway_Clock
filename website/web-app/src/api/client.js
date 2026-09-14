import axios from 'axios'

// Base URL from env (Vite exposes import.meta.env with VITE_ prefix)
const baseURL = import.meta.env.VITE_API_URL || ''

const api = axios.create({
  baseURL,
  timeout: 10000,
  headers: {
    'Content-Type': 'application/json',
  },
})

// --- Gateway selection ---
// In this simple MVP, one gateway is active. In a multi-user app,
// this would be stored in auth context.
let activeGateway = localStorage.getItem('activeGateway') || null

export const setActiveGateway = (id) => {
  activeGateway = id
  localStorage.setItem('activeGateway', id)
}

export const getActiveGateway = () => activeGateway

// --- Request interceptor ---
// Attach JWT if present (should-have feature, for now we use a placeholder)
api.interceptors.request.use((config) => {
  const token = localStorage.getItem('authToken')
  if (token) {
    config.headers.Authorization = `Bearer ${token}`
  }
  return config
})

// --- Response interceptor ---
// Normalize error responses to FSD §8.9 format
api.interceptors.response.use(
  (response) => response,
  (error) => {
    if (error.response) {
      // Server responded with an error status
      const { data } = error.response
      const errMsg = data?.error?.message || error.message || 'Unknown error'
      const errCode = data?.error?.code || 'API_ERROR'
      return Promise.reject({ message: errMsg, code: errCode, status: error.response.status })
    } else if (error.request) {
      // Request was made but no response received
      return Promise.reject({ message: 'No response from server — backend may be down', code: 'NETWORK_ERROR', status: 0 })
    }
    return Promise.reject({ message: error.message, code: 'UNKNOWN', status: 0 })
  },
)

// ─── Gateway API ───
export const gatewaysAPI = {
  list: () => api.get('/api/v1/gateways'),
  get: (id) => api.get(`/api/v1/gateways/${id}`),
  create: (payload) => api.post('/api/v1/gateways', payload),
  update: (id, payload) => api.put(`/api/v1/gateways/${id}`, payload),
}

// ─── Device API ───
export const devicesAPI = {
  list: (gatewayId) => api.get(`/api/v1/gateways/${gatewayId}/devices`),
  create: (gatewayId, payload) => api.post(`/api/v1/gateways/${gatewayId}/devices`, payload),
  update: (gatewayId, deviceId, payload) => api.put(`/api/v1/gateways/${gatewayId}/devices/${deviceId}`, payload),
  delete: (gatewayId, deviceId) => api.delete(`/api/v1/gateways/${gatewayId}/devices/${deviceId}`),
  sendCommand: (gatewayId, deviceId, command) =>
    api.post(`/api/v1/gateways/${gatewayId}/devices/${deviceId}/command`, { command }),
}

// ─── Schedule API ───
export const schedulesAPI = {
  list: (gatewayId, params = {}) => {
    const qs = new URLSearchParams(params).toString()
    return api.get(`/api/v1/gateways/${gatewayId}/schedules${qs ? `?${qs}` : ''}`)
  },
  create: (gatewayId, payload) => api.post(`/api/v1/gateways/${gatewayId}/schedules`, payload),
  update: (gatewayId, id, payload) => api.put(`/api/v1/gateways/${gatewayId}/schedules/${id}`, payload),
  delete: (gatewayId, id) => api.delete(`/api/v1/gateways/${gatewayId}/schedules/${id}`),
  toggle: (gatewayId, id, enabled) => api.patch(`/api/v1/gateways/${gatewayId}/schedules/${id}/toggle`, { enabled }),
}

// ─── Rules API ───
export const rulesAPI = {
  list: (gatewayId) => api.get(`/api/v1/gateways/${gatewayId}/rules`),
  create: (gatewayId, payload) => api.post(`/api/v1/gateways/${gatewayId}/rules`, payload),
  update: (gatewayId, id, payload) => api.put(`/api/v1/gateways/${gatewayId}/rules/${id}`, payload),
  delete: (gatewayId, id) => api.delete(`/api/v1/gateways/${gatewayId}/rules/${id}`),
}

// ─── Events API ───
export const eventsAPI = {
  list: (gatewayId, params = {}) => {
    const qs = new URLSearchParams(params).toString()
    return api.get(`/api/v1/gateways/${gatewayId}/events${qs ? `?${qs}` : ''}`)
  },
}

// ─── Telemetry API ───
export const telemetryAPI = {
  list: (gatewayId, deviceId, params = {}) => {
    const qs = new URLSearchParams(params).toString()
    return api.get(`/api/v1/gateways/${gatewayId}/devices/${deviceId}/telemetry${qs ? `?${qs}` : ''}`)
  },
}

// --- Health check ---
export const healthCheck = () => api.get('/health')

export default api
