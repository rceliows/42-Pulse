'use strict';

const crypto = require('crypto');
const http = require('http');
const { URL, URLSearchParams } = require('url');

const PORT = number(process.env.PORT, 4180);
const UPSTREAM_URL = new URL(process.env.UPSTREAM_URL || 'http://web:80');
const PUBLIC_BASE_URL = trimSlash(process.env.PUBLIC_BASE_URL || 'https://localhost:9443');
const SESSION_SECRET = process.env.SESSION_SECRET || '';
const CLIENT_ID = process.env.OAUTH_42_CLIENT_ID || '';
const CLIENT_SECRET = process.env.OAUTH_42_CLIENT_SECRET || '';
const SCOPES = process.env.OAUTH_42_SCOPES || 'public';
const SESSION_TTL = number(process.env.SESSION_TTL_SECONDS, 8 * 60 * 60);

const SESSION_COOKIE = 'security_gateway_session';
const STATE_COOKIE = 'security_gateway_oauth_state';
const REDIRECT_URI = `${PUBLIC_BASE_URL}/auth/callback`;

if (SESSION_SECRET.length < 32) {
  console.error('Missing SESSION_SECRET. Run: make bootstrap');
  process.exit(1);
}
if (UPSTREAM_URL.protocol !== 'http:') {
  console.error('This simple gateway only supports an http:// upstream for now.');
  process.exit(1);
}

const server = http.createServer(async (req, res) => {
  try {
    await route(req, res);
  } catch (error) {
    console.error(error);
    json(res, 500, { error: 'internal_error' });
  }
});

server.listen(PORT, '0.0.0.0', () => {
  console.log(`OAuth 42 gateway listening on :${PORT}`);
  console.log(`Protected app upstream: ${UPSTREAM_URL.href}`);
});

async function route(req, res) {
  const url = new URL(req.url || '/', 'http://local');

  if (url.pathname === '/healthz') {
    res.writeHead(200, { 'content-type': 'text/plain; charset=utf-8' });
    res.end('OK');
    return;
  }

  if (url.pathname === '/auth/login') {
    login(res, url.searchParams.get('return_to') || '/');
    return;
  }

  if (url.pathname === '/auth/callback') {
    await callback(req, res, url);
    return;
  }

  if (url.pathname === '/auth/logout') {
    clearCookie(res, SESSION_COOKIE);
    res.writeHead(302, { location: '/' });
    res.end();
    return;
  }

  if (url.pathname === '/auth/me') {
    const user = readSession(req);
    json(res, user ? 200 : 401, user ? { authenticated: true, user } : { authenticated: false });
    return;
  }

  const user = readSession(req);
  if (!user) {
    if (wantsHtml(req)) {
      res.writeHead(302, { location: `/auth/login?return_to=${encodeURIComponent(safePath(req.url || '/'))}` });
      res.end();
      return;
    }
    json(res, 401, { error: 'login_required' });
    return;
  }

  proxyHttp(req, res, user);
}

function login(res, returnTo) {
  if (!CLIENT_ID || !CLIENT_SECRET) {
    json(res, 503, { error: 'oauth_42_not_configured' });
    return;
  }

  const state = randomToken();
  const statePayload = {
    state,
    return_to: safePath(returnTo),
    exp: now() + 10 * 60,
  };

  const authorizeUrl = new URL('https://api.intra.42.fr/oauth/authorize');
  authorizeUrl.search = new URLSearchParams({
    client_id: CLIENT_ID,
    redirect_uri: REDIRECT_URI,
    response_type: 'code',
    scope: SCOPES,
    state,
  }).toString();

  setCookie(res, STATE_COOKIE, signJson(statePayload), 10 * 60);
  res.writeHead(302, { location: authorizeUrl.toString() });
  res.end();
}

async function callback(req, res, url) {
  const stateCookie = verifyJson(readCookie(req, STATE_COOKIE));
  const code = url.searchParams.get('code');
  const state = url.searchParams.get('state');

  if (!code || !stateCookie || stateCookie.state !== state) {
    json(res, 401, { error: 'bad_oauth_state' });
    return;
  }

  const token = await get42Token(code);
  const profile = await get42Profile(token.access_token);
  const user = {
    provider: '42',
    id: String(profile.id || ''),
    login: profile.login || '',
    email: profile.email || '',
    name: profile.displayname || profile.usual_full_name || profile.login || '',
    exp: now() + SESSION_TTL,
  };

  setCookie(res, SESSION_COOKIE, signJson(user), SESSION_TTL);
  clearCookie(res, STATE_COOKIE);
  res.writeHead(302, { location: stateCookie.return_to || '/' });
  res.end();
}

