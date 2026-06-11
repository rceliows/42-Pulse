export default function EventFilters({ filters, onChange, onSubmit, onReset, eventTypes }) {
  const handle = field => e => onChange({ ...filters, [field]: e.target.value })

  return (
    <form onSubmit={e => { e.preventDefault(); onSubmit() }} style={{ display: 'flex', flexDirection: 'column', gap: '1rem' }}>
      <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fit, minmax(180px, 1fr))', gap: '0.8rem' }}>

        <label style={{ display: 'flex', flexDirection: 'column', gap: '0.4rem', color: 'var(--text-muted)', fontSize: '0.9rem' }}>
          Search
          <input type="text" value={filters.q} onChange={handle('q')} placeholder="login, user_id, campus…" />
        </label>

        <label style={{ display: 'flex', flexDirection: 'column', gap: '0.4rem', color: 'var(--text-muted)', fontSize: '0.9rem' }}>
          Campus ID
          <input type="number" value={filters.campus_id} onChange={handle('campus_id')} placeholder="e.g. 21" min="0" />
        </label>

        <label style={{ display: 'flex', flexDirection: 'column', gap: '0.4rem', color: 'var(--text-muted)', fontSize: '0.9rem' }}>
          User ID
          <input type="number" value={filters.user_id} onChange={handle('user_id')} placeholder="e.g. 156043" min="0" />
        </label>

        <label style={{ display: 'flex', flexDirection: 'column', gap: '0.4rem', color: 'var(--text-muted)', fontSize: '0.9rem' }}>
          From (UTC)
          <input type="datetime-local" value={filters.from} onChange={handle('from')} />
        </label>

        <label style={{ display: 'flex', flexDirection: 'column', gap: '0.4rem', color: 'var(--text-muted)', fontSize: '0.9rem' }}>
          To (UTC)
          <input type="datetime-local" value={filters.to} onChange={handle('to')} />
        </label>

        <label style={{ display: 'flex', flexDirection: 'column', gap: '0.4rem', color: 'var(--text-muted)', fontSize: '0.9rem' }}>
          Event type
          <select value={filters.event_type} onChange={handle('event_type')}>
            <option value="">All types</option>
            {(eventTypes || []).map(({ event_type, count }) => (
              <option key={event_type} value={event_type}>{event_type} ({count})</option>
            ))}
          </select>
        </label>

        <label style={{ display: 'flex', flexDirection: 'column', gap: '0.4rem', color: 'var(--text-muted)', fontSize: '0.9rem' }}>
          Sort by
          <select value={filters.sort_by} onChange={handle('sort_by')}>
            <option value="event_time">event_time</option>
            <option value="event_type">Event type</option>
            <option value="user_login">User</option>
            <option value="campus_id">Campus</option>
          </select>
        </label>

        <label style={{ display: 'flex', flexDirection: 'column', gap: '0.4rem', color: 'var(--text-muted)', fontSize: '0.9rem' }}>
          Direction
          <select value={filters.sort_dir} onChange={handle('sort_dir')}>
            <option value="desc">desc</option>
            <option value="asc">asc</option>
          </select>
        </label>

        <label style={{ display: 'flex', flexDirection: 'column', gap: '0.4rem', color: 'var(--text-muted)', fontSize: '0.9rem' }}>
          Page size
          <select value={filters.page_size} onChange={handle('page_size')}>
            {[25, 50, 100, 200].map(n => <option key={n} value={n}>{n}</option>)}
          </select>
        </label>

        <div style={{ display: 'flex', gap: '0.7rem', alignItems: 'flex-end', paddingBottom: '0.05rem' }}>
          <button type="submit">Search</button>
          <button type="button" className="ghost" onClick={onReset}>Reset</button>
        </div>
      </div>
    </form>
  )
}