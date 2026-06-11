import { createContext, useContext, useState, useEffect } from 'react'

const AuthContext = createContext({ status: 'loading', isAdmin: false })

export function AuthProvider({ children }) {
  const [status,  setStatus]  = useState('loading')
  const [isAdmin, setIsAdmin] = useState(false)

  useEffect(() => {
    fetch('/api/auth/me', { cache: 'no-store' })
      .then(async res => {
        if (res.ok) {
          const data = await res.json()
          setIsAdmin(data.user?.role === 'admin')
          setStatus('authenticated')
        } else {
          setStatus('unauthenticated')
        }
      })
      .catch(() => setStatus('unauthenticated'))
  }, [])

  return <AuthContext.Provider value={{ status, isAdmin }}>{children}</AuthContext.Provider>
}

export function useAuth() {
  return useContext(AuthContext)
}