async function get42Token(code) {
  const body = new URLSearchParams({
    grant_type: 'authorization_code',
    client_id: CLIENT_ID,
    client_secret: CLIENT_SECRET,
    code,
    redirect_uri: REDIRECT_URI,
  });

  const response = await fetch('https://api.intra.42.fr/oauth/token', {
    method: 'POST',
    headers: {
      accept: 'application/json',
      'content-type': 'application/x-www-form-urlencoded',
    },
    body,
  });

  if (!response.ok) {
    throw new Error(`42 token exchange failed with HTTP ${response.status}`);
  }
  return response.json();
}

async function get42Profile(accessToken) {
  const response = await fetch('https://api.intra.42.fr/v2/me', {
    headers: {
      accept: 'application/json',
      authorization: `Bearer ${accessToken}`,
    },
  });

  if (!response.ok) {
    throw new Error(`42 profile lookup failed with HTTP ${response.status}`);
  }
  return response.json();
}

function proxyHttp(req, res, user) {
  const target = new URL(req.url || '/', UPSTREAM_URL);
  const headers = proxyHeaders(req.headers, user);

  const upstream = http.request({
    protocol: target.protocol,
    hostname: target.hostname,
    port: target.port || 80,
    method: req.method,
    path: `${target.pathname}${target.search}`,
    headers,
  }, (upstreamRes) => {
    const cleanHeaders = { ...upstreamRes.headers };
    delete cleanHeaders['transfer-encoding'];
    res.writeHead(upstreamRes.statusCode || 502, cleanHeaders);
    upstreamRes.pipe(res);
  });

  upstream.on('error', () => json(res, 502, { error: 'upstream_unavailable' }));
  req.pipe(upstream);
}

function proxyHeaders(input, user) {
  const headers = { ...input };
  delete headers.connection;
  delete headers.upgrade;
  delete headers['transfer-encoding'];

  headers.host = UPSTREAM_URL.host;
  headers['x-forwarded-proto'] = 'https';
  headers['x-auth-provider'] = '42';
  headers['x-auth-id'] = cleanHeader(user.id);
  headers['x-auth-login'] = cleanHeader(user.login);
  headers['x-auth-email'] = cleanHeader(user.email);
  headers['x-auth-name'] = cleanHeader(user.name);
  return headers;
}

function setCookie(res, name, value, ttl) {
  res.setHeader('set-cookie', [
    ...currentCookies(res),
    `${name}=${encodeURIComponent(value)}; Path=/; Max-Age=${ttl}; HttpOnly; Secure; SameSite=Lax`,
  ]);
}

function clearCookie(res, name) {
  res.setHeader('set-cookie', [...currentCookies(res), `${name}=; Path=/; Max-Age=0; HttpOnly; Secure; SameSite=Lax`]);
}

function currentCookies(res) {
  const cookies = res.getHeader('set-cookie');
  if (!cookies) {
    return [];
  }
  return Array.isArray(cookies) ? cookies : [cookies];
}

function readCookie(req, name) {
  const raw = req.headers.cookie || '';
  for (const part of raw.split(';')) {
    const [key, ...rest] = part.trim().split('=');
    if (key === name) {
      return decodeURIComponent(rest.join('='));
    }
  }
  return '';
}

function readSession(req) {
  return verifyJson(readCookie(req, SESSION_COOKIE));
}

function signJson(payload) {
  const body = Buffer.from(JSON.stringify(payload)).toString('base64url');
  return `${body}.${sign(body)}`;
}

function verifyJson(value) {
  if (!value || !value.includes('.')) {
    return null;
  }

  const [body, signature] = value.split('.', 2);
  if (sign(body) !== signature) {
    return null;
  }

  try {
    const payload = JSON.parse(Buffer.from(body, 'base64url').toString('utf8'));
    return payload.exp > now() ? payload : null;
  } catch {
    return null;
  }
}

function sign(value) {
  return crypto.createHmac('sha256', SESSION_SECRET).update(value).digest('base64url');
}

function randomToken() {
  return crypto.randomBytes(32).toString('base64url');
}

function json(res, status, payload) {
  res.writeHead(status, {
    'content-type': 'application/json; charset=utf-8',
    'cache-control': 'no-store',
  });
  res.end(JSON.stringify(payload));
}

function wantsHtml(req) {
  return req.method === 'GET' && String(req.headers.accept || '').includes('text/html');
}

function safePath(value) {
  return value && value.startsWith('/') && !value.startsWith('//') ? value : '/';
}

function cleanHeader(value) {
  return String(value || '').replace(/[\r\n]/g, '').slice(0, 256);
}

function trimSlash(value) {
  return value.replace(/\/+$/, '');
}

function number(value, fallback) {
  const parsed = Number.parseInt(value, 10);
  return Number.isFinite(parsed) ? parsed : fallback;
}

function now() {
  return Math.floor(Date.now() / 1000);
}
