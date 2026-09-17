# Alto app

This directory is the website portion of canonical `chantastic/stopwatch`.
Test changes and commit directly to `main`; do not open pull requests unless the
user asks. The standalone Alto repository remains a publication target. The
retained root `scripts/promote-site.sh` requires the former WorkOS repository and
review workflow and cannot publish this repo. A replacement publisher is pending;
read `docs/site-promotion.md` before publication work. A direct push to Alto
`main` starts a production build and can deploy immediately. Pushing source or
flashing a device does not publish the site.

This repository is an Alto app. Alto handles identity, hosting, storage, secrets, builds, and releases.

Use Alto MCP tools by default when they are available. Use the Alto CLI when the work is already happening in a terminal.

Install and sign in:

```bash
curl -fsSL https://cli.workos.cloud | sh
alto login
```

Before guessing an Alto command, load Alto's published MCP skill or run `alto --help` for the current command surface.
