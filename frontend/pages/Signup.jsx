import { useState } from 'react'
import { useNavigate, Link } from 'react-router-dom'
import logo from '../assets/42_pulse_logo.svg'

export default function Signup() {
  const [displayName, setDisplayName] = useState('')
  const [email,       setEmail]       = useState('')
  const [password,    setPassword]    = useState('')
  const [message,     setMessage]     = useState('')
  const [isError,     setIsError]     = useState(false)
  const navigate = useNavigate()

  async function handleSubmit(e) {
    e.preventDefault()
    setMessage('Creating account...')
    setIsError(false)

    const response = await fetch('/api/auth/signup', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json', Accept: 'application/json' },
      body: JSON.stringify({ display_name: displayName, email, password }),
    })

    if (response.status === 409) {
      setMessage('That email is already registered.')
      setIsError(true)
      return
    }
    if (!response.ok) {
      setMessage('Could not create the account. Use a valid email and at least 8 characters.')
      setIsError(true)
      return
    }

    navigate('/')
  }

  return (
    <main style={{ justifyContent: 'center' }}>
      <div style={{ width: 'min(100%, 440px)', margin: '0 auto' }}>
        <section style={{ background: 'var(--panel)', border: '1px solid var(--border)', borderRadius: 8, padding: '1.5rem', display: 'grid', gap: '1rem' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '0.85rem' }}>
            <img src={logo} alt="42 Network Pulse logo" style={{ width: 48, height: 48 }} />
            <div>
              <h1>Sign up</h1>
              <p className="status-text">The first account becomes admin.</p>
            </div>
          </div>

          <form onSubmit={handleSubmit}>
            <label>
              Display name
              <input type="text" value={displayName} onChange={e => setDisplayName(e.target.value)} autoComplete="name" />
            </label>
            <label>
              Email
              <input type="email" value={email} onChange={e => setEmail(e.target.value)} autoComplete="email" required />
            </label>
            <label>
              Password
              <input type="password" value={password} onChange={e => setPassword(e.target.value)} autoComplete="new-password" minLength={8} required />
            </label>
            <button type="submit">Create account</button>
          </form>

          <p style={{ minHeight: '1.25rem', color: isError ? 'var(--danger)' : 'var(--text-muted)', margin: 0 }}>{message}</p>
          <div style={{ display: 'flex', alignItems: 'center', gap: '1rem', flexWrap: 'wrap' }}>
            <Link to="/login" style={{ color: 'var(--accent)', textDecoration: 'none' }}>Log in instead</Link>
          </div>
        </section>
      </div>
    </main>
  )
}
