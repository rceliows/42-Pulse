import { useEffect, useState, useCallback } from 'react'
import { useNavigate, useLocation } from 'react-router-dom'
import { useAdminStatus } from '../hooks/useAdminStatus'
import { useAuth } from '../hooks/useAuth'
import { useLang } from '../context/LangContext'
import ServiceTable  from '../components/ServiceTable'
import QueueTable    from '../components/QueueTable'
import ChangeModal   from '../components/ChangeModal'

const T = {
  en: {
    health:         'Global System Health',
    overallStatus:  'Overall status',
    healthReason:   'Health reason',
    api42:          'API 42 Health',
    resources:      'System Resources',
    queues:         'Queues (Internal / External / Process)',
    services:       'Microservices Status',
    backup:         'Last Backup',
    drInfo:         'DR Info',
    security:        'Security',
    changeUsername:  'Change username',
    changeEmail:     'Change email address',
    changePassword:  'Change password',
    cpuCount:       'CPU count',
    loadAvg:        'Load avg (1m,5m,15m)',
    memUsed:        'Memory used / total',
    diskUsed:       'Disk used (%)',
    diskFree:       'Disk free (MB)',
    dbSize:         'DB size (MB)',
    status:         'Status',
    timestamp:      'Timestamp (UTC)',
    backupAge:      'Backup age',
    file:           'File',
    size:           'Size',
    duration:       'Duration (ms)',
    retention:      'Retention (days)',
    pruned:         'Pruned files',
    error:          'Error',
    runbook:        'Runbook',
    restoreCmd:     'Restore command',
    loading:        'Loading…',
    loadingDot:     'Loading...',
    lastPoll:       'last poll',
    code:           'code',
    latency:        'latency',
    apiError:       'Error',
  },
  fr: {
    health:         'Santé globale du système',
    overallStatus:  'Statut global',
    healthReason:   'Raison de santé',
    api42:          'Santé API 42',
    resources:      'Ressources système',
    queues:         'Files d\'attente (Interne / Externe / Processus)',
    services:       'Statut des microservices',
    backup:         'Dernière sauvegarde',
    drInfo:         'Info DR',
    security:        'Sécurité',
    changeUsername:  'Changer le nom d\'utilisateur',
    changeEmail:     'Changer l\'adresse e-mail',
    changePassword:  'Changer le mot de passe',
    cpuCount:       'Nombre de CPU',
    loadAvg:        'Charge moy. (1m,5m,15m)',
    memUsed:        'Mémoire utilisée / totale',
    diskUsed:       'Disque utilisé (%)',
    diskFree:       'Disque libre (MB)',
    dbSize:         'Taille BD (MB)',
    status:         'Statut',
    timestamp:      'Horodatage (UTC)',
    backupAge:      'Âge de la sauvegarde',
    file:           'Fichier',
    size:           'Taille',
    duration:       'Durée (ms)',
    retention:      'Rétention (jours)',
    pruned:         'Fichiers élagués',
    error:          'Erreur',
    runbook:        'Runbook',
    restoreCmd:     'Commande de restauration',
    loading:        'Chargement…',
    loadingDot:     'Chargement...',
    lastPoll:       'dernier sondage',
    code:           'code',
    latency:        'latence',
    apiError:       'Erreur',
  },
  es: {
    health:         'Salud global del sistema',
    overallStatus:  'Estado general',
    healthReason:   'Razón de salud',
    api42:          'Salud API 42',
    resources:      'Recursos del sistema',
    queues:         'Colas (Interna / Externa / Proceso)',
    services:       'Estado de los microservicios',
    backup:         'Último respaldo',
    drInfo:         'Info DR',
    security:        'Seguridad',
    changeUsername:  'Cambiar nombre de usuario',
    changeEmail:     'Cambiar dirección de correo',
    changePassword:  'Cambiar contraseña',
    cpuCount:       'Núcleos CPU',
    loadAvg:        'Carga prom. (1m,5m,15m)',
    memUsed:        'Memoria usada / total',
    diskUsed:       'Disco usado (%)',
    diskFree:       'Disco libre (MB)',
    dbSize:         'Tamaño BD (MB)',
    status:         'Estado',
    timestamp:      'Marca de tiempo (UTC)',
    backupAge:      'Antigüedad del respaldo',
    file:           'Archivo',
    size:           'Tamaño',
    duration:       'Duración (ms)',
    retention:      'Retención (días)',
    pruned:         'Archivos eliminados',
    error:          'Error',
    runbook:        'Runbook',
    restoreCmd:     'Comando de restauración',
    loading:        'Cargando…',
    loadingDot:     'Cargando...',
    lastPoll:       'último sondeo',
    code:           'código',
    latency:        'latencia',
    apiError:       'Error',
  },
}

function formatBytes(bytes) {
  const n = Number(bytes)
  if (!Number.isFinite(n) || n < 0) return '—'
  if (n < 1024) return `${n} B`
  const units = ['KB', 'MB', 'GB', 'TB']
  let value = n / 1024, unit = units[0]
  for (let i = 1; i < units.length && value >= 1024; i++) { value /= 1024; unit = units[i] }
  return `${value.toFixed(2)} ${unit}`
}

