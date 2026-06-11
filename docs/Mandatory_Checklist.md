Mandatory Checklist (Project Rejection Risk)

 Implement signup/login/logout with email+password auth.
 Store passwords hashed + salted (bcrypt/argon2), never plaintext.
 Add backend auth protections (session/JWT + auth middleware).
 Add frontend + backend validation for all user inputs/forms.
 Create Privacy Policy page with real content.
 Create Terms of Service page with real content.
 Add visible links to Privacy/ToS from app pages (footer/nav).
 Enable HTTPS (TLS certs, 443, redirect 80→443, WSS).
 Validate multi-user logged-in behavior (concurrent use, no conflicts).
 Verify latest Chrome compatibility.
 Fix all browser console warnings/errors.
 Keep one-command container deploy working from clean state.
 Ensure .env is ignored and .env.example is complete.
 Ensure git history shows contributions from all team members.
 Complete README mandatory sections (roles, PM process, stack, schema, modules, contributions).

Technical mùinimal modules to do for our final product:

Claimable (6 pts):

Minor: Use a backend framework (Express, Fastify, NestJS, Django, etc.).
Major: Backend as microservices.
Major: Implement real-time features using WebSockets or similar technology.
Minor: Health check and status page system (backups and disaster recovery procedures).

Claimable soon (3 pts):

Major: Advanced analytics dashboard with data visualization. (partial)
Minor: Implement advanced search functionality with filters, sorting, and pagination.

=> MVP - transcendance 9 pts before and of april.

