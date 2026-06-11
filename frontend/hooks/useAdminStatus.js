// src/hooks/useAdminStatus.js
import { useState, useEffect } from 'react'

export function useAdminStatus(refreshMs = 10000) {
  const [data, setData]   = useState(null)
  const [error, setError] = useState(null)

  useEffect(() => {
    const fetch_ = async () => {
      try {
        const res = await fetch('/api/admin/status', { cache: 'no-store' })
        if (!res.ok) throw new Error(`HTTP ${res.status}`)
        setData(await res.json())
      } catch (err) {
        setError(err.message)
      }
    }
    fetch_()
    const timer = setInterval(fetch_, refreshMs)
    return () => clearInterval(timer)
  }, [refreshMs])

  return { data, error }
}