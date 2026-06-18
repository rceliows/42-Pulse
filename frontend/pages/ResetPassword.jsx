import { useState } from 'react'
import { useNavigate, useSearchParams, Link } from 'react-router-dom'
import logo from '../assets/42_pulse_logo.svg'

export default function ResetPassword() {
  const [searchParams] = useSearchParams()
  const token = searchParams.get('token') || ''
  const [password, setPassword] = useState('')
  const [message,  setMessage]  = useState('')
  const [isError,  setIsError]  = useState(false)
  const navigate = useNavigate()

  async function handleSubmit(e) {
    e.preventDefault()
    setMessage('Resetting password...')
    setIsError(false)

    const response = await fetch('/api/auth/reset-password', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json', Accept: 'application/json' },
      body: JSON.stringify({ token, new_password: password }),
    })

    if (!response.ok) {
      setMessage('That reset link is invalid or has expired. Request a new one.')
      setIsError(true)
      return
    }

    navigate('/login')
  }

  return (
    <main style={{ justifyContent: 'center' }}>
      <div style={{ width: 'min(100%, 440px)', margin: '0 auto' }}>
        <section style={{ background: 'var(--panel)', border: '1px solid var(--border)', borderRadius: 8, padding: '1.5rem', display: 'grid', gap: '1rem' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '0.85rem' }}>
            <img src={logo} alt="42 Network Pulse logo" style={{ width: 48, height: 48 }} />
            <div>
              <h1>Reset password</h1>
              <p className="status-text">42 Network Pulse</p>
            </div>
          </div>

          {!token ? (
            <p style={{ color: 'var(--danger)' }}>Missing reset token. Use the link from your reset email.</p>
          ) : (
            <form onSubmit={handleSubmit}>
              <label>
                New password
                <input type="password" value={password} onChange={e => setPassword(e.target.value)} autoComplete="new-password" minLength={8} required />
              </label>
              <button type="submit">Reset password</button>
            </form>
          )}

          <p style={{ minHeight: '1.25rem', color: isError ? 'var(--danger)' : 'var(--text-muted)', margin: 0 }}>{message}</p>
          <div style={{ display: 'flex', alignItems: 'center', gap: '1rem', flexWrap: 'wrap' }}>
            <Link to="/login" style={{ color: 'var(--accent)', textDecoration: 'none' }}>Back to log in</Link>
          </div>
        </section>
      </div>
    </main>
  )
}
