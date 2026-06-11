“Picture a live 3D globe of the 42 Network where it never goes dark. Every campus lights up in real time as students log in, level up, finish projects, and join coalitions—so you can spin the planet, zoom into any campus, and see its stats and momentum instantly. It turns raw API data into a living map—part analytics, part spectacle—proving 42 is worldwide, always on, and instantly understandable.”

Pulse42
42Lumen
AlwaysOn42
42Glow
42Orbit
Live42 Globe
42Beacon
42Current
42Flow
42Atlas Live

Position globe = lat/lon
Taille de base demi sphere campus = user_count

Activité instantanée = flash à chaque event = activity frequency
Intensité lumière = events/hour / (users_count/100) = normalized
(a small star can bright very strong)
Optional (color, duration, mode, max intesity)

+ halo pour last activity compare to see other on fuxed perios h 1d 7d

lon lat = campus location
country = campus country
name = campus name
campus_count = nombre de campus actifs/publics


students_official = users_count par campus (refresh quotidien)
events_per_hour = events_60m / 1h (fenêtre glissante)
activity_normalized = events_per_hour / (users_count / 100)

CAMPUS INFO DB

    BRAND NAME
    "id": 61,
    "name": "Belo Horizonte",
    "time_zone": "America/Sao_Paulo",
    "language": {
      "id": 17,
      "name": "Brazilian Portuguese",
      "identifier": "pt_br"
    },
    "users_count": 12,
    "country": "Brazil",
    "address": "- No address yet -",
    "zip": "30000",
    "city": "Belo Horizonte",
    "website": "https://42bh.org.br",
  
  activity (flash)
  normalized activity events_per_period / (users_count / 100) (halo)

global activity (users scsan duringthe last period)

  activity period windows 1h, 1d, 7d

