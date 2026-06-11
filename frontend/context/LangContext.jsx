import { createContext, useContext, useState } from 'react'

const LANG_KEY = '42-pulse.language'
const LANGS = ['en', 'fr', 'es']

const LangContext = createContext({ lang: 'en', setLang: () => {} })

export function LangProvider({ children }) {
  const [lang, setLangState] = useState(() => {
    const saved = localStorage.getItem(LANG_KEY)
    return LANGS.includes(saved) ? saved : 'en'
  })

  function setLang(l) {
    localStorage.setItem(LANG_KEY, l)
    setLangState(l)
  }

  return <LangContext.Provider value={{ lang, setLang }}>{children}</LangContext.Provider>
}

export function useLang() {
  return useContext(LangContext)
}