function formatAgeSeconds(value) {
  const s = Number(value)
  if (!Number.isFinite(s) || s < 0) return '—'
  if (s < 60) return `${Math.floor(s)} s`
  const m = Math.floor(s / 60)
  if (m < 60) return `${m} min`
  const h = Math.floor(m / 60), rm = m % 60
  if (h < 24) return rm > 0 ? `${h} h ${rm} min` : `${h} h`
  const d = Math.floor(h / 24), rh = h % 24
  return rh > 0 ? `${d} d ${rh} h` : `${d} d`
}

function parseApi42Status(status) {
  const parts = String(status || '').trim().split(/\s+/)
  const state = parts[0] || 'unknown'
  let code = null, latency = null
  for (let i = 0; i < parts.length; i++) {
    if (/^\d{3}$/.test(parts[i])) {
      code = Number(parts[i])
      const maybeLatency = Number(parts[i + 1])
      if (Number.isFinite(maybeLatency)) latency = maybeLatency
      break
    }
  }
  return { state, code, latency }
}

function overallClass(status) {
  if (status === 'healthy')  return 'ok'
  if (status === 'warning')  return 'warn'
  if (status === 'degraded') return 'bad'
  return ''
}

const DOT_COLOR = { ok: '#35de8c', warn: '#ffbf69', bad: '#ff6b6b' }

function Dot({ cls }) {
  return (
    <span style={{ width: 8, height: 8, borderRadius: '50%', background: DOT_COLOR[cls] || 'var(--text-muted)', display: 'inline-block', flexShrink: 0 }} />
  )
}

function InfoItem({ label, value }) {
  return (
    <div style={{ border: '1px solid var(--border)', borderRadius: 8, padding: '0.75rem', background: 'var(--panel-muted)' }}>
      <div style={{ color: 'var(--text-muted)', fontSize: '0.85rem' }}>{label}</div>
      <div style={{ fontFamily: 'monospace', fontSize: '0.95rem', marginTop: '0.3rem', overflowWrap: 'anywhere' }}>
        {value === null || value === undefined || value === '' ? '—' : String(value)}
      </div>
    </div>
  )
}

function Badge({ children, cls }) {
  const color = DOT_COLOR[cls]
  return (
    <span style={{
      display: 'inline-flex', alignItems: 'center',
      border: `1px solid ${color ? `${color}77` : 'var(--border)'}`,
      borderRadius: 999, padding: '0.2rem 0.6rem',
      fontSize: '0.85rem', color: color || 'var(--text-muted)',
      background: 'var(--panel-muted)',
    }}>
      {children}
    </span>
  )
}


