import { useState, useEffect, useCallback } from 'react'
import { NavLink, useNavigate, useLocation } from 'react-router-dom'
import logo from '../assets/42_pulse_logo.svg'
import { useLang } from '../context/LangContext'
import { useAuth } from '../hooks/useAuth'

const T = {
  en: { live: 'Live Detector', events: 'Events DB', dashboard: 'DB Dashboard', profile: 'Profile', profileAdmin: 'Profile (Admin)', logout: 'Log out', menu: 'Menu', confirmMsg: 'Are you sure you want to log out?', confirmYes: 'Yes, log out', confirmNo: 'Cancel' },
  fr: { live: 'Détecteur live', events: 'Événements DB', dashboard: 'Tableau de bord', profile: 'Profil', profileAdmin: 'Profil (Admin)', logout: 'Déconnexion', menu: 'Menu', confirmMsg: 'Voulez-vous vraiment vous déconnecter ?', confirmYes: 'Oui, se déconnecter', confirmNo: 'Annuler' },
  es: { live: 'Detector en vivo', events: 'Eventos DB', dashboard: 'Panel DB', profile: 'Perfil', profileAdmin: 'Perfil (Admin)', logout: 'Cerrar sesión', menu: 'Menú', confirmMsg: '¿Seguro que quieres cerrar sesión?', confirmYes: 'Sí, cerrar sesión', confirmNo: 'Cancelar' },
}

async function doLogout(navigate) {
  try {
    await fetch('/api/auth/logout', { method: 'POST', headers: { Accept: 'application/json' } })
  } finally {
    navigate('/login')
  }
}

const navClass = ({ isActive }) => isActive ? 'page-link active' : 'page-link'

function LogoutModal({ t, onCancel, onConfirm }) {
  useEffect(() => {
    const onKey = e => { if (e.key === 'Escape') onCancel() }
    window.addEventListener('keydown', onKey)
    return () => window.removeEventListener('keydown', onKey)
  }, [onCancel])

  return (
    <div
      onClick={onCancel}
      style={{ position: 'fixed', inset: 0, background: 'rgba(0,0,0,0.65)', display: 'flex', alignItems: 'center', justifyContent: 'center', zIndex: 1000, padding: '1rem' }}
    >
      <div
        onClick={e => e.stopPropagation()}
        style={{ background: 'var(--panel)', border: '1px solid var(--border)', borderRadius: 12, padding: '2rem', width: '100%', maxWidth: 360, display: 'flex', flexDirection: 'column', gap: '1.5rem' }}
      >
        <h2 style={{ margin: 0, fontSize: '1.1rem' }}>{t.confirmMsg}</h2>
        <div style={{ display: 'flex', gap: '0.75rem', justifyContent: 'flex-end' }}>
          <button type="button" className="ghost" style={{ padding: '0.55rem 1.25rem' }} onClick={onCancel}>
            {t.confirmNo}
          </button>
          <button type="button" style={{ padding: '0.55rem 1.25rem', background: 'var(--danger)', color: '#fff', border: 'none' }} onClick={onConfirm}>
            {t.confirmYes}
          </button>
        </div>
      </div>
    </div>
  )
}

export default function Header() {
  const { lang } = useLang()
  const navigate = useNavigate()
  const location = useLocation()
  const { isAdmin } = useAuth()
  const t = T[lang] || T.en
  const [menuOpen,      setMenuOpen]      = useState(false)
  const [confirmLogout, setConfirmLogout] = useState(false)

  useEffect(() => { setMenuOpen(false); setConfirmLogout(false) }, [location.pathname])

  const profileTo    = isAdmin ? '/admin' : '/profile'
  const profileLabel = isAdmin ? t.profileAdmin : t.profile
  const close        = useCallback(() => setMenuOpen(false), [])
  const cancelLogout = useCallback(() => setConfirmLogout(false), [])
  const handleLogout = useCallback(() => doLogout(navigate), [navigate])

  return (
    <header>
      <div className="branding">
        <img src={logo} alt="42 Network Pulse" style={{ width: 68, height: 68 }} />
        <div>
          <h1>42 Network Pulse</h1>
          <p className="switch-links header-desktop-nav">
            <NavLink to="/"          className={navClass}>{t.live}</NavLink>
            <span> | </span>
            <NavLink to="/events"    className={navClass}>{t.events}</NavLink>
            <span> | </span>
            <NavLink to="/dashboard" className={navClass}>{t.dashboard}</NavLink>
            <span> | </span>
            <NavLink to={profileTo}  className={navClass}>{profileLabel}</NavLink>
          </p>
        </div>
      </div>

      <div className="header-desktop-actions">
        <button type="button" className="ghost" style={{ padding: '0.45rem 0.7rem', fontSize: '0.85rem' }} onClick={() => setConfirmLogout(true)}>
          {t.logout}
        </button>
      </div>

      <button
        type="button"
        className="hamburger-btn"
        aria-expanded={menuOpen}
        onClick={() => setMenuOpen(o => !o)}
      >
        <span style={{ display: 'flex', alignItems: 'center', gap: '0.5rem' }}>
          <span style={{ fontSize: '1.2rem', lineHeight: 1 }}>≡</span>
          {t.menu}
        </span>
        <span style={{ fontSize: '0.75rem' }}>{menuOpen ? '▲' : '▼'}</span>
      </button>

      {menuOpen && (
        <nav className="mobile-menu">
          <NavLink to="/"          className={navClass} onClick={close}>{t.live}</NavLink>
          <NavLink to="/events"    className={navClass} onClick={close}>{t.events}</NavLink>
          <NavLink to="/dashboard" className={navClass} onClick={close}>{t.dashboard}</NavLink>
          <NavLink to={profileTo}  className={navClass} onClick={close}>{profileLabel}</NavLink>
          <button type="button" className="ghost mobile-logout" onClick={() => { close(); setConfirmLogout(true) }}>
            {t.logout}
          </button>
        </nav>
      )}

      {confirmLogout && (
        <LogoutModal
          t={t}
          onCancel={cancelLogout}
          onConfirm={handleLogout}
        />
      )}
    </header>
  )
}
