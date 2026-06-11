import { useLang } from '../context/LangContext'

const CONTENT = {
  en: {
    title: 'Privacy Policy',
    updated: 'Last updated: April 27, 2026',
    sections: [
      { p: 'This Privacy Policy explains how 42 Network Pulse handles information displayed or processed through the live detector, dashboards, admin views, and API data explorer.' },
      { h: '1. Information We Process', p: 'The service may process API-derived event data such as timestamps, campus identifiers, event types, operational status, and related technical metadata. Depending on configuration and access level, some views may also involve user-related identifiers from API 42.' },
      { h: '2. How Information Is Used', ul: [
        'To display live operational activity and detector events.',
        'To support debugging, monitoring, analytics, and administrative review.',
        'To improve reliability, performance, and privacy-safe presentation of event data.',
      ]},
      { h: '3. Local Browser Storage', p: 'Some pages may store interface preferences or optional access settings in your browser, such as language selection or a remembered token if you explicitly enable that option. Browser-stored values remain on the device unless cleared by you or the browser.' },
      { h: '4. Data Minimization', p: '42 Network Pulse should be configured to show only the data needed for the intended operational purpose. Privacy-safe views may hide usernames, subject identifiers, or other direct identifiers while preserving event context such as campus, event type, and time.' },
      { h: '5. Sharing', p: 'Do not export, publish, or share displayed data unless you are authorized to do so and the sharing complies with applicable policies, laws, and API 42 requirements.' },
      { h: '6. Security', p: 'Keep tokens, credentials, and administrative access secure. The service should be accessed only by authorized users and deployed with appropriate network, authentication, and logging protections.' },
      { h: '7. Retention', p: 'Retention depends on the deployment configuration, database settings, and operational needs of the maintainer. Event data should not be kept longer than necessary for the service purpose.' },
      { h: '8. Third-Party Services', p: 'The service may connect to API 42 or other external systems. Those services may handle information under their own privacy policies, terms, and technical controls.' },
      { h: '9. Contact', p: 'For privacy questions, data access concerns, or removal requests, contact the project maintainer or system administrator responsible for this deployment.' },
    ],
  },
  fr: {
    title: 'Politique de confidentialité',
    updated: 'Dernière mise à jour : 27 avril 2026',
    sections: [
      { p: 'Cette politique de confidentialité explique comment 42 Network Pulse traite les informations affichées ou traitées par le détecteur en direct, les tableaux de bord, les vues d\'administration et l\'explorateur de données API.' },
      { h: '1. Informations traitées', p: 'Le service peut traiter des données d\'événements issues d\'API, comme des horodatages, identifiants de campus, types d\'événements, états opérationnels et métadonnées techniques associées. Selon la configuration et le niveau d\'accès, certaines vues peuvent aussi impliquer des identifiants liés aux utilisateurs provenant de l\'API 42.' },
      { h: '2. Utilisation des informations', ul: [
        'Afficher l\'activité opérationnelle en direct et les événements du détecteur.',
        'Prendre en charge le débogage, la supervision, l\'analyse et la revue administrative.',
        'Améliorer la fiabilité, les performances et la présentation respectueuse de la confidentialité.',
      ]},
      { h: '3. Stockage local du navigateur', p: 'Certaines pages peuvent stocker des préférences d\'interface ou des paramètres d\'accès optionnels dans votre navigateur, comme le choix de la langue ou un jeton mémorisé si vous activez explicitement cette option. Les valeurs stockées dans le navigateur restent sur l\'appareil sauf suppression par vous ou par le navigateur.' },
      { h: '4. Minimisation des données', p: '42 Network Pulse doit être configuré pour afficher uniquement les données nécessaires à l\'objectif opérationnel prévu. Les vues respectueuses de la confidentialité peuvent masquer les noms d\'utilisateur, identifiants de sujet ou autres identifiants directs tout en conservant le contexte de l\'événement, comme le campus, le type d\'événement et l\'heure.' },
      { h: '5. Partage', p: 'N\'exportez, ne publiez et ne partagez pas les données affichées sauf si vous y êtes autorisé et si ce partage respecte les politiques applicables, les lois et les exigences de l\'API 42.' },
      { h: '6. Sécurité', p: 'Gardez les jetons, identifiants et accès administratifs sécurisés. Le service doit être accessible uniquement aux utilisateurs autorisés et déployé avec des protections réseau, d\'authentification et de journalisation appropriées.' },
      { h: '7. Conservation', p: 'La conservation dépend de la configuration du déploiement, des paramètres de base de données et des besoins opérationnels du mainteneur. Les données d\'événements ne doivent pas être conservées plus longtemps que nécessaire au but du service.' },
      { h: '8. Services tiers', p: 'Le service peut se connecter à l\'API 42 ou à d\'autres systèmes externes. Ces services peuvent traiter les informations selon leurs propres politiques de confidentialité, conditions et contrôles techniques.' },
      { h: '9. Contact', p: 'Pour toute question liée à la confidentialité, demande d\'accès aux données ou demande de suppression, contactez le responsable du projet ou l\'administrateur système chargé de ce déploiement.' },
    ],
  },
  es: {
    title: 'Política de privacidad',
    updated: 'Última actualización: 27 de abril de 2026',
    sections: [
      { p: 'Esta política de privacidad explica cómo 42 Network Pulse gestiona la información mostrada o procesada a través del detector en vivo, los paneles, las vistas de administración y el explorador de datos de API.' },
      { h: '1. Información que procesamos', p: 'El servicio puede procesar datos de eventos derivados de API, como marcas temporales, identificadores de campus, tipos de eventos, estado operativo y metadatos técnicos relacionados. Según la configuración y el nivel de acceso, algunas vistas también pueden implicar identificadores relacionados con usuarios de API 42.' },
      { h: '2. Uso de la información', ul: [
        'Mostrar actividad operativa en vivo y eventos del detector.',
        'Apoyar la depuración, el monitoreo, el análisis y la revisión administrativa.',
        'Mejorar la fiabilidad, el rendimiento y la presentación de datos respetuosa con la privacidad.',
      ]},
      { h: '3. Almacenamiento local del navegador', p: 'Algunas páginas pueden almacenar preferencias de interfaz o ajustes de acceso opcionales en tu navegador, como la selección de idioma o un token recordado si activas explícitamente esa opción. Los valores almacenados en el navegador permanecen en el dispositivo salvo que tú o el navegador los eliminen.' },
      { h: '4. Minimización de datos', p: '42 Network Pulse debe configurarse para mostrar solo los datos necesarios para el propósito operativo previsto. Las vistas respetuosas con la privacidad pueden ocultar nombres de usuario, identificadores de sujeto u otros identificadores directos, conservando el contexto del evento, como campus, tipo de evento y hora.' },
      { h: '5. Compartición', p: 'No exportes, publiques ni compartas los datos mostrados salvo que estés autorizado a hacerlo y que dicha compartición cumpla las políticas aplicables, las leyes y los requisitos de API 42.' },
      { h: '6. Seguridad', p: 'Mantén seguros los tokens, credenciales y accesos administrativos. El servicio debe ser accesible solo para usuarios autorizados y desplegarse con protecciones adecuadas de red, autenticación y registro.' },
      { h: '7. Conservación', p: 'La conservación depende de la configuración del despliegue, los ajustes de base de datos y las necesidades operativas del responsable. Los datos de eventos no deben conservarse más tiempo del necesario para el propósito del servicio.' },
      { h: '8. Servicios de terceros', p: 'El servicio puede conectarse a API 42 u otros sistemas externos. Esos servicios pueden gestionar información bajo sus propias políticas de privacidad, términos y controles técnicos.' },
      { h: '9. Contacto', p: 'Para preguntas sobre privacidad, solicitudes de acceso a datos o solicitudes de eliminación, contacta con la persona responsable del proyecto o con el administrador del sistema encargado de este despliegue.' },
    ],
  },
}

export default function Privacy() {
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
