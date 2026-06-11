import { useNavigate } from 'react-router-dom'
import { useLang } from '../context/LangContext'

const T = {
  en: { heading: '404 — Page not found', body: "The page you're looking for doesn't exist or was moved.", back: '← Go back', home: 'Go to home' },
  fr: { heading: '404 — Page introuvable', body: 'La page que vous cherchez n\'existe pas ou a été déplacée.', back: '← Retour', home: 'Accueil' },
  es: { heading: '404 — Página no encontrada', body: 'La página que buscas no existe o fue movida.', back: '← Volver', home: 'Ir al inicio' },
}

export default function NotFound() {
  const { lang } = useLang()
  const t = T[lang] || T.en
  const navigate = useNavigate()

  return (
    <main style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', flex: 1 }}>
      <div className="panel" style={{ maxWidth: 480, width: '100%', textAlign: 'center', gap: '1.25rem' }}>
        <div style={{ fontSize: '4rem', lineHeight: 1, color: 'var(--text-muted)' }}>404</div>
        <h2 style={{ margin: 0 }}>{t.heading}</h2>
        <p style={{ margin: 0, color: 'var(--text-muted)' }}>{t.body}</p>
        <div style={{ display: 'flex', gap: '0.75rem', justifyContent: 'center', flexWrap: 'wrap' }}>
          <button className="ghost" type="button" onClick={() => navigate(-1)} style={{ padding: '0.55rem 1.25rem' }}>
            {t.back}
          </button>
          <button type="button" onClick={() => navigate('/')} style={{ padding: '0.55rem 1.25rem' }}>
            {t.home}
          </button>
        </div>
      </div>
    </main>
  )
}
