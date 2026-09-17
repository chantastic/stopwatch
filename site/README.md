# WorkOS init() StopWatch guide

Canonical source is the `site/` directory in
[chantastic/stopwatch](https://github.com/chantastic/stopwatch). Test changes and
commit directly to `main`; pull requests are not required. Source pushes do not
deploy the website. The former WorkOS publisher is retained as legacy, and a
replacement publisher is pending; see [publication status](../docs/site-promotion.md).
The older standalone checkout remains publication history/reference.

Static technical reference plus a downloadable agent skill. The site follows system light/dark appearance through CSS, including the official logos, without requiring JavaScript or a saved preference. Site source uses no external scripts, fonts, analytics, or runtime dependencies; the hosting layer may inject its own analytics beacon.

## Run

Node.js 20 or newer:

```sh
npm start
```

Visit `http://localhost:4173/`. The server binds to `0.0.0.0` and reads `PORT` (default `4173`) and optional `HOST`.

## Routes

- `/` serves the technical reference directly at the subdomain root.
- `/stopwatch` and `/stopwatch/` retain anonymous access to the same page.
- `/stopwatch/SKILL.md` serves the skill as Markdown.
- `/llms.txt` provides a concise resource index for agents.
- `/stopwatch/install/` serves the standalone Chrome installer. Opening
  `/stopwatch/install` redirects to the directory URL for stable relative assets.
- `/stopwatch/install/releases/conference-factory-3/<file>` serves the four
  pinned release downloads: `release.json`, `bootloader.bin`,
  `partition-table.bin`, and `firmware.bin`.

## Conference installer

The root `web-flasher/` project produces the tested static installer bundle;
copy its `index.html`, `style.css`, `installer.js`, and `THIRD_PARTY_NOTICES.txt`
into `public/stopwatch/install/`. Keep the hardware protocol and flashing logic
in `web-flasher/`; this directory owns hosting and the existing guide.

Firmware bytes are GitHub Release assets in
`chantastic/m5stack-stopwatch-authkit`, tag `conference-factory-3`. They must
never be checked into this repository. `src/release-config.json` pins each
filename, byte length, and SHA-256 from the approved package, including the
manifest itself. To publish a new release, update those pins and the installer
version together after qualifying and publishing the new firmware package.
Moving source to `chantastic/stopwatch` does not change these artifact URLs.

The managed build and local preview share `src/release-assets.mjs`. It permits
only the configured release paths, forwards no browser credentials or headers,
allows one redirect to GitHub's exact HTTPS release-asset host, bounds each
download, and checks its type, size, and SHA-256. The managed build fails if any
file cannot be verified. It embeds the four verified responses in the ignored
`src/content.generated.ts`; binaries and generated release data never enter Git.
The deployed Worker serves those bytes directly with immutable versioned URLs
and SHA-256 ETags, with no runtime network request. HEAD returns the same metadata
without a body. The local Node preview verifies upstream responses on request.
The public surface remains `/stopwatch`; no existing sharing policy is changed.

Alto routes tenant Workers' outbound requests through its egress policy, which
blocked direct GitHub downloads in the first hosted build. Fetching during the
managed build avoids expanding that policy and removes GitHub availability from
the visitor's install path. Tests verify every generated release response against
its pinned size and hash with runtime `fetch` disabled.

The installer must run as a top-level page because Alto Drop content is in an
opaque sandboxed iframe without Web Serial permission. The guide and Drop link
to this standalone installer.

`npm run build:drop` creates `.build/drop-stopwatch.html` from the complete
editable guide, inlining its existing image assets, CSS, and guide script. It
does not use the truncated source API response or copy the injected platform
runtime. The installer link opens a separate tab. Generation makes no service
calls and changes no audience settings. After reviewing that file and the
deployed installer, stage and publish the existing `stopwatch` Drop explicitly
with Alto; do not create a replacement Drop or broaden its audience.

## Alto deployment

The production entry is `src/index.ts`, a Cloudflare Worker. `npm run build` packages the files under `public/` into its responses and validates the Worker bundle. `server.mjs` remains the local preview server; Node.js is not the production runtime.

`alto.json` declares `/stopwatch` and `/llms.txt` as public surfaces. Everything under `/stopwatch`, including both official logos and `SKILL.md`, is available without sign-in. The page also lives at `https://stopwatch.workos.cloud/`, where Alto currently requires authentication. Alto rejects `"/": "public"` with `public-root-surface`; the public `/stopwatch` alias is retained until the platform supports anonymous root surfaces. Asset and skill URLs remain stable under `/stopwatch`.

Alto application: `01M2P6VWDZ9GVRJH1ACT7RN7F5` (`stopwatch`). Publication remote:
`https://git.workos.cloud/stopwatch.git`. Git credentials come from
`alto auth git-credential`; never put them in the remote URL. The retained
`scripts/promote-site.sh` only supports the former WorkOS repository and review
workflow; it cannot publish `chantastic/stopwatch`. A replacement publisher is
pending, as documented in [publication status](../docs/site-promotion.md).
An Alto main push starts a managed build and can deploy production immediately.
Any future publication must verify the tested site tree, release pins, hosting,
and anonymous public URLs. Do not run `wrangler deploy` without `--dry-run`.

After verifying the standalone installer, publish the generated HTML to the
existing Drop with `alto drops stage stopwatch --file .build/drop-stopwatch.html --json`,
review the returned candidate, then `alto drops publish stopwatch --version <version> --json`.
These commands update content only. Keep the Drop's existing organization audience.

Local checks:

```sh
npm ci --ignore-scripts
npm run check
npm test
npm run build
npx wrangler deploy --dry-run --outdir .alto-bundle
alto doctor
```

Alto CLI 0.5.9 derives the local application slug from the directory name.
For a checkout named `stopwatch.workos.cloud`, run `alto doctor` against a
temporary symlink named `stopwatch`; otherwise it reports a missing slug even
though the unchanged manifest is valid for the registered `stopwatch` app.
Do not rename the application to match the checkout directory.

## Content and assets

### Sharing previews

Share `https://stopwatch.workos.cloud/stopwatch`. Alto requires authentication at the root, so anonymous unfurlers cannot read its metadata. Canonical and Open Graph URLs therefore identify the public alias. Both page variants include static Open Graph and X large-image-card tags near the beginning of the HTML head.

`public/stopwatch/og.png` is a 1200 × 630 PNG composed from the exact official WorkOS and init() vector logos and the existing device illustration. Its editable composition is `design/og-card.svg`; the original logo path geometry is preserved. No new logos were generated. The image and page are public, require no JavaScript, and use fixed production HTTPS URLs. Image links include `?v=2` to avoid reusing the earlier illustration from image caches.

Slack fetches page metadata and referenced media and caches responses for approximately 30 minutes. Validate with crawler-style HTTP requests; this is distinct from posting into Slack and observing its final rendering. Sources: [Slack Robots](https://api.slack.com/robots), [Open Graph protocol](https://ogp.me/), and [LinkedIn sharing requirements](https://www.linkedin.com/help/linkedin/answer/a521928).

Hardware, toolchain, and recovery details link to M5Stack's official sources. Community project links and the source device illustration are from [ESPtember](https://esptember.com/). Artwork source: <https://esptember.com/images/boards/m5stack-stopwatch.svg>, by ESPtember / chantastic. The displayed `stopwatch-manual.png` was corrected with built-in ImageGen using the user-provided product photographs: a head-on view, a slim black bezel, and rectangular yellow (10 o’clock) and blue (2 o’clock) pushers. The pale purple timer graphic follows the page theme. The original SVG remains available. Generation prompts are recorded in [docs/illustration-prompt.md](docs/illustration-prompt.md). The page includes visible attribution; this package does not assign a new license to that artwork.

The public skill is a downloadable resource, not an installed local skill. Source review date: 2026-09-16. No physical board was flashed or tested while preparing the guide.

## Official brand assets

- `public/stopwatch/assets/workos-logo.svg`: unchanged black symbol and wordmark from the official WorkOS logo download linked on [workos.com](https://workos.com/). Archive: [WorkOS Logos.zip](https://cdn.prod.website-files.com/621f54116cab10f6e9215d8b/6908e076c85d9ad71e6c6b6b_WorkOS%20Logos.zip), entry `WorkOS Logos/SVG/WorkOS_Lockup_Black.svg`.
- `public/stopwatch/assets/workos-logo-white.svg`: unchanged white lockup from the same archive, entry `WorkOS Logos/SVG/WorkOS_Lockup_White.svg`, selected automatically in dark mode.
- `public/stopwatch/assets/init-logo.svg`: unchanged SVG from the official [init() event page](https://workos.com/init). Both logos retain their original geometry and proportions.

The monochrome init() logo is inverted to white with CSS in dark mode; the SVG itself is unchanged.

Retrieved 2026-09-16. These remain WorkOS brand assets; no new license is assigned by this project.
