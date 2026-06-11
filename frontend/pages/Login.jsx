import { useState } from 'react'
import { useNavigate, useSearchParams, Link } from 'react-router-dom'
import logo from '../assets/42_pulse_logo.svg'

export default function Login() {
  const [email,    setEmail]    = useState('')
  const [password, setPassword] = useState('')
  const [message,  setMessage]  = useState('')
  const [isError,  setIsError]  = useState(false)
  const navigate = useNavigate()
  const [searchParams] = useSearchParams()

  async function handleSubmit(e) {
    e.preventDefault()
    setMessage('Logging in...')
    setIsError(false)

    const response = await fetch('/api/auth/login', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json', Accept: 'application/json' },
      body: JSON.stringify({ email, password }),
    })

    if (!response.ok) {
      setMessage('Invalid email or password.')
      setIsError(true)
      return
    }

    navigate(searchParams.get('next') || '/')
  }

  return (
    <main style={{ justifyContent: 'center' }}>
      <div style={{ width: 'min(100%, 440px)', margin: '0 auto' }}>
        <section style={{ background: 'var(--panel)', border: '1px solid var(--border)', borderRadius: 8, padding: '1.5rem', display: 'grid', gap: '1rem' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '0.85rem' }}>
            <img src={logo} alt="42 Network Pulse logo" style={{ width: 48, height: 48 }} />
            <div>
              <h1>Log in</h1>
              <p className="status-text">42 Network Pulse</p>
            </div>
          </div>

          <form onSubmit={handleSubmit}>
            <label>
              Email
              <input type="email" value={email} onChange={e => setEmail(e.target.value)} autoComplete="email" required />
            </label>
            <label>
              Password
              <input type="password" value={password} onChange={e => setPassword(e.target.value)} autoComplete="current-password" required />
            </label>
            <button type="submit">Log in</button>
          </form>

          <p style={{ minHeight: '1.25rem', color: isError ? 'var(--danger)' : 'var(--text-muted)', margin: 0 }}>{message}</p>
          <div style={{ display: 'flex', alignItems: 'center', gap: '1rem', flexWrap: 'wrap' }}>
            <Link to="/signup" style={{ color: 'var(--accent)', textDecoration: 'none' }}>Create an account</Link>
          </div>
        </section>
      </div>
    </main>
  )
}
