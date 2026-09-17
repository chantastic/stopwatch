import { readdir, readFile, writeFile } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
import { contentTypes } from '../src/http.mjs';
import { releaseAsset } from '../src/release-assets.mjs';

const publicRoot = fileURLToPath(new URL('../public/', import.meta.url));
const content = {};
async function collect(directory) {
  for (const entry of await readdir(directory, { withFileTypes: true })) {
    const file = path.join(directory, entry.name);
    if (entry.isDirectory()) await collect(file);
    else if (entry.isFile()) {
      const type = contentTypes[path.extname(file)];
      if (!type) throw new Error(`Unsupported public asset: ${file}`);
      const route = '/' + path.relative(publicRoot, file).split(path.sep).join('/');
      const binary = path.extname(file) === '.png';
      content[route] = {
        type,
        body: (await readFile(file)).toString(binary ? 'base64' : 'utf8'),
        ...(binary ? { encoding: 'base64' } : {}),
      };
    }
  }
}
await collect(publicRoot);
// Managed Alto Workers restrict runtime egress. Download the public, pinned
// release during the managed build, verify every byte with the same bounded
// verifier used by local preview, and embed only its verified response.
const releaseConfig = JSON.parse(await readFile(new URL('../src/release-config.json', import.meta.url), 'utf8'));
await Promise.all(releaseConfig.assets.map(async asset => {
  const route = `/stopwatch/install/releases/${releaseConfig.tag}/${asset.name}`;
  const response = await releaseAsset(new Request(`https://stopwatch.workos.cloud${route}`), releaseConfig);
  if (response.status !== 200) throw new Error(`Could not verify release asset ${asset.name}: ${response.status}`);
  content[route] = {
    type: response.headers.get('Content-Type'),
    body: Buffer.from(await response.arrayBuffer()).toString('base64'),
    encoding: 'base64',
    sha256: asset.sha256,
  };
}));
await writeFile(new URL('../src/content.generated.ts', import.meta.url),
  '// Generated from public/ and verified GitHub release assets during the build.\nexport const content: Record<string, { type: string; body: string; encoding?: "base64"; sha256?: string }> = ' +
  JSON.stringify(content, null, 2) + ';\n');
console.log(`Prepared ${Object.keys(content).length} public files for the Worker.`);
