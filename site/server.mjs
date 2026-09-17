import http from 'node:http';
import { readFile, stat } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
import { contentTypes, publicRoute, securityHeaders } from './src/http.mjs';
import { releaseAsset } from './src/release-assets.mjs';

const root = path.resolve(fileURLToPath(new URL('./public/', import.meta.url)));
const releaseConfig = JSON.parse(await readFile(new URL('./src/release-config.json', import.meta.url), 'utf8'));
const server = http.createServer(async (req, res) => {
  if (!['GET', 'HEAD'].includes(req.method)) { res.writeHead(405, { Allow: 'GET, HEAD' }); return res.end(); }
  try {
    const pathname = decodeURIComponent(new URL(req.url, 'http://localhost').pathname);
    if (pathname === '/stopwatch/install') { res.writeHead(308, { ...securityHeaders, Location: '/stopwatch/install/' }); return res.end(); }
    const download = await releaseAsset(new Request(new URL(req.url, 'http://localhost'), { method: req.method }), releaseConfig);
    if (download) {
      res.writeHead(download.status, { ...securityHeaders, ...Object.fromEntries(download.headers) });
      return res.end(req.method === 'HEAD' ? undefined : Buffer.from(await download.arrayBuffer()));
    }
    const relative = publicRoute(pathname).replace(/^\/+/, '');
    const file = path.resolve(root, relative);
    if (!file.startsWith(root + path.sep) || !(await stat(file)).isFile()) throw new Error('Not found');
    const contents = await readFile(file);
    res.writeHead(200, {
      ...securityHeaders,
      'Content-Type': contentTypes[path.extname(file)] || 'application/octet-stream',
      'Content-Length': contents.length,
    });
    res.end(req.method === 'HEAD' ? undefined : contents);
  } catch { res.writeHead(404, { 'Content-Type': 'text/plain; charset=utf-8' }); res.end('Not found'); }
});
server.listen(Number(process.env.PORT || 4173), process.env.HOST || '0.0.0.0', () => console.log(`StopWatch guide listening on port ${server.address().port}`));
