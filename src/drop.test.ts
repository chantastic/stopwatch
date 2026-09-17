import { execFileSync } from 'node:child_process';
import { readFile } from 'node:fs/promises';
import { describe, expect, it } from 'vitest';

describe('Editable Drop source', () => {
  it('preserves the complete guide and assets while opening the installer outside the sandbox', async () => {
    execFileSync(process.execPath, ['scripts/build-drop.mjs'], { cwd: new URL('..', import.meta.url) });
    const html = await readFile(new URL('../.build/drop-stopwatch.html', import.meta.url), 'utf8');
    for (const section of ['start', 'hardware', 'resources', 'agents']) expect(html).toContain(`id="${section}"`);
    expect(html).toContain('href="https://drop.workos.cloud/stopwatch"');
    expect(html).toContain('href="https://stopwatch.workos.cloud/stopwatch/SKILL.md"');
    expect(html).toContain('const skillURL = "https://stopwatch.workos.cloud/stopwatch/SKILL.md"');
    const links = [...html.matchAll(/<a\b[^>]*href="https:\/\/stopwatch\.workos\.cloud\/stopwatch\/install\/"[^>]*>/g)];
    expect(links.length).toBeGreaterThan(0);
    for (const [link] of links) expect(link).toContain('target="_blank" rel="noopener noreferrer"');
    expect(html).not.toMatch(/(?:src|srcset|href)="\/stopwatch\//);
    expect(html).not.toContain('drop-content');
    expect(html).not.toContain('cloudflareinsights.com');
    const image = html.match(/src="data:image\/png;base64,([A-Za-z0-9+/=]+)"/);
    expect(image).not.toBeNull();
    expect(Buffer.from(image![1], 'base64')).toEqual(await readFile(new URL('../public/stopwatch/assets/stopwatch-manual.png', import.meta.url)));
  });
});
