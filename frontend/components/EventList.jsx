import { campusParts } from '../utils/campusInfo'

const FLAG_VERSION = '20251220b'

const TYPE_COLORS = {
  connection:   '#2ecc71',
  deconnection: '#cf4636',
  evaluation:   '#1f3ad8',
  correction:   '#e67e22',
  new_seen:     '#9b59b6',
  wallet:       '#f1c40f',
  data:         '#1abc9c',
  error:        '#95a5a6',
}

function parseEventMs(event) {
  const updMs = event?.updated_at ? Date.parse(event.updated_at) : NaN
  const tsMs  = typeof event?.ts === 'number' ? event.ts * 1000 : NaN
  if (Number.isFinite(updMs) && Number.isFinite(tsMs))
    return updMs < tsMs - 5 * 60_000 ? tsMs : updMs
  return Number.isFinite(updMs) ? updMs : tsMs
}

function formatTimestamp(event) {
  const ms = parseEventMs(event)
  if (!Number.isFinite(ms)) return event?.updated_at || '—'
  return new Date(Math.round((ms + 20_000) / 1000) * 1000)
    .toISOString().replace('T', ' ').slice(0, 19)
}

function titleCase(text) {
  return (text || '').replace(/[\s_]+(.)/g, (_, c) => ' ' + c.toUpperCase())
    .replace(/^./, c => c.toUpperCase())
}

function EventItem({ event, isTop }) {
  const types      = Array.isArray(event.types) ? event.types : []
  const primary    = types[0] || ''
  const typeColor  = TYPE_COLORS[primary] || 'var(--text)'
  const { label: campusLabel, flagSvg, country } = campusParts(event.campus_id)
  const userLabel  = event.user_login
    ? `${event.user_login} (#${event.user_id ?? '?'}) · ${campusLabel}`
    : `#${event.user_id ?? '?'} · ${campusLabel}`

  return (
    <li style={isTop ? { border: `1px solid ${typeColor}`, color: typeColor, minHeight: 40, marginBottom: '0.25rem' } : {}}>
      <div style={{ display: 'grid', gridTemplateColumns: '180px 140px 1fr', gap: '0.45rem', alignItems: 'center' }}>
        <span style={{ fontFamily: 'monospace', fontSize: '0.82rem', color: isTop ? 'inherit' : 'var(--text-muted)', textAlign: 'center' }}>
          {formatTimestamp(event)}
        </span>
        <span style={{ fontWeight: 700, color: isTop ? 'inherit' : typeColor, textAlign: 'center', fontSize: '0.9rem' }}>
          {titleCase(primary || 'unknown')}
        </span>
        <span style={{ display: 'flex', flexWrap: 'wrap', gap: '0.4rem', alignItems: 'center', fontSize: '0.88rem' }}>
          {userLabel}
          {flagSvg && (
            <img
              src={`/assets/twemoji/svg/${flagSvg}.svg?v=${FLAG_VERSION}`}
              alt={country ? `${country} flag` : 'flag'}
              loading="lazy"
              style={{ width: 18, height: 18, flexShrink: 0 }}
            />
          )}
        </span>
      </div>
    </li>
  )
}

export default function EventList({ events }) {
  if (!events.length) {
    return (
      <ul style={{ flex: 1, listStyle: 'none', margin: 0, padding: '2rem', color: 'var(--text-muted)', textAlign: 'center' }}>
        <li>Waiting for events…</li>
      </ul>
    )
  }
  return (
    <ul
      aria-live="polite"
      style={{ flex: 1, listStyle: 'none', margin: 0, padding: '0.5rem 0.25rem', display: 'flex', flexDirection: 'column', gap: '0.3rem', overflowY: 'auto', minHeight: 0 }}
    >
      {events.map((ev, i) => (
        <EventItem
          key={ev._event_id || `${ev.user_id}-${ev.updated_at}-${i}`}
          event={ev}
          isTop={i === 0}
        />
      ))}
    </ul>
  )
}