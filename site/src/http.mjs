export const securityHeaders = {
  'X-Content-Type-Options': 'nosniff',
  'Cache-Control': 'no-cache',
  'Content-Security-Policy': "default-src 'self'; img-src 'self'; style-src 'self'; script-src 'self'; connect-src 'self'; base-uri 'none'; frame-ancestors 'none'",
  'Referrer-Policy': 'strict-origin-when-cross-origin',
  'Permissions-Policy': 'serial=(self)',
};

export const contentTypes = {
  '.html': 'text/html; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.js': 'text/javascript; charset=utf-8',
  '.json': 'application/json; charset=utf-8',
  '.svg': 'image/svg+xml',
  '.png': 'image/png',
  '.md': 'text/markdown; charset=utf-8',
  '.txt': 'text/plain; charset=utf-8',
};

export function publicRoute(pathname) {
  if (['/', '/stopwatch', '/stopwatch/'].includes(pathname)) return '/stopwatch/index.html';
  if (pathname === '/stopwatch/install/') return '/stopwatch/install/index.html';
  return pathname;
}
