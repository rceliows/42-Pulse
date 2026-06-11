import { campusParts, countryFlagSvg, COUNTRY_TO_ISO } from '../utils/campusInfo'

const FLAG_VERSION = '20251220b'

export default function CampusTable({ rows, grouping }) {
  if (!rows?.length) return <p style={{ color: 'var(--text-muted)', padding: '0.5rem' }}>No data for current window.</p>

  const isCampus = grouping !== 'country'

  return (
    <div style={{ overflowX: 'auto', border: '1px solid var(--border)', borderRadius: 10 }}>
      <table style={{ width: '100%', borderCollapse: 'collapse', minWidth: 560 }}>
        <thead>
          <tr>
            {['#', isCampus ? 'Campus ID' : 'ISO', isCampus ? 'Campus' : 'Country', 'Pulses', 'Unique users', 'Share (%)'].map(h => (
              <th key={h} style={{ textAlign: 'left', padding: '0.55rem 0.65rem', borderBottom: '1px solid var(--border)', color: 'var(--text-muted)', fontSize: '0.84rem', background: 'var(--panel-muted)' }}>{h}</th>
            ))}
          </tr>
        </thead>
        <tbody>
          {rows.map(row => {
            let idLabel, nameLabel, flagSvg

            if (isCampus) {
              idLabel  = row.campus_id ?? '?'
              const { label, flagSvg: fs } = campusParts(row.campus_id)
              nameLabel = label
              flagSvg   = fs
            } else {
              const country = row.country || 'Unknown'
              idLabel   = COUNTRY_TO_ISO[country] || '—'
              nameLabel = country
              flagSvg   = countryFlagSvg(country)
            }

            return (
              <tr key={isCampus ? row.campus_id : row.country} style={{ borderBottom: '1px solid var(--border)' }}>
                <td style={{ padding: '0.55rem 0.65rem', fontFamily: 'monospace', fontSize: '0.9rem' }}>{row.rank}</td>
                <td style={{ padding: '0.55rem 0.65rem', fontFamily: 'monospace', fontSize: '0.9rem' }}>{String(idLabel)}</td>
                <td style={{ padding: '0.55rem 0.65rem' }}>
                  <span style={{ display: 'flex', alignItems: 'center', gap: '0.45rem' }}>
                    <span>{nameLabel}</span>
                    {flagSvg && (
                      <img
                        src={`/assets/twemoji/svg/${flagSvg}.svg?v=${FLAG_VERSION}`}
                        alt=""
                        loading="lazy"
                        style={{ width: 18, height: 18, flexShrink: 0 }}
                      />
                    )}
                  </span>
                </td>
                <td style={{ padding: '0.55rem 0.65rem', fontFamily: 'monospace' }}>{Number(row.pulses || 0).toLocaleString('en-US')}</td>
                <td style={{ padding: '0.55rem 0.65rem', fontFamily: 'monospace' }}>{Number(row.unique_users || 0).toLocaleString('en-US')}</td>
                <td style={{ padding: '0.55rem 0.65rem', fontFamily: 'monospace' }}>{Number(row.share_pct || 0).toFixed(2)}</td>
              </tr>
            )
          })}
        </tbody>
      </table>
    </div>
  )
}