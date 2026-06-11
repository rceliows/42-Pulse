const THRESHOLDS = {
  fetch_queue_internal: { key: 'fetch_queue_internal_warn_max',      level: 'warn' },
  fetch_queue_external: { key: 'fetch_queue_external_degraded_max',  level: 'bad' },
  process_queue:        { key: 'process_queue_degraded_max',         level: 'bad' },
}

const BADGE = {
  ok:   { background: 'rgba(53,222,140,0.15)',  color: '#35de8c' },
  warn: { background: 'rgba(255,191,105,0.15)', color: '#ffbf69' },
  bad:  { background: 'rgba(255,107,107,0.15)', color: '#ff6b6b' },
}

export default function QueueTable({ queues, thresholds }) {
  if (!queues) return null
  const rows = Object.entries(queues)

  return (
    <div style={{ overflowX: 'auto', border: '1px solid var(--border)', borderRadius: 10 }}>
      <table style={{ width: '100%', borderCollapse: 'collapse' }}>
        <thead>
          <tr>
            {['Queue','Count','Oldest SLA (min)','Limit','Flag'].map(h => (
              <th key={h} style={{ textAlign: 'left', padding: '0.55rem', borderBottom: '1px solid var(--border)', color: 'var(--text-muted)', fontSize: '0.85rem', background: 'var(--panel-muted)' }}>{h}</th>
            ))}
          </tr>
        </thead>
        <tbody>
          {rows.map(([name, payload]) => {
            const cfg     = THRESHOLDS[name]
            const limit   = thresholds?.[cfg?.key] ?? null
            const count   = payload?.count ?? 0
            const over    = limit !== null && count > limit
            const badge   = over ? BADGE[cfg?.level || 'warn'] : BADGE.ok
            const badgeTxt = over ? (cfg?.level === 'bad' ? 'DEGRADED' : 'WARN') : 'OK'
            return (
              <tr key={name}>
                <td style={{ padding: '0.55rem', borderBottom: '1px solid var(--border)', fontSize: '0.9rem', fontFamily: 'monospace' }}>{name}</td>
                <td style={{ padding: '0.55rem', borderBottom: '1px solid var(--border)', fontFamily: 'monospace' }}>{count}</td>
                <td style={{ padding: '0.55rem', borderBottom: '1px solid var(--border)', fontFamily: 'monospace' }}>{payload?.oldest_sla_minutes ?? '—'}</td>
                <td style={{ padding: '0.55rem', borderBottom: '1px solid var(--border)', fontFamily: 'monospace' }}>{limit ?? '—'}</td>
                <td style={{ padding: '0.55rem', borderBottom: '1px solid var(--border)' }}>
                  <span style={{ ...badge, padding: '0.2rem 0.6rem', borderRadius: 999, fontSize: '0.78rem', fontWeight: 600 }}>{badgeTxt}</span>
                </td>
              </tr>
            )
          })}
        </tbody>
      </table>
    </div>
  )
}