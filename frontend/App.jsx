import { BrowserRouter, Routes, Route, Link } from 'react-router-dom'
import { LangProvider, useLang } from './context/LangContext'
import { AuthProvider } from './context/AuthContext'
import ErrorBoundary  from './components/ErrorBoundary.jsx'
import Header         from './components/Header.jsx'
import ProtectedRoute from './components/ProtectedRoute.jsx'
import Terms          from './pages/Terms.jsx'
import Privacy        from './pages/Privacy.jsx'
import LiveDetector   from './pages/LiveDetector.jsx'
import Admin          from './pages/Admin.jsx'
import Profile        from './pages/Profile.jsx'
import EventsDB       from './pages/EventsDB.jsx'
import Dashboard      from './pages/Dashboard.jsx'
import Login          from './pages/Login.jsx'
import Signup         from './pages/Signup.jsx'
import ForgotPassword from './pages/ForgotPassword.jsx'
import ResetPassword  from './pages/ResetPassword.jsx'
import NotFound       from './pages/NotFound.jsx'

function Footer() {
  const { lang, setLang } = useLang()
  return (
    <footer style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', padding: '1rem 1.5rem' }}>
      <label className="inline" style={{ fontSize: '0.85rem' }}>
        Language
        <select value={lang} onChange={e => setLang(e.target.value)} style={{ minWidth: 96, padding: '0.25rem 0.55rem' }}>
          <option value="en">English</option>
          <option value="fr">Français</option>
          <option value="es">Español</option>
        </select>
      </label>
      <div>
        <Link to="/terms">Terms</Link>
        {' · '}
        <Link to="/privacy">Privacy Policy</Link>
      </div>
    </footer>
  )
}

function Layout({ children }) {
  return (
    <>
      <Header />
      {children}
      <Footer />
    </>
  )
}

export default function App() {
  return (
    <ErrorBoundary>
    <AuthProvider>
    <LangProvider>
      <BrowserRouter>
        <Routes>
          <Route path="/login"   element={<Login />} />
          <Route path="/signup"  element={<Signup />} />
          <Route path="/forgot-password" element={<ForgotPassword />} />
          <Route path="/reset-password"  element={<ResetPassword />} />
          <Route path="/terms"   element={<Layout><Terms /></Layout>} />
          <Route path="/privacy" element={<Layout><Privacy /></Layout>} />

          <Route path="/" element={
            <ProtectedRoute><Layout><LiveDetector /></Layout></ProtectedRoute>
          } />
          <Route path="/admin" element={
            <ProtectedRoute><Layout><Admin /></Layout></ProtectedRoute>
          } />
          <Route path="/profile" element={
            <ProtectedRoute><Layout><Profile /></Layout></ProtectedRoute>
          } />
          <Route path="/events" element={
            <ProtectedRoute><Layout><EventsDB /></Layout></ProtectedRoute>
          } />
          <Route path="/dashboard" element={
            <ProtectedRoute><Layout><Dashboard /></Layout></ProtectedRoute>
          } />

          <Route path="*" element={<Layout><NotFound /></Layout>} />
        </Routes>
      </BrowserRouter>
    </LangProvider>
    </AuthProvider>
    </ErrorBoundary>
  )
}
