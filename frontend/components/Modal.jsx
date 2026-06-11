import { useEffect } from 'react'

export default function Modal({ onClose, children }) {
  useEffect(() => {
    const onKey = e => { if (e.key === 'Escape') onClose() }
    window.addEventListener('keydown', onKey)
    return () => window.removeEventListener('keydown', onKey)
  }, [onClose])

  return (
    <div
      onClick={onClose}
      style={{
        position: 'fixed', inset: 0,
        background: 'rgba(0,0,0,0.6)',
        display: 'flex', alignItems: 'flex-start', justifyContent: 'center',
        zIndex: 1000,
        overflowY: 'auto',
        padding: '2rem 1rem',
      }}
    >
      <div
        onClick={e => e.stopPropagation()}
        style={{
          background: 'var(--bg, #1a1a2e)',
          border: '1px solid var(--border, #333)',
          borderRadius: 8,
          maxWidth: 980,
          width: '100%',
          position: 'relative',
          padding: '2rem',
        }}
      >
        <button
          onClick={onClose}
          aria-label="Close"
          style={{
            position: 'absolute', top: '1rem', right: '1rem',
            background: 'none', border: 'none',
            fontSize: '1.25rem', lineHeight: 1,
            cursor: 'pointer', color: 'var(--text-muted, #aaa)',
            padding: '0.25rem 0.5rem',
          }}
        >✕</button>
        {children}
      </div>
    </div>
  )
}
