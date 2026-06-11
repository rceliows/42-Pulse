import { useState, useEffect, useRef } from 'react'
import { useLang } from '../context/LangContext'

const T = {
  en: {
    changeUsername:  'Change username',
    changeEmail:     'Change email address',
    changePassword:  'Change password',
    oldPassword:     'Current password',
    newUsername:     'New username',
    newPassword:     'New password',
    repeatPassword:  'Repeat new password',
    newEmail:        'New email address',
    repeatEmail:     'Repeat new email',
    save:            'Save',
    saving:          'Saving…',
    cancel:          'Cancel',
    saved:           'Saved successfully.',
    mismatch:        'New passwords do not match.',
    emailMismatch:   'Email addresses do not match.',
    wrongPassword:   'Incorrect current password.',
    emailTaken:      'That email is already in use.',
    usernameTaken:   'That username is already taken.',
    fieldRequired:   'All fields are required.',
    tooShort:        'Password must be at least 8 characters.',
    serverError:     'Something went wrong. Please try again.',
  },
  fr: {
    changeUsername:  'Changer le nom d\'utilisateur',
    changeEmail:     'Changer l\'adresse e-mail',
    changePassword:  'Changer le mot de passe',
    oldPassword:     'Mot de passe actuel',
    newUsername:     'Nouveau nom d\'utilisateur',
    newPassword:     'Nouveau mot de passe',
    repeatPassword:  'Répéter le nouveau mot de passe',
    newEmail:        'Nouvelle adresse e-mail',
    repeatEmail:     'Répéter le nouvel e-mail',
    save:            'Enregistrer',
    saving:          'Enregistrement…',
    cancel:          'Annuler',
    saved:           'Enregistré avec succès.',
    mismatch:        'Les nouveaux mots de passe ne correspondent pas.',
    emailMismatch:   'Les adresses e-mail ne correspondent pas.',
    wrongPassword:   'Mot de passe actuel incorrect.',
    emailTaken:      'Cet e-mail est déjà utilisé.',
    usernameTaken:   'Ce nom d\'utilisateur est déjà pris.',
    fieldRequired:   'Tous les champs sont obligatoires.',
    tooShort:        'Le mot de passe doit comporter au moins 8 caractères.',
    serverError:     'Une erreur est survenue. Veuillez réessayer.',
  },
  es: {
    changeUsername:  'Cambiar nombre de usuario',
    changeEmail:     'Cambiar dirección de correo',
    changePassword:  'Cambiar contraseña',
    oldPassword:     'Contraseña actual',
    newUsername:     'Nuevo nombre de usuario',
    newPassword:     'Nueva contraseña',
    repeatPassword:  'Repetir nueva contraseña',
    newEmail:        'Nueva dirección de correo',
    repeatEmail:     'Repetir nuevo correo',
    save:            'Guardar',
    saving:          'Guardando…',
    cancel:          'Cancelar',
    saved:           'Guardado correctamente.',
    mismatch:        'Las nuevas contraseñas no coinciden.',
    emailMismatch:   'Las direcciones de correo no coinciden.',
    wrongPassword:   'Contraseña actual incorrecta.',
    emailTaken:      'Ese correo ya está registrado.',
    usernameTaken:   'Ese nombre de usuario ya está en uso.',
    fieldRequired:   'Todos los campos son obligatorios.',
    tooShort:        'La contraseña debe tener al menos 8 caracteres.',
    serverError:     'Algo salió mal. Por favor inténtalo de nuevo.',
  },
}

