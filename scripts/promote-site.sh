#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: scripts/promote-site.sh --commit <full-main-SHA> [--publish]

Verify a clean, approved workos/stopwatch main commit and its site/ subtree.
The default runs checks in a temporary checkout and changes no remote state.
--publish pushes the verified site to Alto main, which starts production deployment.
EOF
}
die() { printf 'Error: %s\n' "$*" >&2; exit 1; }
source_sha=''
publish=0
while [[ $# -gt 0 ]]; do
  case "$1" in
    --commit) [[ $# -ge 2 ]] || die '--commit requires a full SHA'; source_sha="$2"; shift 2 ;;
    --publish) publish=1; shift ;;
    --help|-h) usage; exit 0 ;;
    *) usage >&2; die "Unknown argument: $1" ;;
  esac
done
[[ "$source_sha" =~ ^[0-9a-f]{40}$ ]] || die 'Use --commit with the full 40-character main commit SHA.'
for command in git gh node npm alto; do command -v "$command" >/dev/null || die "Missing command: $command"; done
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)"
repository='workos/stopwatch'
alto_remote='https://git.workos.cloud/stopwatch.git'
[[ "$(git -C "$root" rev-parse --show-toplevel)" == "$root" ]] || die 'Run this script from its workos/stopwatch checkout.'
case "$(git -C "$root" remote get-url origin)" in
  git@github.com:workos/stopwatch.git|https://github.com/workos/stopwatch.git|https://github.com/workos/stopwatch|ssh://git@github.com/workos/stopwatch.git) ;;
  *) die 'origin must be the workos/stopwatch GitHub repository.' ;;
esac

assert_source() {
  [[ -z "$(git -C "$root" status --porcelain --untracked-files=all)" ]] || die 'The source checkout must be clean, including untracked files.'
  [[ "$(git -C "$root" rev-parse HEAD)" == "$source_sha" ]] || die 'HEAD must equal the requested commit.'
  git -C "$root" fetch --no-tags origin '+refs/heads/main:refs/remotes/origin/main'
  [[ "$(git -C "$root" rev-parse refs/remotes/origin/main)" == "$source_sha" ]] || die 'The requested commit must be the freshly fetched origin/main tip.'
}
assert_source
[[ "$(git -C "$root" cat-file -t "$source_sha:site")" == tree ]] || die 'The selected commit has no site/ tree.'
site_tree="$(git -C "$root" rev-parse "$source_sha:site")"

temporary="$(mktemp -d "${TMPDIR:-/tmp}/stopwatch-promotion.XXXXXX")"
publication_attempted=0
alto_sha=''
cleanup() {
  local result=$?
  if [[ "$result" -ne 0 && "$publication_attempted" == 1 ]]; then
    printf '\nPublication was attempted for %s. Inspect Alto build/hosting status before retrying; production may already have changed.\n' "$alto_sha" >&2
  fi
  rm -rf -- "$temporary"
  exit "$result"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

verify_review_and_ci() {
  gh api --paginate --slurp "repos/$repository/commits/$source_sha/pulls?per_page=100" > "$temporary/pulls.json"
  pr_number="$(node - "$temporary/pulls.json" "$source_sha" <<'JS'
const fs = require('node:fs');
const [file, sha] = process.argv.slice(2);
const matches = JSON.parse(fs.readFileSync(file, 'utf8')).flat().filter(pr =>
  pr.merged_at && pr.merge_commit_sha === sha && pr.base?.ref === 'main' && pr.base?.repo?.full_name === 'workos/stopwatch');
if (matches.length !== 1) throw new Error('Expected exactly one merged PR into workos/stopwatch main for this commit. Direct pushes/bootstrap commits cannot be promoted.');
if (!Number.isSafeInteger(matches[0].number) || matches[0].number < 1) throw new Error('Invalid PR number.');
process.stdout.write(String(matches[0].number));
JS
)"
  gh pr view "$pr_number" --repo "$repository" --json number,baseRefName,mergeCommit,mergedAt,reviewDecision,url > "$temporary/review.json"
  gh api --paginate --slurp "repos/$repository/commits/$source_sha/check-runs?filter=latest&per_page=100" > "$temporary/checks.json"
  node - "$temporary/review.json" "$temporary/checks.json" "$source_sha" <<'JS'
const fs = require('node:fs');
const [reviewFile, checksFile, sha] = process.argv.slice(2);
const pr = JSON.parse(fs.readFileSync(reviewFile, 'utf8'));
if (!pr.mergedAt || pr.baseRefName !== 'main' || pr.mergeCommit?.oid !== sha || pr.reviewDecision !== 'APPROVED')
  throw new Error('The exact merged main commit must have an APPROVED GitHub review decision.');
const runs = JSON.parse(fs.readFileSync(checksFile, 'utf8')).flatMap(page => page.check_runs);
for (const name of ['Browser checks', 'Site checks', 'Firmware host checks', 'Firmware build']) {
  const matches = runs.filter(run => run.name === name && run.head_sha === sha && run.app?.slug === 'github-actions');
  if (matches.length !== 1 || matches[0].status !== 'completed' || matches[0].conclusion !== 'success')
    throw new Error(`Required GitHub Actions check is missing, ambiguous, pending, or unsuccessful: ${name}`);
}
console.log(`Approved PR #${pr.number}; all four required checks passed on ${sha}.`);
JS
}
verify_review_and_ci

