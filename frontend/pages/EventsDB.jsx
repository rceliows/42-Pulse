import { useState, useEffect, useCallback } from 'react'
import EventFilters from '../components/EventFilters'

const DEFAULT_FILTERS = {
  q:          '',
  campus_id:  '',
  user_id:    '',
  from:       '',
  to:         '',
  event_type: '',
  sort_by:    'event_time',
  sort_dir:   'desc',
  page_size:  50,
}

function fmtDate(value) {
  if (!value) return '—'
  const t = Date.parse(String(value))
  if (!Number.isFinite(t)) return '—'
  return new Date(t).toISOString().slice(0, 19).replace('T', ' ')
}

function localToIso(value) {
  if (!value) return ''
  const d = new Date(value)
  return Number.isFinite(d.getTime()) ? d.toISOString() : ''
}

function escHtml(v) {
  return String(v ?? '').replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;')
}

export default function EventsDB() {
  const [filters,    setFilters]    = useState(DEFAULT_FILTERS)
  const [submitted,  setSubmitted]  = useState(DEFAULT_FILTERS)
  const [page,       setPage]       = useState(1)
  const [data,       setData]       = useState(null)
  const [meta,       setMeta]       = useState(null)
  const [status,     setStatus]     = useState('Loading…')

  // Load meta (event types, totals)
  useEffect(() => {
    fetch('/api/events/db/meta', { headers: { Accept: 'application/json' } })
      .then(r => r.ok ? r.json() : Promise.reject(`HTTP ${r.status}`))
      .then(setMeta)
      .catch(e => setStatus(`Meta error: ${e}`))
  }, [])

  // Load events whenever submitted filters or page changes
  const loadEvents = useCallback(() => {
    const params = new URLSearchParams({ page, page_size: submitted.page_size, sort_by: submitted.sort_by, sort_dir: submitted.sort_dir })
    if (submitted.q)          params.set('q',          submitted.q)
    if (submitted.event_type) params.set('event_type', submitted.event_type)
    if (submitted.campus_id)  params.set('campus_id',  submitted.campus_id)
    if (submitted.user_id)    params.set('user_id',    submitted.user_id)
    const fromIso = localToIso(submitted.from)
    const toIso   = localToIso(submitted.to)
    if (fromIso) params.set('from', fromIso)
    if (toIso)   params.set('to',   toIso)

    setStatus('Loading…')
    fetch(`/api/events/db?${params}`, { headers: { Accept: 'application/json' } })
      .then(r => r.ok ? r.json() : Promise.reject(`HTTP ${r.status}`))
      .then(d => { setData(d); setStatus(`Loaded ${d.items?.length ?? 0} rows`) })
      .catch(e => setStatus(`Error: ${e}`))
  }, [submitted, page])

  useEffect(() => { loadEvents() }, [loadEvents])

  const pagination = data?.pagination || {}
  const totalPages = pagination.total_pages || 1
  const total      = pagination.total || 0

  return (
    <main>
      <section className="panel">
        <div className="panel-header">
          <h2>Filter & Query</h2>
          <div style={{ display: 'flex', gap: '0.5rem', flexWrap: 'wrap' }}>
            <span style={{ border: '1px solid var(--border)', borderRadius: 999, padding: '0.2rem 0.55rem', fontSize: '0.85rem', color: 'var(--text-muted)' }}>
              rows={meta?.summary?.total_rows ?? '…'}
            </span>
            <span style={{ border: '1px solid var(--border)', borderRadius: 999, padding: '0.2rem 0.55rem', fontSize: '0.85rem', color: 'var(--text-muted)' }}>
              {status}
            </span>
          </div>
        </div>
        <EventFilters
          filters={filters}
          onChange={setFilters}
          onSubmit={() => { setPage(1); setSubmitted(filters) }}
          onReset={() => { setFilters(DEFAULT_FILTERS); setSubmitted(DEFAULT_FILTERS); setPage(1) }}
          eventTypes={meta?.event_types || []}
        />
      </section>

      <section className="panel">
        <div className="panel-header">
          <h2>Detector Events</h2>
          <div style={{ display: 'flex', gap: '0.5rem' }}>
            <span style={{ border: '1px solid var(--border)', borderRadius: 999, padding: '0.2rem 0.55rem', fontSize: '0.85rem', color: 'var(--text-muted)' }}>
              page={page}/{totalPages}
            </span>
            <span style={{ border: '1px solid var(--border)', borderRadius: 999, padding: '0.2rem 0.55rem', fontSize: '0.85rem', color: 'var(--text-muted)' }}>
              items={total}
            </span>
          </div>
        </div>

        <div style={{ overflowX: 'auto', border: '1px solid var(--border)', borderRadius: 10 }}>
          <table style={{ width: '100%', borderCollapse: 'collapse', minWidth: 780 }}>
            <thead>
              <tr>
                {['event_time','event_types','User','Campus','Changes'].map(h => (
                  <th key={h} style={{ textAlign: 'left', padding: '0.6rem 0.7rem', borderBottom: '1px solid var(--border)', color: 'var(--text-muted)', fontSize: '0.85rem', background: 'var(--panel-muted)' }}>{h}</th>
                ))}
              </tr>
            </thead>
            <tbody>
              {!data?.items?.length ? (
                <tr><td colSpan={5} style={{ padding: '1.5rem', textAlign: 'center', color: 'var(--text-muted)', fontFamily: 'monospace' }}>No rows.</td></tr>
              ) : data.items.map(row => {
                const types = Array.isArray(row.event_types) ? row.event_types : []
                const changes = Array.isArray(row.changes) ? row.changes : []
                const changesText = changes.map(c => {
                  const path = c?.path || 'unknown'
                  const oldV = c?.old ?? 'null'
                  const newV = c?.new ?? 'null'
                  const delta = typeof oldV === 'number' && typeof newV === 'number'
                    ? ` (delta ${newV - oldV > 0 ? '+' : ''}${newV - oldV})` : ''
                  return `${path}: ${JSON.stringify(oldV)} → ${JSON.stringify(newV)}${delta}`
                }).join('\n')

                return (
                  <tr key={row.id} style={{ borderBottom: '1px solid var(--border)' }}>
                    <td style={{ padding: '0.6rem 0.7rem', fontFamily: 'monospace', fontSize: '0.85rem', verticalAlign: 'top' }}>{fmtDate(row.event_time || row.event_at)}</td>
                    <td style={{ padding: '0.6rem 0.7rem', verticalAlign: 'top' }}>
                      <div style={{ display: 'flex', flexWrap: 'wrap', gap: '0.3rem' }}>
                        {types.map(t => (
                          <span key={t} style={{ border: '1px solid var(--border)', borderRadius: 999, padding: '0.08rem 0.45rem', fontSize: '0.8rem' }}>{t}</span>
                        ))}
                      </div>
                    </td>
                    <td style={{ padding: '0.6rem 0.7rem', fontSize: '0.9rem', verticalAlign: 'top' }}>
                      {row.user_login || ''} <span style={{ fontFamily: 'monospace', color: 'var(--text-muted)' }}>(#{row.user_id ?? '—'})</span>
                    </td>
                    <td style={{ padding: '0.6rem 0.7rem', fontFamily: 'monospace', fontSize: '0.9rem', verticalAlign: 'top' }}>{row.campus_id ?? '—'}</td>
                    <td style={{ padding: '0.6rem 0.7rem', fontFamily: 'monospace', fontSize: '0.82rem', color: 'var(--text-muted)', whiteSpace: 'pre', verticalAlign: 'top' }}>{changesText || '—'}</td>
                  </tr>
                )
              })}
            </tbody>
          </table>
        </div>

        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', flexWrap: 'wrap', gap: '0.8rem' }}>
          <span style={{ fontFamily: 'monospace', color: 'var(--text-muted)', fontSize: '0.9rem' }}>
            {total} rows · page {page} / {totalPages}
          </span>
          <div style={{ display: 'flex', gap: '0.5rem' }}>
            <button className="ghost" onClick={() => setPage(p => Math.max(1, p - 1))} disabled={page <= 1}>Previous</button>
            <button className="ghost" onClick={() => setPage(p => Math.min(totalPages, p + 1))} disabled={page >= totalPages}>Next</button>
          </div>
        </div>
      </section>
    </main>
  )
}