export default function ChangeModal({ mode, onClose }) {
  const { lang } = useLang()
  const t = T[lang] || T.en

  const isEmail    = mode === 'email'
  const isUsername = mode === 'username'

  const [fields, setFields] = useState({ oldPassword: '', a: '', b: '' })
  const [status, setStatus] = useState('idle')
  const [errMsg, setErrMsg] = useState('')
  const firstRef = useRef(null)

  useEffect(() => {
    firstRef.current?.focus()
    const onKey = e => { if (e.key === 'Escape' && status !== 'loading') onClose() }
    window.addEventListener('keydown', onKey)
    return () => window.removeEventListener('keydown', onKey)
  }, [onClose, status])

  const set = key => e => setFields(f => ({ ...f, [key]: e.target.value }))

  async function handleSave() {
    const needsRepeat = !isUsername
    if (!fields.oldPassword || !fields.a || (needsRepeat && !fields.b)) { setStatus('error'); setErrMsg(t.fieldRequired); return }
    if (needsRepeat && fields.a !== fields.b) { setStatus('error'); setErrMsg(isEmail ? t.emailMismatch : t.mismatch); return }
    if (!isEmail && !isUsername && fields.a.length < 8) { setStatus('error'); setErrMsg(t.tooShort); return }

    setStatus('loading')
    setErrMsg('')

    const endpoint = isUsername ? '/api/auth/change-username'
                   : isEmail    ? '/api/auth/change-email'
                                : '/api/auth/change-password'
    const body = isUsername ? { old_password: fields.oldPassword, new_username: fields.a }
               : isEmail    ? { old_password: fields.oldPassword, new_email:    fields.a }
                            : { old_password: fields.oldPassword, new_password: fields.a }

    try {
      const res  = await fetch(endpoint, { method: 'PATCH', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) })
      const data = await res.json()
      if (res.ok) {
        setStatus('success')
        setTimeout(onClose, 1200)
      } else {
        const msg = {
          invalid_old_password:     t.wrongPassword,
          username_already_taken:   t.usernameTaken,
          email_already_registered: t.emailTaken,
          password_too_short:       t.tooShort,
          all_fields_required:      t.fieldRequired,
          valid_email_required:     t.fieldRequired,
        }[data?.error] || t.serverError
        setStatus('error')
        setErrMsg(msg)
      }
    } catch {
      setStatus('error')
      setErrMsg(t.serverError)
    }
  }

  const fieldStyle = {
    width: '100%', padding: '0.6rem 0.75rem', borderRadius: 8,
    border: '1px solid var(--border)', background: 'var(--panel-muted)',
    color: 'var(--text)', fontSize: '1rem',
  }
  const labelStyle = { display: 'flex', flexDirection: 'column', gap: '0.35rem', fontSize: '0.9rem', color: 'var(--text-muted)' }

  return (
    <div
      onClick={() => status !== 'loading' && onClose()}
      style={{ position: 'fixed', inset: 0, background: 'rgba(0,0,0,0.65)', display: 'flex', alignItems: 'center', justifyContent: 'center', zIndex: 1000, padding: '1rem' }}
    >
      <div
        onClick={e => e.stopPropagation()}
        style={{ background: 'var(--panel)', border: '1px solid var(--border)', borderRadius: 12, padding: '2rem', width: '100%', maxWidth: 420, display: 'flex', flexDirection: 'column', gap: '1.25rem' }}
      >
        <h2 style={{ margin: 0, fontSize: '1.1rem' }}>
          {isUsername ? t.changeUsername : isEmail ? t.changeEmail : t.changePassword}
        </h2>

        <label style={labelStyle}>
          {t.oldPassword}
          <input ref={firstRef} type="password" value={fields.oldPassword} onChange={set('oldPassword')} style={fieldStyle} autoComplete="current-password" disabled={status === 'loading'} />
        </label>

        <label style={labelStyle}>
          {isUsername ? t.newUsername : isEmail ? t.newEmail : t.newPassword}
          <input type={isEmail ? 'email' : isUsername ? 'text' : 'password'} value={fields.a} onChange={set('a')} style={fieldStyle} autoComplete={isEmail ? 'email' : isUsername ? 'username' : 'new-password'} disabled={status === 'loading'} />
        </label>

        {!isUsername && (
          <label style={labelStyle}>
            {isEmail ? t.repeatEmail : t.repeatPassword}
            <input type={isEmail ? 'email' : 'password'} value={fields.b} onChange={set('b')} style={fieldStyle} autoComplete={isEmail ? 'email' : 'new-password'} disabled={status === 'loading'} />
          </label>
        )}

        {status === 'error'   && <p style={{ margin: 0, color: 'var(--danger)', fontSize: '0.9rem' }}>{errMsg}</p>}
        {status === 'success' && <p style={{ margin: 0, color: '#35de8c',       fontSize: '0.9rem' }}>{t.saved}</p>}

        <div style={{ display: 'flex', gap: '0.75rem', justifyContent: 'flex-end' }}>
          <button className="ghost" type="button" onClick={onClose} disabled={status === 'loading'} style={{ padding: '0.55rem 1.25rem' }}>{t.cancel}</button>
          <button type="button" onClick={handleSave} disabled={status === 'loading' || status === 'success'} style={{ padding: '0.55rem 1.25rem' }}>
            {status === 'loading' ? t.saving : t.save}
          </button>
        </div>
      </div>
    </div>
  )
}
