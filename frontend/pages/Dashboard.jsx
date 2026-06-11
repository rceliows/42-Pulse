import { useState, useEffect } from 'react'
import Histogram  from '../components/Histogram'
import CampusTable from '../components/CampusTable'

const HOURS = 24

const PULSE_TIER = {
  low:       { border: 'rgba(53,222,140,0.5)',   shadow: 'rgba(53,222,140,0.14)' },
  medium:    { border: 'rgba(155,220,93,0.52)',   shadow: 'rgba(155,220,93,0.16)' },
  high:      { border: 'rgba(255,191,105,0.55)',  shadow: 'rgba(255,191,105,0.16)' },
  very_high: { border: 'rgba(255,107,107,0.62)',  shadow: 'rgba(255,107,107,0.2)' },
}

function fmt(n) { return Number(n || 0).toLocaleString('en-US') }
function fmtDate(v) {
  if (!v) return '—'
  const t = Date.parse(String(v))
  return Number.isFinite(t) ? new Date(t).toISOString().slice(0, 19).replace('T', ' ') : '—'
}

export default function Dashboard() {
  const [data,     setData]     = useState(null)
  const [error,    setError]    = useState(null)
  const [grouping, setGrouping] = useState('campus')  // 'campus' | 'country'

  useEffect(() => {
    let mounted = true
    const load = () =>
      fetch(`/api/events/db/dashboard?hours=${HOURS}`, { cache: 'no-store', headers: { Accept: 'application/json' } })
        .then(r => r.ok ? r.json() : Promise.reject(`HTTP ${r.status}`))
        .then(d => { if (mounted) setData(d) })
        .catch(e => { if (mounted) setError(String(e)) })
    load()
    const timer = setInterval(load, 10_000)
    return () => { mounted = false; clearInterval(timer) }
  }, [])

  if (error) return <main><p style={{ color: 'var(--danger)', padding: '1rem' }}>Dashboard error: {error}</p></main>
  if (!data)  return <main><p style={{ color: 'var(--text-muted)', padding: '1rem' }}>Loading dashboard…</p></main>

  const summary   = data.summary   || {}
  const intensity = summary.last_hour_intensity || { tier: 'low', label: '<500 low' }
  const tierStyle = PULSE_TIER[intensity.tier] || PULSE_TIER.low
  const tableRows = grouping === 'country' ? (data.countries_by_pulses || []) : (data.campuses_by_pulses || [])

  return (
    <main>
      <section className="panel">
        <div className="panel-header">
          <h2>Detector Events Dashboard</h2>
          <div style={{ display: 'flex', gap: '0.5rem', flexWrap: 'wrap' }}>
            {[
              `source=${data.source || 'detector_events'}`,
              `window=${HOURS}h`,
              `updated=${fmtDate(data.timestamp_utc)}`,
            ].map(t => (
              <span key={t} style={{ border: '1px solid var(--border)', borderRadius: 999, padding: '0.2rem 0.55rem', fontSize: '0.85rem', color: 'var(--text-muted)' }}>{t}</span>
            ))}
          </div>
        </div>

        {/* Summary cards */}
        <div style={{ display: 'grid', gridTemplateColumns: '1.4fr repeat(3, 1fr)', gap: '0.85rem' }}>
          {/* Hero pulse card */}
          <div style={{ border: `2px solid ${tierStyle.border}`, boxShadow: `inset 0 0 0 1px ${tierStyle.shadow}`, borderRadius: 10, padding: '0.85rem', background: 'var(--panel-muted)', display: 'flex', flexDirection: 'column', gap: '0.4rem' }}>
            <div style={{ color: 'var(--text-muted)', fontSize: '0.82rem' }}>Pulses · rolling last 1h</div>
            <div style={{ fontFamily: 'monospace', fontSize: 'clamp(2rem,4vw,3.2rem)', lineHeight: 1 }}>{fmt(summary.pulses_last_hour)}</div>
            <div style={{ fontSize: '0.84rem', color: 'var(--text-muted)' }}>{intensity.label}</div>
          </div>
          {[
            ['Pulses · last 24h',                fmt(summary.pulses_window)],
            ['Unique users · last 24h',          fmt(summary.unique_users_window)],
            ['Unique users · DB',                fmt(summary.unique_users_db)],
          ].map(([label, value]) => (
            <div key={label} style={{ border: '1px solid var(--border)', borderRadius: 10, padding: '0.85rem', background: 'var(--panel-muted)', display: 'flex', flexDirection: 'column', gap: '0.4rem' }}>
              <div style={{ color: 'var(--text-muted)', fontSize: '0.82rem' }}>{label}</div>
              <div style={{ fontFamily: 'monospace', fontSize: '1.35rem' }}>{value}</div>
            </div>
          ))}
        </div>

        <h3 style={{ margin: '0.3rem 0 0 0' }}>Pulses histogram by hour · rolling last 24h (UTC)</h3>
        <div style={{ border: '1px solid var(--border)', borderRadius: 10, padding: '0.7rem', background: 'var(--panel-muted)', overflowX: 'auto' }}>
          <Histogram data={data.histogram_by_hour || []} />
        </div>
      </section>

      <section className="panel">
        <div className="panel-header">
          <h2>
            {grouping === 'country'
              ? 'Countries ordered by pulses · last 24h'
              : 'Campuses ordered by pulses · last 24h'}
          </h2>
          <div style={{ display: 'flex', gap: '0.5rem', alignItems: 'center' }}>
            <button
              className="ghost"
              style={{ padding: '0.35rem 0.7rem', fontSize: '0.84rem' }}
              onClick={() => setGrouping(g => g === 'campus' ? 'country' : 'campus')}
            >
              Group: {grouping === 'campus' ? 'campuses' : 'countries'}
            </button>
            <span style={{ border: '1px solid var(--border)', borderRadius: 999, padding: '0.2rem 0.55rem', fontSize: '0.85rem', color: 'var(--text-muted)' }}>
              {grouping === 'campus' ? 'campuses' : 'countries'}={fmt(tableRows.length)}
            </span>
          </div>
        </div>
        <CampusTable rows={tableRows} grouping={grouping} />
      </section>
    </main>
  )
}