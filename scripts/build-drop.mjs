import { mkdir, readFile, writeFile } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
import { contentTypes } from '../src/http.mjs';

// Generate editable source from this repository's complete guide. Never use
// the size-limited source API or a served copy with injected platform scripts.
const root = fileURLToPath(new URL('../public/', import.meta.url));
const origin = 'https://stopwatch.workos.cloud';
const canonical = 'https://drop.workos.cloud/stopwatch';
const output = new URL('../.build/drop-stopwatch.html', import.meta.url);
let html = await readFile(path.join(root, 'stopwatch/index.html'), 'utf8');
const css = await readFile(path.join(root, 'stopwatch/style.css'), 'utf8');
let script = await readFile(path.join(root, 'stopwatch/app.js'), 'utf8');
script = script.replace("new URL('/stopwatch/SKILL.md', window.location.href).href", JSON.stringify(origin + '/stopwatch/SKILL.md'));
html = html.replace('<link rel="stylesheet" href="/stopwatch/style.css">', `<style>\n${css}\n</style>`)
  .replace('<script src="/stopwatch/app.js" defer></script>', '')
  .replace('</body>', `<script>\n${script}\n</script>\n</body>`)
  .replace('<span id="skill-address">/stopwatch/SKILL.md</span>', `<span id="skill-address">${origin}/stopwatch/SKILL.md</span>`)
  .replace(`rel="canonical" href="${origin}/stopwatch"`, `rel="canonical" href="${canonical}"`)
  .replace(`property="og:url" content="${origin}/stopwatch"`, `property="og:url" content="${canonical}"`);

const assetReferences = [...html.matchAll(/\b(src|srcset)="(\/stopwatch\/assets\/[^\"]+)"/g)];
for (const [reference, attribute, url] of assetReferences) {
  const pathname = new URL(url, origin).pathname;
  const bytes = await readFile(path.join(root, pathname));
  const type = contentTypes[path.extname(pathname)];
  if (!type || !type.startsWith('image/')) throw new Error(`Unsupported Drop image: ${pathname}`);
  html = html.replace(reference, `${attribute}="data:${type};base64,${bytes.toString('base64')}"`);
}
html = html.replace(/href="(\/(?!\/)[^\"]*)"/g, (_, href) => `href="${origin}${href}"`)
  .replace(/<a\b[^>]*href="https:\/\/stopwatch\.workos\.cloud\/stopwatch\/install\/"[^>]*>/g,
    tag => tag.slice(0, -1) + ' target="_blank" rel="noopener noreferrer">');
if (html.includes('src="/stopwatch/') || html.includes('srcset="/stopwatch/') || html.includes('href="/stopwatch/'))
  throw new Error('Unresolved guide resource in Drop source');
if (!html.includes('Install conference badge') || !html.includes('target="_blank"'))
  throw new Error('Drop installer link is missing its standalone window target');
await mkdir(new URL('../.build/', import.meta.url), { recursive: true });
await writeFile(output, html);
console.log(`Prepared editable Drop source: ${fileURLToPath(output)} (${Buffer.byteLength(html)} bytes)`);
console.log('No Drop was staged or published. Existing audience settings are unchanged.');
