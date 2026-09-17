const NAMES = ['release.json', 'bootloader.bin', 'partition-table.bin', 'firmware.bin'];
const REPOSITORY = 'chantastic/m5stack-stopwatch-authkit';
const MAX_BYTES = 3 * 1024 * 1024;
const REDIRECTS = new Set([301, 302, 303, 307, 308]);

const unavailable = () => new Response('The release download could not be verified. Please try again later.', {
  status: 502, headers: { 'Content-Type': 'text/plain; charset=utf-8', 'Cache-Control': 'no-store' },
});

function validConfig(config) {
  return config.repository === REPOSITORY && /^conference-factory-[1-9][0-9]*$/.test(config.tag) &&
    Array.isArray(config.assets) && config.assets.length === NAMES.length &&
    NAMES.every(name => config.assets.filter(asset => asset.name === name).length === 1);
}

function validAsset(asset) {
  return Number.isSafeInteger(asset.sizeBytes) && asset.sizeBytes > 0 && asset.sizeBytes <= MAX_BYTES &&
    typeof asset.sha256 === 'string' && /^[a-f0-9]{64}$/.test(asset.sha256);
}

// Only these exact paths can reach GitHub. Neither caller URLs nor caller
// headers participate in the upstream request; every response is verified
// before any bytes are returned to the browser or marked cacheable.
export async function releaseAsset(request, config, fetchAsset = fetch) {
  const pathname = new URL(request.url).pathname;
  if (!pathname.startsWith('/stopwatch/install/releases/')) return null;
  if (!['GET', 'HEAD'].includes(request.method)) return new Response(null, { status: 405, headers: { Allow: 'GET, HEAD' } });
  if (!validConfig(config)) return unavailable();
  const prefix = `/stopwatch/install/releases/${config.tag}/`;
  const asset = config.assets.find(candidate => pathname === prefix + candidate.name);
  if (!asset) return new Response('Not found', { status: 404, headers: { 'Cache-Control': 'no-store' } });
  if (!validAsset(asset)) return new Response('This release is not ready to download.', {
    status: 503, headers: { 'Content-Type': 'text/plain; charset=utf-8', 'Cache-Control': 'no-store' },
  });

  const controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(), 20_000);
  try {
    const options = {
      method: 'GET', redirect: 'manual', credentials: 'omit', signal: controller.signal,
      headers: { Accept: 'application/octet-stream' },
    };
    let upstream = await fetchAsset(`https://github.com/${REPOSITORY}/releases/download/${config.tag}/${asset.name}`, options);
    if (REDIRECTS.has(upstream.status)) {
      const location = upstream.headers.get('Location');
      if (upstream.body) await upstream.body.cancel();
      if (!location) return unavailable();
      const destination = new URL(location);
      if (destination.protocol !== 'https:' || destination.hostname !== 'release-assets.githubusercontent.com' ||
          destination.port || destination.username || destination.password || destination.hash) return unavailable();
      upstream = await fetchAsset(destination.href, options);
    }
    const type = (upstream.headers.get('Content-Type') || '').split(';')[0].trim().toLowerCase();
    const types = asset.name === 'release.json' ? ['application/json', 'application/octet-stream'] : ['application/octet-stream'];
    const length = upstream.headers.get('Content-Length');
    if (upstream.status !== 200 || !types.includes(type) || !upstream.body ||
        (length !== null && (!/^\d+$/.test(length) || Number(length) !== asset.sizeBytes))) {
      if (upstream.body) await upstream.body.cancel();
      return unavailable();
    }
    const bytes = new Uint8Array(asset.sizeBytes);
    const reader = upstream.body.getReader();
    let written = 0;
    try {
      while (true) {
        const { done, value } = await reader.read();
        if (done) break;
        if (written + value.byteLength > bytes.byteLength) {
          await reader.cancel();
          return unavailable();
        }
        bytes.set(value, written); written += value.byteLength;
      }
    } finally { reader.releaseLock(); }
    if (written !== bytes.byteLength) return unavailable();
    const digest = Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256', bytes)), value => value.toString(16).padStart(2, '0')).join('');
    if (digest !== asset.sha256) return unavailable();
    return new Response(request.method === 'HEAD' ? null : bytes, {
      headers: {
        'Content-Type': asset.name === 'release.json' ? 'application/json; charset=utf-8' : 'application/octet-stream',
        'Content-Length': String(bytes.byteLength),
        'X-Content-Type-Options': 'nosniff',
        'Cache-Control': 'public, max-age=31536000, immutable',
        ETag: `"sha256-${asset.sha256}"`,
      },
    });
  } catch { return unavailable(); }
  finally { clearTimeout(timeout); }
}
