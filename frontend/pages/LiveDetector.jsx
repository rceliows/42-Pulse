import { useEventWebSocket } from '../hooks/useWebSocket'
import EventList      from '../components/EventList'
import AnalyticsPanel from '../components/AnalyticsPanel'

export default function LiveDetector() {
  const { events, analyticsEvents, connected } = useEventWebSocket()

  return (
    <main>
      <section style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '1.5rem' }}>

        <div className="panel" style={{ display: 'flex', flexDirection: 'column', minHeight: '79vh' }}>
          <div className="panel-header" style={{ flexShrink: 0 }}>
            <h2>Last 10 events (UTC)</h2>
            <div style={{ display: 'flex', alignItems: 'center', gap: '0.75rem' }}>
              <span
                title={connected ? 'connected' : 'disconnected'}
                style={{ width: 10, height: 10, borderRadius: '50%', background: connected ? '#2ecc71' : '#e74c3c', display: 'inline-block' }}
              />
              <span style={{ fontFamily: 'monospace', fontSize: '0.8rem', color: 'var(--text-muted)', border: '1px solid var(--border)', borderRadius: 999, padding: '0.25rem 0.65rem' }}>
                /ws/events
              </span>
            </div>
          </div>
          <EventList events={events} />
        </div>

        <AnalyticsPanel analyticsEvents={analyticsEvents} />

      </section>
    </main>
  )
}