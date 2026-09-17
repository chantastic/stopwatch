import { content } from './content.generated';
import { Buffer } from 'node:buffer';
import { publicRoute, securityHeaders } from './http.mjs';

// Alto gates the root and permits anonymous requests on the public surfaces
// in alto.json. This static guide does not read or require a user identity.
export default {
  async fetch(request: Request): Promise<Response> {
    if (request.method !== 'GET' && request.method !== 'HEAD') {
      return new Response(null, { status: 405, headers: { Allow: 'GET, HEAD' } });
    }
    const url = new URL(request.url);
    if (url.pathname === '/stopwatch/install') return new Response(null, {
      status: 308, headers: { ...securityHeaders, Location: '/stopwatch/install/' },
    });
    const route = publicRoute(url.pathname);
    const asset = Object.hasOwn(content, route) ? content[route] : undefined;
    if (!asset) {
      return new Response(request.method === 'HEAD' ? null : 'Not found', {
        status: 404,
        headers: { ...securityHeaders, 'Content-Type': 'text/plain; charset=utf-8' },
      });
    }
    const body = request.method === 'HEAD' ? null : asset.encoding === 'base64'
      ? new Uint8Array(Buffer.from(asset.body, 'base64'))
      : asset.body;
    return new Response(body, {
      headers: {
        ...securityHeaders,
        'Content-Type': asset.type,
        ...(asset.sha256 ? {
          'Cache-Control': 'public, max-age=31536000, immutable',
          ETag: `"sha256-${asset.sha256}"`,
        } : {}),
      },
    });
  },
};
