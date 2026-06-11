import { useState, useEffect, useRef } from 'react'

const WS_BASE_RETRY_MS   = 1_000
const WS_MAX_RETRY_MS    = 10_000
const ANALYTICS_WINDOW_MS = 60 * 60_000
const MAX_DISPLAY        = 10

export function useEventWebSocket() {
  const [events,          setEvents]          = useState([])
  const [analyticsEvents, setAnalyticsEvents] = useState([])
  const [connected,       setConnected]       = useState(false)

  const wsRef          = useRef(null)
  const retryMsRef     = useRef(WS_BASE_RETRY_MS)
  const retryTimerRef  = useRef(null)
  const lastEventIdRef = useRef('')
  const windowMapRef   = useRef(new Map())

  useEffect(() => {
    let cancelled = false

    function buildUrl() {
      const proto = location.protocol === 'https:' ? 'wss:' : 'ws:'
      const url   = new URL(`${proto}//${location.host}/ws/events`)
      if (lastEventIdRef.current) url.searchParams.set('since_id', lastEventIdRef.current)
      return url.toString()
    }

    function flush() {
      const nowMs  = Date.now()
      const cutoff = nowMs - ANALYTICS_WINDOW_MS

      // Prune stale entries
      for (const [id, env] of windowMapRef.current) {
        if ((env.event_ms || 0) < cutoff) windowMapRef.current.delete(id)
      }

      const sorted = Array.from(windowMapRef.current.values())
        .sort((a, b) => (b.event_ms || 0) - (a.event_ms || 0))

      setEvents(sorted.slice(0, MAX_DISPLAY).map(e => e.event))
      setAnalyticsEvents(sorted.map(e => e.event))
    }

    function ingest(incoming, replaceWindow) {
      if (replaceWindow) windowMapRef.current.clear()
      for (const envelope of (incoming || [])) {
        if (!envelope?.id) continue
        windowMapRef.current.set(envelope.id, envelope)
        if (envelope.id) lastEventIdRef.current = envelope.id
      }
      flush()
    }

    function connect() {
      if (cancelled) return
      const ws = new WebSocket(buildUrl())
      wsRef.current = ws

      ws.onopen = () => {
        if (cancelled) { ws.close(); return }
        setConnected(true)
        retryMsRef.current = WS_BASE_RETRY_MS
      }

      ws.onmessage = ({ data }) => {
        if (cancelled) return
        let payload
        try { payload = JSON.parse(data) } catch { return }
        const type = payload?.type
        if (type === 'events_snapshot') ingest(payload.events, payload.mode === 'full')
        else if (type === 'events_delta') ingest(payload.events, false)
      }

      ws.onclose = () => {
        if (cancelled) return
        setConnected(false)
        wsRef.current = null
        retryTimerRef.current = setTimeout(() => {
          retryMsRef.current = Math.min(WS_MAX_RETRY_MS, Math.round(retryMsRef.current * 1.7))
          connect()
        }, retryMsRef.current)
      }

      ws.onerror = () => {}
    }

    connect()
    return () => {
      cancelled = true
      clearTimeout(retryTimerRef.current)
      wsRef.current?.close()
    }
  }, [])

  return { events, analyticsEvents, connected }
}