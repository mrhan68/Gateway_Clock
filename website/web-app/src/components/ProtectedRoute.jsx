import React from 'react'
import { Navigate, useLocation } from 'react-router-dom'
import { authAPI } from '../api/client'

export default function ProtectedRoute({ children }) {
  const location = useLocation()
  const authenticated = authAPI.isAuthenticated()

  if (!authenticated) {
    return <Navigate to="/login" state={{ from: location }} replace />
  }

  return children
}
