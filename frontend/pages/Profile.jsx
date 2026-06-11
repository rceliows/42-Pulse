import { useState, useCallback } from 'react'
import { useLang } from '../context/LangContext'
import ChangeModal from '../components/ChangeModal'

const T = {
  en: { security: 'Security', changeUsername: 'Change username', changeEmail: 'Change email address', changePassword: 'Change password' },
  fr: { security: 'Sécurité', changeUsername: 'Changer le nom d\'utilisateur', changeEmail: 'Changer l\'adresse e-mail', changePassword: 'Changer le mot de passe' },
  es: { security: 'Seguridad', changeUsername: 'Cambiar nombre de usuario', changeEmail: 'Cambiar dirección de correo', changePassword: 'Cambiar contraseña' },
}

const btnStyle = {
  width: '100%', textAlign: 'left', padding: '0.85rem 1rem', borderRadius: 8,
  background: 'var(--panel-muted)', border: '1px solid var(--border)',
  color: 'var(--text)', fontWeight: 500, cursor: 'pointer', fontSize: '1rem',
}

export default function Profile() {
  const { lang } = useLang()
  const t = T[lang] || T.en
  const [modal, setModal] = useState(null)
  const handleClose = useCallback(() => setModal(null), [])

  return (
    <main>
      <article className="panel">
        <h2>{t.security}</h2>
        <div style={{ display: 'flex', flexDirection: 'column', gap: '0.75rem' }}>
          {[
            ['username', t.changeUsername],
            ['email',    t.changeEmail],
            ['password', t.changePassword],
          ].map(([m, label]) => (
            <button key={m} type="button" style={btnStyle} onClick={() => setModal(m)}>
              {label}
            </button>
          ))}
        </div>
      </article>

      {modal && <ChangeModal mode={modal} onClose={handleClose} />}
    </main>
  )
}
