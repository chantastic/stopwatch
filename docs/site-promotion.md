# Promote the website to Alto

WorkOS GitHub owns reviewable source in `workos/stopwatch`. The website lives in
`site/`; firmware remains at the repository root. Alto continues serving the
existing `stopwatch` application from `https://git.workos.cloud/stopwatch.git`.
GitHub CI tests changes but does not hold Alto credentials or deploy the site.

## Review, then promote

1. Make changes on a branch and open a pull request into `main`.
2. Obtain an approved review and pass all four required checks: **Browser checks**,
   **Site checks**, **Firmware host checks**, and **Firmware build**.
3. Merge the pull request. Wait for those four checks to pass on the resulting
   `main` commit as well. This repository uses squash merges: select the resulting
   main commit, not the former branch tip.
4. Fetch and check out that exact commit in a clean local checkout whose `origin`
   is `workos/stopwatch`. The script requires a full 40-character SHA, and it must
   still be the latest `origin/main` commit.
5. Verify locally, then explicitly publish the same commit:

   ```sh
   scripts/promote-site.sh --commit <full-main-SHA>
   scripts/promote-site.sh --commit <full-main-SHA> --publish
   ```

Use Node.js 24 (matching CI), npm, Git, GitHub CLI (`gh auth login`), and Alto CLI (`alto login`).
The operator needs GitHub source/review/check access and Alto deployment authority.
Alto authentication uses its short-lived Git credential helper. No credential is
written to source, embedded in a remote URL, or stored in GitHub Actions.

The bootstrap import has no merged approved pull request and cannot be promoted
by this command. Importing the repository does not change the live site.

## What the command verifies

The default invocation changes no remote state. It fetches GitHub main and reads
GitHub review/check metadata, then uses a temporary Alto checkout to install the
committed npm lockfile and run the site's syntax checks, tests, bundle build, and
`alto doctor`. Temporary files are removed on exit. Existing ignored build files
in the source checkout are allowed; tracked changes or untracked source files are
not.

The script requires one merged pull request targeting `workos/stopwatch`'s
`main`, whose `merge_commit_sha` exactly matches the selected commit and whose
review decision remains `APPROVED`. All four named GitHub Actions checks must
have completed successfully on that exact SHA; missing, pending, failed,
skipped, or ambiguous check results refuse promotion. It repeats source and
review/check verification after local checks when `--publish` is selected.

The checked `site/` Git tree is placed on top of the current Alto `main` history.
A new publication commit records the source SHA, pull-request URL, and site-tree
ID. Its tree must match the reviewed subtree exactly. The script does not push
firmware history or root-only files to Alto. If Alto already has the exact site
tree, it makes no new commit or push. Default verification still reports live
deployment as unverified; `--publish` checks the existing Alto SHA through the
same deploy/readiness flow, promoting it if necessary. Matching Git content
alone does not prove a previous build succeeded or that production still serves it.

A temporary pre-push hook compares the server's advertised Alto main SHA with
the captured base and accepts only the expected new commit targeting `main`.
Concurrent Alto changes cause refusal. There is no force-push or history rewrite.
The command then follows that exact publication SHA with `alto deploy --no-push`
and requires verified production routing and an edge response for its release.

## A push is a production action

**Pushing Alto `main` starts its managed build and can automatically deploy it.**
The later `alto deploy --no-push` call waits for and verifies the selected commit;
it is not an approval gate that prevents the preceding push from going live.
The `--publish` flag is the deliberate boundary before the push.

Alto currently permits only `main` writes and has no application pull-request
workflow or public per-app switch to disable automatic promotion. Keep code
review in GitHub. The script does not alter Alto sharing, grants, secrets, public
surfaces, the `drops` redirect app, or the existing singular Drop.

## Firmware releases remain separate

Promoting a website does not compile, flash, tag, or upload firmware. The site's
`src/release-config.json` controls the public release repository, tag, filenames,
sizes, and SHA-256 pins. The managed site build downloads and validates those
public assets before embedding them into ignored generated Worker content.
Neither firmware binaries nor generated release content enter Git.

A firmware release update needs its own validated firmware build and published
release assets, followed by a reviewed update to the site pins and installer
bundle. Private GitHub source does not make public installer downloads private;
this promotion command does not change their existing publication location.

## After publication or a failure

Check the installer, downloads, and issue link at
<https://drops.workos.cloud/stopwatch>. Use the [Alto releases page](https://workos.cloud/applications/stopwatch?tab=releases)
and `alto hosting stopwatch --json` to inspect production. Release promotion and
rollback select an existing managed release; they do not reverse GitHub source
or firmware downloads.

If the push or deployment verification fails, inspect the printed publication
SHA and Alto's build/hosting state before retrying. A timeout can occur after
production has changed. The command never automatically rolls back a release.
If GitHub or Alto main advanced during verification, fetch the new state and
review it before starting again; do not bypass the guard with a force push.

Older standalone website checkouts are retained as deployment history, not a
second place to author changes. Future source edits belong in this repository's
`site/` directory.
