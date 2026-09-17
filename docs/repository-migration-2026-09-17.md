# WorkOS team repository — September 17, 2026

Canonical source is now private [workos/stopwatch](https://github.com/workos/stopwatch).
The repository inherits WorkOS organization access. No individual or team grants
were added during setup.

## Imported source

- Firmware and browser installer retain their existing Git history through
  `93f6e38dc6506cf0f756dd0677a09714257124c4` from the prior
  `chantastic/m5stack-stopwatch-authkit` conference branch.
- The website is imported into `site/` from Alto source commit
  `4ab7b1d0908bdf2359f083d1520d9f6e9125a9f5`, using a squashed Git subtree import
  that records its original commit.
- Only tracked source was imported. Local dependencies, untracked generated build output,
  firmware binaries, device backups, cached profiles and private observations
  were not copied into this repository.

The original repositories and published firmware release remain available. The
current browser installer still serves `conference-factory-3`; creating this
repository does not update the badge or deploy the website.

## Contribution and publication

Use issues for requests and pull requests for source changes. The main branch
requires review and automated checks. Firmware host checks and compilation do not
replace a hardware acceptance run before releasing a new binary.

Website publication is a separate maintainer action through the
[Alto promotion runbook](site-promotion.md). The managed Alto repository remains
the deployment target; its main branch is not a feature-review branch.

Current public firmware assets remain in the original GitHub release repository.
The [release runbook](web-releases.md) explains that distribution boundary and its
current maintainer dependency. A private GitHub repository cannot serve those
downloads anonymously by merely changing their URLs.
