import { useState } from 'react'
import { Link } from 'react-router-dom'
import logo from '../assets/42_pulse_logo.svg'

export default function ForgotPassword() {
  const [email,   setEmail]   = useState('')
  const [message, setMessage] = useState('')
  const [isError, setIsError] = useState(false)
  const [sent,    setSent]    = useState(false)

  async function handleSubmit(e) {
    e.preventDefault()
    setMessage('Sending...')
    setIsError(false)

    const response = await fetch('/api/auth/forgot-password', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json', Accept: 'application/json' },
      body: JSON.stringify({ email }),
    })

    if (!response.ok) {
      setMessage('Something went wrong. Please try again.')
      setIsError(true)
      return
    }

    setSent(true)
    setMessage('If an account exists for that email, a password reset link has been sent.')
  }

  return (
    <main style={{ justifyContent: 'center' }}>
      <div style={{ width: 'min(100%, 440px)', margin: '0 auto' }}>
        <section style={{ background: 'var(--panel)', border: '1px solid var(--border)', borderRadius: 8, padding: '1.5rem', display: 'grid', gap: '1rem' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '0.85rem' }}>
            <img src={logo} alt="42 Network Pulse logo" style={{ width: 48, height: 48 }} />
            <div>
              <h1>Forgot password</h1>
              <p className="status-text">42 Network Pulse</p>
            </div>
          </div>

          {!sent && (
            <form onSubmit={handleSubmit}>
              <label>
                Email
                <input type="email" value={email} onChange={e => setEmail(e.target.value)} autoComplete="email" required />
              </label>
              <button type="submit">Send reset link</button>
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