export default function Admin() {
  const { status: authStatus, isAdmin } = useAuth()
  const { data, error, unauthorized } = useAdminStatus()
  const navigate  = useNavigate()
  const location  = useLocation()
  const [lastPoll, setLastPoll] = useState(null)
  const [secModal, setSecModal] = useState(null)
  const handleSecModalClose = useCallback(() => setSecModal(null), [])
  const { lang } = useLang()
  const t = T[lang] || T.en

  useEffect(() => {
    if (authStatus === 'unauthenticated' || unauthorized) {
      navigate(`/login?next=${encodeURIComponent(location.pathname)}`)
    } else if (authStatus === 'authenticated' && !isAdmin) {
      navigate('/profile', { replace: true })
    }
  }, [authStatus, isAdmin, unauthorized, navigate, location.pathname])

  useEffect(() => {
    if (data) setLastPoll(new Date().toLocaleTimeString())
  }, [data])

  if (authStatus === 'loading') return <main><p style={{ color: 'var(--text-muted)', padding: '1rem' }}>{t.loading}</p></main>
  if (authStatus === 'unauthenticated' || unauthorized || !isAdmin) return null
  if (error) return <main><p style={{ color: 'var(--danger)', padding: '1rem' }}>{t.apiError}: {error}</p></main>
  if (!data)  return <main><p style={{ color: 'var(--text-muted)', padding: '1rem' }}>{t.loading}</p></main>

  const g   = data.global_system_health || {}
  const r   = data.system_resources     || {}
  const a42 = data.api42_health         || {}
  const b   = data.last_backup          || {}
  const dr  = data.dr_info              || {}

  const overall   = g.overall_status || 'unknown'
  const overCls   = overallClass(overall)
  const a42Parsed = parseApi42Status(a42.status || 'unknown')
  const loadAvg   = `${Number(r.loadavg_1  || 0).toFixed(2)}, ${Number(r.loadavg_5  || 0).toFixed(2)}, ${Number(r.loadavg_15 || 0).toFixed(2)}`
  const diskUsed  = Number(r.disk_used_percent)
  const dbSizeMb  = r.db_size_bytes != null ? (Number(r.db_size_bytes) / (1024 * 1024)).toFixed(2) : '—'

  return (
    <main>
      <div style={{ display: 'grid', gap: '1rem', gridTemplateColumns: 'repeat(auto-fit, minmax(260px, 1fr))' }}>

        <article className="panel">
          <h2>{t.health}</h2>
          <div style={{ display: 'grid', gap: '0.75rem' }}>
            <div style={{ border: '1px solid var(--border)', borderRadius: 8, padding: '0.75rem', background: 'var(--panel-muted)' }}>
              <div style={{ color: 'var(--text-muted)', fontSize: '0.85rem' }}>{t.overallStatus}</div>
              <div style={{ marginTop: '0.3rem' }}><Badge cls={overCls}>{overall}</Badge></div>
            </div>
            <InfoItem label={t.healthReason} value={g.overall_reason} />
            <div style={{ border: '1px solid var(--border)', borderRadius: 8, padding: '0.75rem', background: 'var(--panel-muted)' }}>
              <div style={{ color: 'var(--text-muted)', fontSize: '0.85rem' }}>{t.api42}</div>
              <div style={{ display: 'flex', gap: '0.5rem', flexWrap: 'wrap', marginTop: '0.5rem' }}>
                <Badge>{a42Parsed.state}</Badge>
                <Badge>{t.code}: {a42Parsed.code ?? '—'}</Badge>
                <Badge>{t.latency}: {a42Parsed.latency != null ? `${a42Parsed.latency.toFixed(3)}s` : '—'}</Badge>
              </div>
            </div>
          </div>
        </article>

        <article className="panel">
          <h2>{t.resources}</h2>
          <div style={{ display: 'grid', gap: '0.75rem', gridTemplateColumns: 'repeat(2, minmax(0, 1fr))' }}>
            <InfoItem label={t.cpuCount}  value={r.cpu_count} />
            <InfoItem label={t.loadAvg}   value={loadAvg} />
            <InfoItem label={t.memUsed}   value={`${formatBytes(r.memory_used_bytes)} / ${formatBytes(r.memory_total_bytes)}`} />
            <InfoItem label={t.diskUsed}  value={Number.isFinite(diskUsed) ? diskUsed.toFixed(1) : '—'} />
            <InfoItem label={t.diskFree}  value={r.disk_free_mb} />
            <InfoItem label={t.dbSize}    value={dbSizeMb} />
          </div>
        </article>

        <article className="panel">
          <h2>{t.queues}</h2>
          <QueueTable queues={data.queues_status} thresholds={data.queue_thresholds} />
        </article>
      </div>

      <section className="panel">
        <h2>{t.services}</h2>
        <ServiceTable services={data.microservices_status?.services ?? []} />
      </section>

      <div style={{ display: 'grid', gap: '1rem', gridTemplateColumns: '2fr 1fr 1fr' }}>

        <article className="panel">
          <h2>{t.backup}</h2>
          <div style={{ display: 'grid', gap: '0.75rem', gridTemplateColumns: 'repeat(2, minmax(0, 1fr))' }}>
            <InfoItem label={t.status}    value={b.status} />
            <InfoItem label={t.timestamp} value={b.timestamp_utc} />
            <InfoItem label={t.backupAge} value={formatAgeSeconds(b.age_seconds)} />
            <InfoItem label={t.file}      value={b.backup_file} />
            <InfoItem label={t.size}      value={formatBytes(b.size_bytes)} />
            <InfoItem label={t.duration}  value={b.duration_ms} />
            <InfoItem label={t.retention} value={b.retention_days} />
            <InfoItem label={t.pruned}    value={b.pruned_count} />
            <InfoItem label={t.error}     value={b.error || 'none'} />
          </div>
        </article>

        <article className="panel">
          <h2>{t.drInfo}</h2>
          <div style={{ display: 'grid', gap: '0.75rem' }}>
            <InfoItem label={t.runbook}    value={dr.runbook} />
            <InfoItem label={t.restoreCmd} value={dr.restore_command} />
          </div>
        </article>

        <article className="panel" style={{ display: 'flex', flexDirection: 'column', gap: '0.75rem' }}>
          <h2>{t.security}</h2>
          {[
            ['username', t.changeUsername],
            ['email',    t.changeEmail],
            ['password', t.changePassword],
          ].map(([m, label]) => (
            <button key={m} type="button" onClick={() => setSecModal(m)} style={{ width: '100%', textAlign: 'left', padding: '0.7rem 0.9rem', borderRadius: 8, background: 'var(--panel-muted)', border: '1px solid var(--border)', color: 'var(--text)', fontWeight: 500, cursor: 'pointer' }}>
              {label}
            </button>
          ))}
        </article>
      </div>

      <div style={{ display: 'flex', alignItems: 'center', gap: '0.65rem', color: 'var(--text-muted)', fontSize: '0.85rem' }}>
        <Dot cls={overCls} />
        <span>{lastPoll ? `${t.lastPoll}: ${lastPoll}` : t.loadingDot}</span>
      </div>

      {secModal && <ChangeModal mode={secModal} onClose={handleSecModalClose} />}
    </main>
  )
}
