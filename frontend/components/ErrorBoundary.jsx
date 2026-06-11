import { Component } from 'react'

export default class ErrorBoundary extends Component {
  constructor(props) {
    super(props)
    this.state = { error: null }
  }

  static getDerivedStateFromError(error) {
    return { error }
  }

  componentDidCatch(error, info) {
    console.error('Unhandled render error:', error, info.componentStack)
  }

  render() {
    if (this.state.error) {
      return (
        <main style={{ padding: '2rem', color: 'var(--danger)' }}>
          <h2>Something went wrong</h2>
          <p style={{ fontFamily: 'monospace', fontSize: '0.85rem' }}>
            {this.state.error.message}
          </p>
          <button type="button" onClick={() => this.setState({ error: null })}>
            Try again
          </button>
        </main>
      )
    }
    return this.props.children
  }
}
