import { useMemo } from 'react'
import { campusParts } from '../utils/campusInfo'
import { useLang } from '../context/LangContext'

const ANALYTICS_WINDOW_MS = 60 * 60_000

const T = {
  en: {
    title:      'Detector Analytics · rolling 1h',
    total:      'Total events',
    users:      'Distinct users',
    campuses:   'Campuses',
    topType:    'Top event type',
    byCampus:   'Events by campus · top 7',
    noData:     'No data yet.',
    eventMix:   'Event mix · all types',
  },
  fr: {
    title:      'Analytiques du détecteur · dernière heure',
    total:      'Total événements',
    users:      'Utilisateurs uniques',
    campuses:   'Campus',
    topType:    'Type d\'événement principal',
    byCampus:   'Événements par campus · top 7',
    noData:     'Pas de données.',
    eventMix:   'Mix d\'événements · tous types',
  },
  es: {
    title:      'Analítica del detector · última hora',
    total:      'Total eventos',
    users:      'Usuarios únicos',
    campuses:   'Campus',
    topType:    'Tipo de evento principal',
    byCampus:   'Eventos por campus · top 7',
    noData:     'Sin datos aún.',
    eventMix:   'Mix de eventos · todos los tipos',
  },
}

const TYPE_COLORS = {
  new_seen:     '#9b59b6',
  connection:   '#2ecc71',
  deconnection: '#cf4636',
  evaluation:   '#1f3ad8',
  correction:   '#e67e22',
  wallet:       '#f1c40f',
  data:         '#1abc9c',
  error:        '#95a5a6',
}
const FALLBACK_COLORS = ['#3a7afe','#2ecc71','#f39c12','#e74c3c','#9b59b6','#1abc9c','#95a5a6']

function mixColor(type, idx) {
  return TYPE_COLORS[String(type).toLowerCase()] || FALLBACK_COLORS[idx % FALLBACK_COLORS.length]
}

function titleCase(text) {
  return (text || '').replace(/[\s_]+(.)/g, (_, c) => ' ' + c.toUpperCase())
    .replace(/^./, c => c.toUpperCase())
}

function parseEventMs(event) {
  const tsMs  = typeof event?.ts === 'number' ? event.ts * 1000 : NaN
  const updMs = event?.updated_at ? Date.parse(event.updated_at) : NaN
  if (Number.isFinite(tsMs)) return tsMs
  return Number.isFinite(updMs) ? updMs : NaN
}

