import { Navigate, useLocation } from 'react-router-dom'
import { useAuth } from '../hooks/useAuth'

export default function ProtectedRoute({ children }) {
  const { status } = useAuth()
  const location = useLocation()

  if (status === 'loading') return null
  if (status === 'unauthenticated')
    return <Navigate to={`/login?next=${encodeURIComponent(location.pathname)}`} replace />

  return children
}
