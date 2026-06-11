import { useLang } from '../context/LangContext'

const CONTENT = {
  en: {
    title: 'Terms of Service',
    updated: 'Last updated: April 27, 2026',
    sections: [
      { p: 'These Terms of Service govern your access to and use of 42 Network Pulse, including the live detector, dashboards, admin views, and API data explorer provided through this application.' },
      { h: '1. Acceptance', p: 'By accessing or using 42 Network Pulse, you agree to follow these terms. If you do not agree, do not use the service.' },
      { h: '2. Service Scope', p: '42 Network Pulse is a data visualization and monitoring tool for inspecting API 42 related events and operational signals. The service is provided for informational and internal operational use.' },
      { h: '3. User Responsibilities', ul: [
        'Use the service only for lawful, authorized, and appropriate purposes.',
        'Keep access tokens, credentials, and administrative access private and secure.',
        'Do not attempt to disrupt, overload, reverse engineer, or bypass protections in the service.',
        'Do not use displayed data to harass, profile, or target individuals.',
      ]},
      { h: '4. Data and Privacy', p: 'The service may display event data, campus identifiers, timestamps, and other API-derived information. Handle this data with care. Access to the service does not grant permission to export, redistribute, or misuse personal or sensitive information.' },
      { h: '5. Third-Party Services', p: 'The service may rely on API 42 or other external systems. Your use of those systems may also be governed by their own terms, policies, and technical limits.' },
      { h: '6. Availability and Accuracy', p: 'The service is provided as available. Event streams, dashboards, and API payloads may be delayed, incomplete, unavailable, or inaccurate. Do not rely on the service as the sole source for critical decisions.' },
      { h: '7. Changes', p: 'These terms may be updated from time to time. Continued use of the service after changes are published means you accept the updated terms.' },
      { h: '8. Contact', p: 'For questions about these terms or the operation of the service, contact the project maintainer or system administrator responsible for this deployment.' },
    ],
  },
  fr: {
    title: 'Conditions d\'utilisation',
    updated: 'Dernière mise à jour : 27 avril 2026',
    sections: [
      { p: 'Les présentes conditions d\'utilisation régissent votre accès à 42 Network Pulse et son usage, y compris le détecteur en direct, les tableaux de bord, les vues d\'administration et l\'explorateur de données API fournis par cette application.' },
      { h: '1. Acceptation', p: 'En accédant à 42 Network Pulse ou en l\'utilisant, vous acceptez de respecter ces conditions. Si vous ne les acceptez pas, n\'utilisez pas le service.' },
      { h: '2. Périmètre du service', p: '42 Network Pulse est un outil de visualisation et de supervision destiné à inspecter des événements liés à l\'API 42 et des signaux opérationnels. Le service est fourni à des fins d\'information et d\'exploitation interne.' },
      { h: '3. Responsabilités des utilisateurs', ul: [
        'Utiliser le service uniquement à des fins légales, autorisées et appropriées.',
        'Garder les jetons d\'accès, identifiants et accès administratifs privés et sécurisés.',
        'Ne pas tenter de perturber, surcharger, rétroconcevoir ou contourner les protections du service.',
        'Ne pas utiliser les données affichées pour harceler, profiler ou cibler des personnes.',
      ]},
      { h: '4. Données et confidentialité', p: 'Le service peut afficher des données d\'événements, des identifiants de campus, des horodatages et d\'autres informations issues d\'API. Manipulez ces données avec soin. L\'accès au service ne donne pas l\'autorisation d\'exporter, de redistribuer ou d\'utiliser abusivement des informations personnelles ou sensibles.' },
      { h: '5. Services tiers', p: 'Le service peut s\'appuyer sur l\'API 42 ou d\'autres systèmes externes. Votre utilisation de ces systèmes peut également être régie par leurs propres conditions, politiques et limites techniques.' },
      { h: '6. Disponibilité et exactitude', p: 'Le service est fourni selon sa disponibilité. Les flux d\'événements, tableaux de bord et réponses API peuvent être retardés, incomplets, indisponibles ou inexacts. Ne vous appuyez pas sur le service comme seule source pour des décisions critiques.' },
      { h: '7. Modifications', p: 'Ces conditions peuvent être mises à jour de temps à autre. La poursuite de l\'utilisation du service après publication des changements signifie que vous acceptez les conditions mises à jour.' },
      { h: '8. Contact', p: 'Pour toute question concernant ces conditions ou le fonctionnement du service, contactez le responsable du projet ou l\'administrateur système chargé de ce déploiement.' },
    ],
  },
  es: {
    title: 'Términos de servicio',
    updated: 'Última actualización: 27 de abril de 2026',
    sections: [
      { p: 'Estos términos de servicio regulan tu acceso y uso de 42 Network Pulse, incluido el detector en vivo, los paneles, las vistas de administración y el explorador de datos de API proporcionados por esta aplicación.' },
      { h: '1. Aceptación', p: 'Al acceder a 42 Network Pulse o utilizarlo, aceptas cumplir estos términos. Si no estás de acuerdo, no uses el servicio.' },
      { h: '2. Alcance del servicio', p: '42 Network Pulse es una herramienta de visualización y monitoreo de datos para inspeccionar eventos relacionados con API 42 y señales operativas. El servicio se proporciona con fines informativos y de operación interna.' },
      { h: '3. Responsabilidades del usuario', ul: [
        'Usar el servicio solo con fines legales, autorizados y apropiados.',
        'Mantener privados y seguros los tokens de acceso, credenciales y accesos administrativos.',
        'No intentar interrumpir, sobrecargar, aplicar ingeniería inversa ni eludir las protecciones del servicio.',
        'No usar los datos mostrados para acosar, perfilar o dirigirse a personas.',
      ]},
      { h: '4. Datos y privacidad', p: 'El servicio puede mostrar datos de eventos, identificadores de campus, marcas temporales y otra información derivada de API. Trata estos datos con cuidado. El acceso al servicio no concede permiso para exportar, redistribuir o usar indebidamente información personal o sensible.' },
      { h: '5. Servicios de terceros', p: 'El servicio puede depender de API 42 u otros sistemas externos. Tu uso de esos sistemas también puede estar regido por sus propios términos, políticas y límites técnicos.' },
      { h: '6. Disponibilidad y exactitud', p: 'El servicio se proporciona según disponibilidad. Los flujos de eventos, paneles y respuestas API pueden estar retrasados, incompletos, no disponibles o ser inexactos. No dependas del servicio como única fuente para decisiones críticas.' },
      { h: '7. Cambios', p: 'Estos términos pueden actualizarse ocasionalmente. El uso continuado del servicio después de publicar cambios significa que aceptas los términos actualizados.' },
      { h: '8. Contacto', p: 'Para preguntas sobre estos términos o sobre el funcionamiento del servicio, contacta con la persona responsable del proyecto o con el administrador del sistema encargado de este despliegue.' },
    ],
  },
}

export default function Terms() {
  const { lang } = useLang()
  const { title, updated, sections } = CONTENT[lang] || CONTENT.en

  return (
    <main style={{ maxWidth: 980, width: '100%', margin: '0 auto' }}>
      <section className="panel" style={{ lineHeight: 1.7 }}>
        <div style={{ display: 'flex', justifyContent: 'space-between', gap: '1rem', alignItems: 'flex-start', flexWrap: 'wrap' }}>
          <div>
            <h2>{title}</h2>
            <p style={{ color: 'var(--text-muted)', margin: 0 }}>{updated}</p>
          </div>
        </div>
        {sections.map((item, i) => (
          <div key={i}>
            {item.h && <h2>{item.h}</h2>}
            {item.p && <p style={{ margin: 0, color: 'var(--text-muted)' }}>{item.p}</p>}
            {item.ul && (
              <ul style={{ listStyle: 'disc', paddingLeft: '1.25rem', maxHeight: 'none', overflow: 'visible' }}>
                {item.ul.map((li, j) => (
                  <li key={j} style={{ color: 'var(--text-muted)', fontSize: '0.95rem' }}>{li}</li>
                ))}
              </ul>
            )}
          </div>
        ))}
      </section>
    </main>
  )
}