export default function AnalyticsPanel({ analyticsEvents }) {
  const { lang } = useLang()
  const t = T[lang] || T.en

  const stats = useMemo(() => {
    const nowMs  = Date.now()
    const cutoff = nowMs - ANALYTICS_WINDOW_MS
    const window = (analyticsEvents || []).filter(ev => {
      const ms = parseEventMs(ev)
      return Number.isFinite(ms) && ms >= cutoff && ms <= nowMs + 60_000
    })

    const campusCount = new Map()
    const typeCount   = new Map()
    const userSet     = new Set()

    for (const ev of window) {
      const campus = ev.campus_id != null ? String(ev.campus_id) : null
      if (campus) campusCount.set(campus, (campusCount.get(campus) || 0) + 1)

      const types = Array.isArray(ev.types) && ev.types.length ? ev.types : ['unknown']
      for (const tp of types) typeCount.set(tp, (typeCount.get(tp) || 0) + 1)

      if (ev.user_id) userSet.add(ev.user_id)
      else if (ev.user_login) userSet.add(ev.user_login)
    }

    const campusRows = [...campusCount.entries()].sort((a, b) => b[1] - a[1]).slice(0, 7)
    const typeRows   = [...typeCount.entries()].sort((a, b) => b[1] - a[1])
    const topType    = typeRows[0]?.[0] || '—'

    return { total: window.length, users: userSet.size, campuses: campusCount.size, topType, campusRows, typeRows }
  }, [analyticsEvents])

  const pieBg = useMemo(() => {
    const total = stats.typeRows.reduce((s, [, c]) => s + c, 0)
    if (!total) return 'conic-gradient(#3a7afe 0deg, #3a7afe 360deg)'
    let acc = 0
    const segments = stats.typeRows.map(([type, count], i) => {
      const color = mixColor(type, i)
      const start = (acc / total) * 360
      acc += count
      const end = (acc / total) * 360
      return `${color} ${start.toFixed(2)}deg ${end.toFixed(2)}deg`
    })
    return `conic-gradient(${segments.join(', ')})`
  }, [stats.typeRows])

  const maxCampus = stats.campusRows[0]?.[1] || 1

  return (
    <div className="panel" style={{ display: 'flex', flexDirection: 'column', gap: '0.75rem', overflowY: 'auto' }}>
      <div className="panel-header">
        <h2>{t.title}</h2>
      </div>

      <div className="analytics-summary-grid">
        {[
          [t.total,   stats.total],
          [t.users,   stats.users],
          [t.campuses, stats.campuses],
          [t.topType, titleCase(stats.topType)],
        ].map(([label, value]) => (
          <div key={label} style={{ border: '1px solid var(--border)', borderRadius: 8, padding: '0.75rem', background: 'rgba(255,255,255,0.02)', textAlign: 'center' }}>
            <div style={{ color: 'var(--text-muted)', fontSize: '0.78rem', marginBottom: '0.2rem' }}>{label}</div>
            <div style={{ fontFamily: 'monospace' }}>{value}</div>
          </div>
        ))}
      </div>

      <div className="analytics-charts-grid">
        <div style={{ border: '1px solid var(--border)', borderRadius: 8, background: 'var(--panel-muted)', padding: '0.75rem' }}>
          <h3 style={{ margin: '0 0 0.55rem 0', fontSize: '0.92rem' }}>{t.byCampus}</h3>
          {stats.campusRows.length === 0 ? (
            <p style={{ color: 'var(--text-muted)', fontSize: '0.85rem' }}>{t.noData}</p>
          ) : (
            <ol style={{ listStyle: 'none', margin: 0, padding: 0, display: 'flex', flexDirection: 'column', gap: '0.62rem' }}>
              {stats.campusRows.map(([campusKey, count], i) => {
                const { label } = campusParts(isNaN(Number(campusKey)) ? campusKey : Number(campusKey))
                const ratio = (count / maxCampus) * 100
                return (
                  <li key={campusKey} style={{ display: 'flex', flexDirection: 'column', gap: '0.35rem', background: 'transparent', border: 'none', padding: '0.25rem 0', borderRadius: 0 }}>
                    <div style={{ display: 'grid', gridTemplateColumns: 'minmax(0, 1fr) auto', gap: '0.5rem', alignItems: 'baseline' }}>
                      <span style={{ fontSize: '0.85rem' }}>
                        {i + 1}. {label}
                      </span>
                      <span style={{ fontFamily: 'monospace', fontSize: '0.85rem' }}>{count}</span>
                    </div>
                    <div style={{ width: '100%', height: 8, borderRadius: 999, background: 'rgba(255,255,255,0.08)', overflow: 'hidden' }}>
                      <div style={{ height: '100%', borderRadius: 999, background: 'linear-gradient(90deg,#2d9cdb,#27ae60)', width: `${ratio.toFixed(1)}%` }} />
                    </div>
                  </li>
                )
              })}
            </ol>
          )}
        </div>

        <div style={{ border: '1px solid var(--border)', borderRadius: 8, background: 'var(--panel-muted)', padding: '0.75rem', display: 'flex', flexDirection: 'column', gap: '0.7rem' }}>
          <h3 style={{ margin: 0, fontSize: '0.92rem' }}>{t.eventMix}</h3>
          <div style={{ width: '80%', aspectRatio: '1/1', borderRadius: '50%', border: '1px solid var(--border)', background: pieBg, margin: '0 auto' }} />
          <ul style={{ listStyle: 'none', margin: 0, padding: 0, display: 'grid', gridTemplateColumns: 'minmax(0, 1fr) minmax(0, 1fr)', gap: '0.18rem 0.7rem', maxHeight: 'none', overflow: 'visible' }}>
            {stats.typeRows.map(([type, count], i) => {
              const total = stats.typeRows.reduce((s, [, c]) => s + c, 0)
              const pct   = total > 0 ? ((count / total) * 100).toFixed(1) : '0.0'
              return (
                <li key={type} style={{ display: 'grid', gridTemplateColumns: 'auto 1fr auto', gap: '0.35rem', alignItems: 'center', fontSize: '0.76rem', background: 'transparent', border: 'none', padding: '0.15rem 0', borderRadius: 0 }}>
                  <span style={{ width: 10, height: 10, borderRadius: '50%', background: mixColor(type, i), flexShrink: 0 }} />
                  <span>{titleCase(type)}</span>
                  <span style={{ fontFamily: 'monospace', whiteSpace: 'nowrap' }}>{count} ({pct}%)</span>
                </li>
              )
            })}
          </ul>
        </div>
      </div>
    </div>
  )
}
