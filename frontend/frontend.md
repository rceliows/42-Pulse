-Proxy server to call for the current backend:

npm create vite@latest transcendence-frontend -- --template react
cd transcendence-frontend
npm install
npm install react-router-dom recharts

../vite.config.js added for it

-In repo/app/web/nginx.conf added:

location / {
  root /usr/share/nginx/html;
  try_files $uri /index.html;
}

(SPA fallback for React Router)

-In repo/infra/docker-compose.yml changed:

- type: bind
  source: ${PROJECT_ROOT:-..}/app/web/www        # <- old
  source: ${PROJECT_ROOT:-..}/app/web/dist       # <- new
  target: /usr/share/nginx/html

(To mount to the React app)

To run with the npm proxy server:

cd repo/app/web
npm install        # first time only
npm run build      # creates dist/
cd ../../..        # back to repo root
make deploy