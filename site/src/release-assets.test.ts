import { createHash } from 'node:crypto';
import { describe, expect, it, vi } from 'vitest';
import { releaseAsset } from './release-assets.mjs';

const bytes = new Uint8Array([0xe9, 0, 1, 255, 21, 40]);
const digest = createHash('sha256').update(bytes).digest('hex');
const names = ['release.json', 'bootloader.bin', 'partition-table.bin', 'firmware.bin'];
const config = {
  repository: 'chantastic/m5stack-stopwatch-authkit', tag: 'conference-factory-3',
  assets: names.map(name => ({ name, sizeBytes: bytes.length, sha256: digest })),
};
const path = '/stopwatch/install/releases/conference-factory-3/';
const request = (name = 'firmware.bin', method = 'GET') => new Request(`https://stopwatch.workos.cloud${path}${name}`, {
  method, headers: { Authorization: 'Bearer must-stay-local', Cookie: 'session=private', Range: 'bytes=0-1', 'X-Alto-Identity': 'private' },
});
const response = (body: BodyInit = bytes) => new Response(body, {
  headers: { 'Content-Type': 'application/octet-stream', 'Content-Length': String(bytes.length) },
});

describe('Pinned release downloads', () => {
  it.each(names)('serves only verified %s bytes, with no caller credentials or headers upstream', async name => {
    const fetchAsset = vi.fn().mockResolvedValue(response());
    const result = await releaseAsset(request(name), config, fetchAsset);
    expect(result.status).toBe(200);
    expect(new Uint8Array(await result.arrayBuffer())).toEqual(bytes);
    expect(result.headers.get('Cache-Control')).toContain('immutable');
    expect(result.headers.get('ETag')).toBe(`"sha256-${digest}"`);
    expect(result.headers.get('Content-Type')).toBe(name === 'release.json' ? 'application/json; charset=utf-8' : 'application/octet-stream');
    expect(fetchAsset).toHaveBeenCalledOnce();
    const [url, options] = fetchAsset.mock.calls[0];
    expect(url).toBe(`https://github.com/chantastic/m5stack-stopwatch-authkit/releases/download/conference-factory-3/${name}`);
    expect(options).toMatchObject({ method: 'GET', redirect: 'manual', credentials: 'omit', headers: { Accept: 'application/octet-stream' } });
    expect(Object.keys(options.headers)).toEqual(['Accept']);
  });

  it('allows one HTTPS GitHub asset redirect with the same credential-free options', async () => {
    const target = 'https://release-assets.githubusercontent.com/github-production-release-asset/123/asset?signature=opaque';
    const fetchAsset = vi.fn().mockResolvedValueOnce(new Response(null, { status: 302, headers: { Location: target } })).mockResolvedValueOnce(response());
    const result = await releaseAsset(request(), config, fetchAsset);
    expect(result.status).toBe(200);
    expect(fetchAsset.mock.calls[1][0]).toBe(target);
    expect(fetchAsset.mock.calls[1][1]).toEqual(fetchAsset.mock.calls[0][1]);
  });

  it.each([
    'https://example.com/firmware.bin', 'http://release-assets.githubusercontent.com/asset',
    'https://release-assets.githubusercontent.com.attacker.example/asset',
    'https://user:password@release-assets.githubusercontent.com/asset',
    'https://release-assets.githubusercontent.com:8443/asset', '/other/file',
  ])('rejects a redirect outside the asset host: %s', async target => {
    const fetchAsset = vi.fn().mockResolvedValue(new Response(null, { status: 302, headers: { Location: target } }));
    const result = await releaseAsset(request(), config, fetchAsset);
    expect(result.status).toBe(502);
    expect(result.headers.get('Cache-Control')).toBe('no-store');
    expect(fetchAsset).toHaveBeenCalledOnce();
  });

  it.each(['secret.bin', 'firmware.bin/extra', 'firmware%2ebin', '../firmware.bin', '../conference-factory-4/firmware.bin'])('never fetches an unlisted path: %s', async name => {
    const fetchAsset = vi.fn();
    expect((await releaseAsset(request(name), config, fetchAsset)).status).toBe(404);
    expect(fetchAsset).not.toHaveBeenCalled();
  });

  it('ignores unrelated routes and refuses writes or incomplete pins without making a request', async () => {
    const fetchAsset = vi.fn();
    expect(await releaseAsset(new Request('https://example.com/stopwatch/'), config, fetchAsset)).toBeNull();
    expect((await releaseAsset(request('firmware.bin', 'POST'), config, fetchAsset)).status).toBe(405);
    expect((await releaseAsset(request(), { ...config, assets: config.assets.map(asset => ({ ...asset, sha256: null })) }, fetchAsset)).status).toBe(503);
    expect((await releaseAsset(request(), { ...config, repository: 'someone/else' }, fetchAsset)).status).toBe(502);
    expect(fetchAsset).not.toHaveBeenCalled();
  });

  it.each([
    () => new Response('not found', { status: 404 }),
    () => new Response(bytes, { status: 206, headers: { 'Content-Type': 'application/octet-stream' } }),
    () => new Response(bytes, { headers: { 'Content-Type': 'text/html' } }),
    () => response(new Uint8Array([0, 0, 1, 255, 21, 40])),
    () => response(bytes.slice(0, -1)),
    () => new Response(bytes, { headers: { 'Content-Type': 'application/octet-stream', 'Content-Length': '999999999' } }),
  ])('rejects missing, partial, malformed, short, or corrupted upstream content', async makeResponse => {
    const result = await releaseAsset(request(), config, vi.fn().mockResolvedValue(makeResponse()));
    expect(result.status).toBe(502);
    expect(result.headers.get('Cache-Control')).toBe('no-store');
    expect(result.headers.get('ETag')).toBeNull();
  });

  it('cancels an oversized chunked response before buffering beyond the pinned size', async () => {
    const cancel = vi.fn();
    const body = new ReadableStream({ start(controller) { controller.enqueue(new Uint8Array(bytes.length + 1)); }, cancel });
    const upstream = new Response(body, { headers: { 'Content-Type': 'application/octet-stream' } });
    const result = await releaseAsset(request(), config, vi.fn().mockResolvedValue(upstream));
    expect(result.status).toBe(502);
    expect(cancel).toHaveBeenCalledOnce();
  });

  it('verifies a HEAD response against the actual upstream bytes and returns no body', async () => {
    const fetchAsset = vi.fn().mockResolvedValue(response());
    const result = await releaseAsset(request('firmware.bin', 'HEAD'), config, fetchAsset);
    expect(result.status).toBe(200);
    expect((await result.arrayBuffer()).byteLength).toBe(0);
    expect(result.headers.get('Content-Length')).toBe(String(bytes.length));
    expect(fetchAsset.mock.calls[0][1].method).toBe('GET');
  });
});
