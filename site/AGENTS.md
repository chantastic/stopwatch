# Alto app

This directory is the website portion of `workos/stopwatch`. Edit it in a GitHub
pull request. The standalone Alto repository is a publication target; use the
root `scripts/promote-site.sh` and `docs/site-promotion.md` after review. A direct
push to Alto `main` deploys production. Do not bypass the team review workflow.

This repository is an Alto app. Alto handles identity, hosting, storage, secrets, builds, and releases.

Use Alto MCP tools by default when they are available. Use the Alto CLI when the work is already happening in a terminal.

Install and sign in:

```bash
curl -fsSL https://cli.workos.cloud | sh
alto login
```

Before guessing an Alto command, load Alto's published MCP skill or run `alto --help` for the current command surface.
