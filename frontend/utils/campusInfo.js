export const CAMPUS_INFO = {
  1:  { name: '42 Paris',            country: 'France' },
  9:  { name: '42 Lyon',             country: 'France' },
  12: { name: '42 Brussels',         country: 'Belgium' },
  13: { name: '42 Helsinki',         country: 'Finland' },
  14: { name: 'Codam',               country: 'Netherlands' },
  16: { name: '1337 Khouribga',      country: 'Morocco' },
  20: { name: '42 São Paulo',        country: 'Brazil' },
  21: { name: '1337 Ben Guérir',     country: 'Morocco' },
  22: { name: '42 Madrid',           country: 'Spain' },
  25: { name: '42 Québec',           country: 'Canada' },
  26: { name: '42 Tokyo',            country: 'Japan' },
  28: { name: '42 Rio',              country: 'Brazil' },
  29: { name: '42 Seoul',            country: 'Korea, Republic of' },
  30: { name: '42 Roma',             country: 'Italy' },
  31: { name: '42 Angoulême',        country: 'France' },
  32: { name: '42 Yerevan',          country: 'Armenia' },
  33: { name: '42 Bangkok',          country: 'Thailand' },
  34: { name: '42 Kuala Lumpur',     country: 'Malaysia' },
  35: { name: '42 Amman',            country: 'Jordan' },
  36: { name: '42 Adelaide',         country: 'Australia' },
  37: { name: '42 Málaga',           country: 'Spain' },
  38: { name: '42 Lisboa',           country: 'Portugal' },
  39: { name: '42 Heilbronn',        country: 'Germany' },
  40: { name: '42 Urduliz',          country: 'Spain' },
  41: { name: '42 Nice',             country: 'France' },
  43: { name: '42 Abu Dhabi',        country: 'United Arab Emirates' },
  44: { name: '42 Wolfsburg',        country: 'Germany' },
  46: { name: '42 Barcelona',        country: 'Spain' },
  47: { name: '42 Lausanne',         country: 'Switzerland' },
  48: { name: '42 Mulhouse',         country: 'France' },
  49: { name: '42 Istanbul',         country: 'Turkey' },
  50: { name: '42 Kocaeli',          country: 'Turkey' },
  51: { name: '42 Berlin',           country: 'Germany' },
  52: { name: '42 Firenze',          country: 'Italy' },
  53: { name: '42 Vienna',           country: 'Austria' },
  55: { name: '1337 Med',            country: 'Morocco' },
  56: { name: '42 Prague',           country: 'Czech Republic' },
  57: { name: '42 London',           country: 'United Kingdom' },
  58: { name: '42 Porto',            country: 'Portugal' },
  59: { name: '42 Luxembourg',       country: 'Luxembourg' },
  60: { name: '42 Perpignan',        country: 'France' },
  61: { name: '42 Belo Horizonte',   country: 'Brazil' },
  62: { name: '42 Le Havre',         country: 'France' },
  64: { name: '42 Singapore',        country: 'Singapore' },
  65: { name: '42 Antananarivo',     country: 'Madagascar' },
  67: { name: '42 Warsaw',           country: 'Poland' },
  68: { name: '42 Luanda',           country: 'Angola' },
  69: { name: '42 Gyeongsan',        country: 'Korea, Republic of' },
  70: { name: '42 Nablus',           country: 'Palestine, State of' },
  71: { name: '42 Beirut',           country: 'Lebanon' },
  72: { name: '42 Milano',           country: 'Italy' },
  73: { name: '42 Iskandar Puteri',  country: 'Malaysia' },
  75: { name: '1337 Rabat',          country: 'Morocco' },
  76: { name: '42 Al-Aïn',           country: 'United Arab Emirates' },
}

export const COUNTRY_TO_ISO = {
  Angola:                'AO',
  Armenia:               'AM',
  Australia:             'AU',
  Austria:               'AT',
  Belgium:               'BE',
  Brazil:                'BR',
  Canada:                'CA',
  'Czech Republic':      'CZ',
  Finland:               'FI',
  France:                'FR',
  Germany:               'DE',
  Italy:                 'IT',
  Japan:                 'JP',
  Jordan:                'JO',
  'Korea, Republic of':  'KR',
  Lebanon:               'LB',
  Luxembourg:            'LU',
  Madagascar:            'MG',
  Malaysia:              'MY',
  Morocco:               'MA',
  Netherlands:           'NL',
  'Palestine, State of': 'PS',
  Poland:                'PL',
  Portugal:              'PT',
  Singapore:             'SG',
  Spain:                 'ES',
  Switzerland:           'CH',
  Thailand:              'TH',
  Turkey:                'TR',
  'United Arab Emirates':'AE',
  'United Kingdom':      'GB',
}

export function isoToFlagSvg(isoCode) {
  if (!isoCode) return ''
  const upper = String(isoCode).toUpperCase()
  if (!/^[A-Z]{2}$/.test(upper)) return ''
  const a = 0x1f1e6 + (upper.charCodeAt(0) - 65)
  const b = 0x1f1e6 + (upper.charCodeAt(1) - 65)
  return `${a.toString(16)}-${b.toString(16)}`
}

export function countryFlagSvg(country) {
  return isoToFlagSvg(COUNTRY_TO_ISO[country])
}

export function campusParts(campusId) {
  if (campusId === null || campusId === undefined)
    return { label: 'campus ?', flagSvg: '', country: '' }
  const info = CAMPUS_INFO[String(campusId)] || CAMPUS_INFO[Number(campusId)]
  if (info?.name) {
    const flagSvg = countryFlagSvg(info.country)
    return {
      label:   info.country ? `${info.name} · ${info.country}` : info.name,
      flagSvg,
      country: info.country || '',
    }
  }
  return { label: `campus ${campusId}`, flagSvg: '', country: '' }
}
