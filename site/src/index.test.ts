import { describe, expect, it, vi } from 'vitest';
import { createHash } from 'node:crypto';
import { readFile } from 'node:fs/promises';
import worker from './index';
import { publicRoute } from './http.mjs';
import releaseConfig from './release-config.json';

const get = (route: string, method = 'GET') => worker.fetch(new Request(`https://example.com${route}`, { method }));

describe('Public StopWatch routes', () => {
  it.each(['/', '/stopwatch', '/stopwatch/'])('serves page content at %s; Alto controls access upstream', async route => {
    const response = await get(route);
    expect(response.status).toBe(200);
    expect(response.headers.get('Content-Type')).toBe('text/html; charset=utf-8');
    expect(await response.text()).toBe(await readFile(new URL('../public/stopwatch/index.html', import.meta.url), 'utf8'));
  });

  it.each([
    ['/stopwatch/SKILL.md', 'text/markdown; charset=utf-8'],
    ['/llms.txt', 'text/plain; charset=utf-8'],
    ['/stopwatch/style.css', 'text/css; charset=utf-8'],
    ['/stopwatch/app.js', 'text/javascript; charset=utf-8'],
    ['/stopwatch/assets/stopwatch.svg', 'image/svg+xml'],
    ['/stopwatch/assets/workos-logo.svg', 'image/svg+xml'],
    ['/stopwatch/assets/workos-logo-white.svg', 'image/svg+xml'],
    ['/stopwatch/assets/init-logo.svg', 'image/svg+xml'],
    ['/stopwatch/install/index.html', 'text/html; charset=utf-8'],
    ['/stopwatch/install/style.css', 'text/css; charset=utf-8'],
    ['/stopwatch/install/installer.js', 'text/javascript; charset=utf-8'],
    ['/stopwatch/install/THIRD_PARTY_NOTICES.txt', 'text/plain; charset=utf-8'],
  ])('serves the exact file and type for %s', async (route, type) => {
    const response = await get(route);
    expect(response.status).toBe(200);
    expect(response.headers.get('Content-Type')).toBe(type);
    expect(await response.text()).toBe(await readFile(new URL(`../public${route}`, import.meta.url), 'utf8'));
  });

  it('keeps a missing file as a 404 rather than an HTML fallback', async () => {
    expect((await get('/stopwatch/missing.svg')).status).toBe(404);
  });

  it('serves the illustration as an intact PNG and omits its body for HEAD', async () => {
    const route = '/stopwatch/assets/stopwatch-manual.png';
    const response = await get(route);
    expect(response.status).toBe(200);
    expect(response.headers.get('Content-Type')).toBe('image/png');
    expect(Buffer.from(await response.arrayBuffer())).toEqual(
      await readFile(new URL(`../public${route}`, import.meta.url)),
    );
    const head = await get(route, 'HEAD');
    expect(head.status).toBe(200);
    expect(head.headers.get('Content-Type')).toBe('image/png');
    expect((await head.arrayBuffer()).byteLength).toBe(0);
  });

  it('exposes consistent sharing metadata early in HTML and a fetchable image', async () => {
    const response = await worker.fetch(new Request('https://untrusted-host.example/stopwatch', {
      headers: {
        'User-Agent': 'Slackbot-LinkExpanding 1.0 (+https://api.slack.com/robots)',
        Range: 'bytes=0-8191',
      },
    }));
    const head = (await response.text()).split('</head>')[0];
    expect(response.status).toBe(200);
    expect(Buffer.byteLength(head)).toBeLessThan(8192);
    const metadata = Object.fromEntries([...head.matchAll(/<meta (?:property|name)="([^"]+)" content="([^"]+)"/g)].map(match => [match[1], match[2]]));
    const canonical = 'https://stopwatch.workos.cloud/stopwatch';
    expect(head).toContain(`<link rel="canonical" href="${canonical}">`);
    expect(metadata['og:url']).toBe(canonical);
    expect(metadata['og:type']).toBe('website');
    expect(metadata['og:title']).toBe(metadata['twitter:title']);
    expect(metadata['og:description']).toBe(metadata['twitter:description']);
    expect(metadata['og:image']).toBe(metadata['twitter:image']);
    expect(metadata['og:image:alt']).toBe(metadata['twitter:image:alt']);
    expect(metadata['og:image:alt'].length).toBeGreaterThan(20);
    expect(metadata['twitter:card']).toBe('summary_large_image');
    const imageUrl = new URL(metadata['og:image']);
    expect(imageUrl.origin).toBe('https://stopwatch.workos.cloud');
    expect(imageUrl.pathname).toBe('/stopwatch/og.png');
    const image = await worker.fetch(new Request(imageUrl));
    expect(image.status).toBe(200);
    expect(image.headers.get('Content-Type')).toBe(metadata['og:image:type']);
    const bytes = Buffer.from(await image.arrayBuffer());
    expect(bytes.equals(await readFile(new URL('../public/stopwatch/og.png', import.meta.url)))).toBe(true);
    expect(bytes.subarray(0, 8).toString('hex')).toBe('89504e470d0a1a0a');
    expect(bytes.readUInt32BE(16)).toBe(Number(metadata['og:image:width']));
    expect(bytes.readUInt32BE(20)).toBe(Number(metadata['og:image:height']));
    expect(bytes.byteLength).toBeLessThan(5_000_000);
    const headImage = await get(imageUrl.pathname, 'HEAD');
    expect(headImage.status).toBe(200);
    expect((await headImage.arrayBuffer()).byteLength).toBe(0);
  });

  it('supports HEAD without a body and rejects writes', async () => {
    const head = await get('/stopwatch/SKILL.md', 'HEAD');
    expect(head.status).toBe(200);
    expect(await head.text()).toBe('');
    expect((await get('/stopwatch', 'POST')).status).toBe(405);
  });

  it('serves the root directly without redirecting to a path', async () => {
    const response = await get('/');
    expect(response.status).toBe(200);
    expect(response.headers.get('Location')).toBeNull();
  });

  it('links the preserved guide to the installer and issue form', async () => {
    const html = await (await get('/stopwatch')).text();
    expect(html).toContain('href="/stopwatch/install/">Install conference badge');
    expect(html).toContain('https://github.com/chantastic/m5stack-stopwatch-authkit/issues/new');
    expect(html).toContain('id="hardware"');
    expect(html).toContain('id="agents"');
    expect(html).not.toContain('a WorkOS init() image is not included');
  });

  it('canonicalizes the installer directory and permits serial only on its own origin', async () => {
    const redirect = await get('/stopwatch/install');
    expect(redirect.status).toBe(308);
    expect(redirect.headers.get('Location')).toBe('/stopwatch/install/');
    expect(publicRoute('/stopwatch/install/')).toBe('/stopwatch/install/index.html');
    const guide = await get('/stopwatch');
    expect(guide.headers.get('Permissions-Policy')).toBe('serial=(self)');
    expect(guide.headers.get('Content-Security-Policy')).toContain("frame-ancestors 'none'");
  });

  it('serves the complete installer as a top-level page with working same-origin assets', async () => {
    const response = await get('/stopwatch/install/');
    expect(response.status).toBe(200);
    const html = await response.text();
    expect(html).toBe(await readFile(new URL('../public/stopwatch/install/index.html', import.meta.url), 'utf8'));
    expect(html).toContain('conference-factory-3');
    for (const [, asset] of html.matchAll(/(?:src|href)="(\/stopwatch\/install\/[^\"]+\.(?:css|js))"/g)) {
      expect((await get(asset)).status).toBe(200);
    }
    expect(response.headers.get('Permissions-Policy')).toBe('serial=(self)');
    expect(response.headers.get('Content-Security-Policy')).toContain("connect-src 'self'");
    const head = await get('/stopwatch/install/', 'HEAD');
    expect(head.status).toBe(200);
    expect(await head.text()).toBe('');
  });

  it('keeps recovery files local and exposes distinct factory, update, and recovery actions', async () => {
    const html = await (await get('/stopwatch/install/')).text();
    for (const id of ['prepare', 'install', 'save-backup', 'replace-confirm', 'factory-write', 'finish-confirm', 'finish-install']) {
      expect(html).toContain(`id="${id}"`);
    }
    expect(html).toContain('Your recovery copy stays on your computer');
    expect((await get('/stopwatch/install/recovery.bin')).status).toBe(404);
    expect((await get('/stopwatch/install/recovery.bin', 'POST')).status).toBe(405);
  });

  it.each(releaseConfig.assets)('serves the verified $name without runtime network access', async asset => {
    const fetch = vi.spyOn(globalThis, 'fetch').mockRejectedValue(new Error('Runtime egress forbidden'));
    try {
      const path = `/stopwatch/install/releases/${releaseConfig.tag}/${asset.name}`;
      const response = await get(path);
      const bytes = Buffer.from(await response.arrayBuffer());
      expect(response.status).toBe(200);
      expect(bytes.length).toBe(asset.sizeBytes);
      expect(createHash('sha256').update(bytes).digest('hex')).toBe(asset.sha256);
      expect(response.headers.get('Cache-Control')).toContain('immutable');
      expect(response.headers.get('ETag')).toBe(`"sha256-${asset.sha256}"`);
      const head = await get(path, 'HEAD');
      expect(head.status).toBe(200);
      expect(await head.text()).toBe('');
      expect(fetch).not.toHaveBeenCalled();
    } finally { fetch.mockRestore(); }
  });

  it.each(['secret.bin', '../conference-factory-4/firmware.bin', 'firmware%2ebin'])('keeps an unlisted release path at 404: %s', async name => {
    expect((await get(`/stopwatch/install/releases/${releaseConfig.tag}/${name}`)).status).toBe(404);
  });
});
