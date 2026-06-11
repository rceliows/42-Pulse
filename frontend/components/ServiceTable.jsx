function dot(health, running) {
  if (!running || health === 'down')       return { color: '#ff6b6b', label: health || 'down' }
  if (health === 'healthy')                return { color: '#35de8c', label: 'healthy' }
  if (health === 'starting')               return { color: '#ffbf69', label: 'starting' }
  if (health === 'unhealthy')              return { color: '#ff6b6b', label: 'unhealthy' }
  return { color: '#ffbf69', label: health || 'running' }
}

export default function ServiceTable({ services }) {
  if (!services?.length) return <p style={{ color: 'var(--text-muted)' }}>No service data.</p>
  return (
    <div style={{ overflowX: 'auto', border: '1px solid var(--border)', borderRadius: 10 }}>
      <table style={{ width: '100%', borderCollapse: 'collapse', minWidth: 760 }}>
        <thead>
          <tr>
            {['Service','Container','Running','Health','Status','Ports'].map(h => (
              <th key={h} style={{ textAlign: 'left', padding: '0.55rem', borderBottom: '1px solid var(--border)', color: 'var(--text-muted)', fontSize: '0.85rem', background: 'var(--panel-muted)' }}>{h}</th>
            ))}
          </tr>
        </thead>
        <tbody>
          {services.map(svc => {
            const { color, label } = dot(svc.health, svc.running)
            return (
              <tr key={svc.key}>
                <td style={{ padding: '0.55rem', borderBottom: '1px solid var(--border)', fontSize: '0.9rem' }}>{svc.key}</td>
                <td style={{ padding: '0.55rem', borderBottom: '1px solid var(--border)', fontSize: '0.85rem', fontFamily: 'monospace' }}>{svc.container}</td>
                <td style={{ padding: '0.55rem', borderBottom: '1px solid var(--border)' }}>
                  <span style={{ display: 'flex', alignItems: 'center', gap: '0.5rem' }}>
                    <span style={{ width: 8, height: 8, borderRadius: '50%', background: svc.running ? '#35de8c' : '#ff6b6b', flexShrink: 0 }} />
                    {svc.running ? 'running' : 'down'}
                  </span>
                </td>
                <td style={{ padding: '0.55rem', borderBottom: '1px solid var(--border)' }}>
                  <span style={{ display: 'flex', alignItems: 'center', gap: '0.5rem' }}>
                    <span style={{ width: 8, height: 8, borderRadius: '50%', background: color, flexShrink: 0 }} />
                    {label}
                  </span>
                </td>
                <td style={{ padding: '0.55rem', borderBottom: '1px solid var(--border)', fontSize: '0.85rem', color: 'var(--text-muted)' }}>{svc.status}</td>
                <td style={{ padding: '0.55rem', borderBottom: '1px solid var(--border)', fontSize: '0.82rem', fontFamily: 'monospace', color: 'var(--text-muted)' }}>{svc.ports}</td>
              </tr>
            )
          })}
        </tbody>
      </table>
    </div>
  )
}