# Authentication stays in Alto's credential helper, never in a URL or file.
alto_git() {
  git -c credential.helper= -c 'credential.https://git.workos.cloud.helper=!alto auth git-credential' -c credential.https://git.workos.cloud.useHttpPath=true "$@"
}
staging="$temporary/stopwatch"
alto_git clone --quiet --single-branch --branch main "$alto_remote" "$staging"
alto_base="$(git -C "$staging" rev-parse HEAD)"
# Import objects locally, then use exactly site/'s reviewed tree. The publication
# commit retains Alto's existing parent; firmware history is not published.
git -C "$staging" fetch --quiet --no-tags "$root" "$source_sha"
git -C "$staging" read-tree --reset -u "$site_tree"
(
  cd "$staging"
  npm ci --ignore-scripts
  npm run check
  npm test
  npm run build
  alto doctor
)
git -C "$staging" diff --quiet || die 'Site checks changed tracked files.'
[[ -z "$(git -C "$staging" ls-files --others --exclude-standard)" ]] || die 'Site checks created unreviewed files.'
[[ "$(git -C "$staging" write-tree)" == "$site_tree" ]] || die 'The staged site differs from the reviewed subtree.'
[[ "$(git -C "$staging" rev-parse HEAD)" == "$alto_base" ]] || die 'Site checks changed the Alto base commit.'
printf '\nVerified GitHub commit: %s\nReviewed site tree: %s\nCurrent Alto base: %s\n' "$source_sha" "$site_tree" "$alto_base"
same_tree=0
if [[ "$(git -C "$staging" rev-parse HEAD^{tree})" == "$site_tree" ]]; then same_tree=1; fi
if [[ "$publish" == 0 ]]; then
  printf '\nVerification complete. No remote state changed. Re-run with --publish to deploy this exact commit.\n'
  if [[ "$same_tree" == 1 ]]; then
    printf 'Alto main already has this site tree; no new push is needed. Live deployment remains unverified.\n'
  fi
  exit 0
fi

# Re-read source and review/check state after the potentially lengthy local build.
assert_source
verify_review_and_ci
if [[ "$same_tree" == 1 ]]; then
  [[ "$(alto_git -C "$staging" ls-remote --exit-code "$alto_remote" refs/heads/main)" == "$alto_base"$'\t'refs/heads/main ]] || die 'Alto main changed; stop and re-verify.'
  alto_sha="$alto_base"
  printf 'Alto main already has this site tree. Verifying or promoting existing commit %s without another push.\n' "$alto_sha"
else
git -C "$staging" -c core.hooksPath=/dev/null commit --quiet -m "Publish site from workos/stopwatch $source_sha" -m "Reviewed-PR: https://github.com/workos/stopwatch/pull/$pr_number" -m "Source-Site-Tree: $site_tree"
alto_sha="$(git -C "$staging" rev-parse HEAD)"
[[ "$(git -C "$staging" rev-parse HEAD^)" == "$alto_base" ]] || die 'Publication must preserve the captured Alto parent.'
[[ "$(git -C "$staging" rev-parse HEAD^{tree})" == "$site_tree" ]] || die 'Publication tree does not match the reviewed site.'

# Compare with the server's advertised ref inside the normal Git push. A
# concurrent deployment causes refusal; no force-push or history rewrite is used.
mkdir -p "$temporary/hooks"
cat > "$temporary/hooks/pre-push" <<'HOOK'
#!/usr/bin/env bash
set -euo pipefail
count=0
while read -r local_ref local_sha remote_ref remote_sha; do
  [[ "$remote_ref" == refs/heads/main && "$local_sha" == "$PROMOTE_SITE_COMMIT" && "$remote_sha" == "$PROMOTE_SITE_BASE" ]] || {
    printf 'Alto main changed or the push contains an unexpected ref; stop and re-verify.\n' >&2
    exit 1
  }
  count=$((count + 1))
done
[[ "$count" == 1 ]]
HOOK
chmod 700 "$temporary/hooks/pre-push"
printf '\nPublishing %s to Alto main; this push starts production deployment.\n' "$alto_sha"
publication_attempted=1
PROMOTE_SITE_COMMIT="$alto_sha" PROMOTE_SITE_BASE="$alto_base" \
  alto_git -C "$staging" -c core.hooksPath="$temporary/hooks" -c push.followTags=false push "$alto_remote" "$alto_sha:refs/heads/main"
fi
publication_attempted=1
(
  cd "$staging"
  alto deploy stopwatch --sha "$alto_sha" --no-push --json
) | tee "$temporary/deployment.json"
node - "$temporary/deployment.json" "$alto_sha" <<'JS'
const fs = require('node:fs');
const [file, sha] = process.argv.slice(2);
const result = JSON.parse(fs.readFileSync(file, 'utf8'));
if (result.sha !== sha || result.outcome !== 'deployed' || result.verification?.verified !== true || result.publicEdge?.matches !== true)
  throw new Error('Alto has not verified the exact publication commit at the production edge.');
console.log(`Verified production release ${result.release.releaseNumber} (${result.release.id}) for ${sha}.`);
JS
printf 'GitHub source remains %s; firmware binaries and release pins were not independently changed by this script.\n' "$source_sha"
