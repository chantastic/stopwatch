#!/usr/bin/env python3
"""Exercise the production promotion script with local Git and mocked services.

No GitHub/Alto request is made. Even --publish pushes only to a disposable local
bare repository. The script's review, exact-tree, and pre-push guards are real.
"""
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / "scripts/promote-site.sh"
REAL_GIT = shutil.which("git")
CHECKS = ["Browser checks", "Site checks", "Firmware host checks", "Firmware build"]

GIT_STUB = r'''#!/usr/bin/env python3
import os,sys
args=sys.argv[1:]
if 'fetch' in args and 'origin' in args:
    args[args.index('origin')]=os.environ['PROMOTION_TEST_GITHUB']
args=[os.environ['PROMOTION_TEST_ALTO'] if arg=='https://git.workos.cloud/stopwatch.git' else arg for arg in args]
if 'push' in args:
    with open(os.environ['PROMOTION_TEST_LOG'],'a') as out: out.write('git push\n')
os.execv(os.environ['PROMOTION_TEST_GIT'],[os.environ['PROMOTION_TEST_GIT'],*args])
'''
GH_STUB = r'''#!/usr/bin/env python3
import json,os,sys
args=sys.argv[1:]; sha=os.environ['PROMOTION_TEST_SHA']; mode=os.environ.get('PROMOTION_TEST_MODE','')
if args[0]=='api' and '/pulls?' in args[-1]:
    value=[[]] if mode=='no-pr' else [[{'number':7,'merged_at':'2026-09-17T12:00:00Z','merge_commit_sha':sha,'base':{'ref':'main','repo':{'full_name':'workos/stopwatch'}}}]]
elif args[0]=='pr':
    value={'number':7,'mergedAt':'2026-09-17T12:00:00Z','mergeCommit':{'oid':sha},'baseRefName':'main','reviewDecision':'REVIEW_REQUIRED' if mode=='unapproved' else 'APPROVED','url':'https://github.com/workos/stopwatch/pull/7'}
elif args[0]=='api' and '/check-runs?' in args[-1]:
    runs=[{'name':name,'head_sha':sha,'app':{'slug':'github-actions'},'status':'completed','conclusion':'success'} for name in ['Browser checks','Site checks','Firmware host checks','Firmware build']]
    if mode=='missing-check': runs.pop()
    if mode=='failed-check': runs[-1]['conclusion']='failure'
    if mode=='wrong-sha-check': runs[-1]['head_sha']='0'*40
    if mode=='duplicate-check': runs.append(runs[-1])
    value=[{'check_runs':runs}]
else: raise SystemExit('Unexpected gh invocation: '+repr(args))
print(json.dumps(value))
'''
NPM_STUB = r'''#!/usr/bin/env python3
import os,subprocess,sys,tempfile
from pathlib import Path
args=sys.argv[1:]; mode=os.environ.get('PROMOTION_TEST_MODE','')
with open(os.environ['PROMOTION_TEST_LOG'],'a') as out: out.write('npm '+' '.join(args)+'\n')
if mode=='local-check-failed' and args==['run','check']: raise SystemExit(9)
if args==['run','build']:
    if mode=='changed-tree': Path('README.md').write_text('unreviewed mutation\n')
    if mode in ('stale-alto','stale-github'):
        remote=os.environ['PROMOTION_TEST_ALTO' if mode=='stale-alto' else 'PROMOTION_TEST_GITHUB']
        git=os.environ['PROMOTION_TEST_GIT']
        with tempfile.TemporaryDirectory() as writer:
            subprocess.run([git,'clone','--quiet',remote,writer],check=True)
            subprocess.run([git,'-C',writer,'commit','--quiet','--allow-empty','-m','Concurrent change'],check=True)
            subprocess.run([git,'-C',writer,'push','--quiet','origin','HEAD:main'],check=True)
'''
ALTO_STUB = r'''#!/usr/bin/env python3
import json,os,sys
args=sys.argv[1:]
with open(os.environ['PROMOTION_TEST_LOG'],'a') as out: out.write('alto '+' '.join(args)+'\n')
if args==['doctor']: print(json.dumps({'status':'pass'}))
elif args[:2]==['deploy','stopwatch']:
    if os.environ.get('PROMOTION_TEST_MODE')=='deploy-failed': raise SystemExit('Mock previous build failed')
    sha=args[args.index('--sha')+1]
    print(json.dumps({'sha':sha,'outcome':'deployed','verification':{'verified':True},'publicEdge':{'matches':True},'release':{'releaseNumber':9,'id':'test-release'}}))
else: raise SystemExit('Unexpected alto invocation')
'''

@unittest.skipUnless(REAL_GIT and shutil.which("node"), "Git and Node.js are required")
class PromotionTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="test-site-promotion-")
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.root = self.base / "source"
        self.github = self.base / "github.git"
        self.alto = self.base / "alto.git"
        self.log = self.base / "calls.log"
        self.env = os.environ.copy()
        self.env.update({
            "GIT_CONFIG_NOSYSTEM": "1", "GIT_CONFIG_GLOBAL": str(self.base / "empty-git-config"),
            "GIT_AUTHOR_NAME": "Promotion test", "GIT_AUTHOR_EMAIL": "promotion@example.invalid",
            "GIT_COMMITTER_NAME": "Promotion test", "GIT_COMMITTER_EMAIL": "promotion@example.invalid",
            "PROMOTION_TEST_GIT": REAL_GIT, "PROMOTION_TEST_GITHUB": str(self.github),
            "PROMOTION_TEST_ALTO": str(self.alto), "PROMOTION_TEST_LOG": str(self.log),
        })
        self.git("init", "--quiet", "--bare", "--initial-branch=main", str(self.github))
        self.git("init", "--quiet", "--initial-branch=main", str(self.root))
        (self.root / "scripts").mkdir()
        shutil.copy2(SCRIPT, self.root / "scripts/promote-site.sh")
        (self.root / "site").mkdir()
        (self.root / "site/README.md").write_text("Reviewed site\n")
        (self.root / "firmware.txt").write_text("Firmware must stay out of Alto\n")
        self.git("-C", str(self.root), "add", ".")
        self.git("-C", str(self.root), "commit", "--quiet", "-m", "Reviewed monorepo")
        self.sha = self.git("-C", str(self.root), "rev-parse", "HEAD").strip()
        self.env["PROMOTION_TEST_SHA"] = self.sha
        self.git("-C", str(self.root), "remote", "add", "origin", "https://github.com/workos/stopwatch.git")
        self.git("-C", str(self.root), "push", "--quiet", str(self.github), "HEAD:main")
        writer = self.base / "initial-alto"
        self.git("init", "--quiet", "--initial-branch=main", str(writer))
        (writer / "README.md").write_text("Previous deployed site\n")
        self.git("-C", str(writer), "add", ".")
        self.git("-C", str(writer), "commit", "--quiet", "-m", "Existing Alto history")
        self.git("clone", "--quiet", "--bare", str(writer), str(self.alto))
        self.alto_before = self.git("--git-dir", str(self.alto), "rev-parse", "main").strip()
        self.mock_bin = self.base / "bin"
        self.mock_bin.mkdir()
        for name, body in {"git": GIT_STUB, "gh": GH_STUB, "npm": NPM_STUB, "alto": ALTO_STUB}.items():
            executable = self.mock_bin / name
            executable.write_text(body)
            executable.chmod(0o755)
        self.env["PATH"] = str(self.mock_bin) + os.pathsep + self.env["PATH"]

    def git(self, *args):
        result = subprocess.run([REAL_GIT, *args], env=self.env, capture_output=True, text=True)
        if result.returncode:
            self.fail(result.stderr)
        return result.stdout

    def run_script(self, mode="", publish=False):
        env = {**self.env, "PROMOTION_TEST_MODE": mode}
        command = ["bash", str(self.root / "scripts/promote-site.sh"), "--commit", self.sha]
        if publish:
            command.append("--publish")
        return subprocess.run(command, cwd=self.root, env=env, capture_output=True, text=True, timeout=30)

    def calls(self):
        return self.log.read_text() if self.log.exists() else ""

    def test_default_verifies_without_pushing_or_changing_source(self):
        result = self.run_script()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("Verification complete. No remote state changed", result.stdout)
        self.assertNotIn("git push", self.calls())
        self.assertNotIn("alto deploy", self.calls())
        self.assertEqual(self.git("-C", str(self.root), "status", "--porcelain"), "")
        self.assertEqual(self.git("--git-dir", str(self.alto), "rev-parse", "main").strip(), self.alto_before)

    def test_dirty_checkout_refuses_before_services_or_push(self):
        (self.root / "unreviewed.txt").write_text("dirty")
        result = self.run_script(publish=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("must be clean", result.stderr)
        self.assertEqual(self.calls(), "")

    def test_unreviewed_bootstrap_and_failed_checks_never_push(self):
        for mode in ["no-pr", "unapproved", "missing-check", "failed-check", "wrong-sha-check", "duplicate-check", "local-check-failed", "changed-tree"]:
            with self.subTest(mode=mode):
                self.log.unlink(missing_ok=True)
                result = self.run_script(mode, publish=True)
                self.assertNotEqual(result.returncode, 0, mode)
                self.assertNotIn("git push", self.calls())
                self.assertNotIn("alto deploy", self.calls())
                self.assertEqual(self.git("--git-dir", str(self.alto), "rev-parse", "main").strip(), self.alto_before)

    def test_new_github_main_after_checks_refuses_publication(self):
        result = self.run_script("stale-github", publish=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("freshly fetched origin/main tip", result.stderr)
        self.assertNotIn("git push", self.calls())
        self.assertNotIn("alto deploy", self.calls())

    def test_concurrent_alto_change_is_rejected_by_real_pre_push_hook(self):
        result = self.run_script("stale-alto", publish=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Alto main changed", result.stderr)
        self.assertNotIn("alto deploy", self.calls())
        self.assertEqual(self.git("--git-dir", str(self.alto), "show", "main:README.md"), "Previous deployed site\n")

    def test_explicit_publish_pushes_only_reviewed_site_and_preserves_parent(self):
        result = self.run_script(publish=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(self.git("--git-dir", str(self.alto), "rev-parse", "main^").strip(), self.alto_before)
        expected_tree = self.git("-C", str(self.root), "rev-parse", self.sha + ":site").strip()
        self.assertEqual(self.git("--git-dir", str(self.alto), "rev-parse", "main^{tree}").strip(), expected_tree)
        self.assertEqual(self.git("--git-dir", str(self.alto), "ls-tree", "--name-only", "main").strip(), "README.md")
        self.assertIn("Reviewed-PR: https://github.com/workos/stopwatch/pull/7", self.git("--git-dir", str(self.alto), "log", "-1", "--format=%B", "main"))
        self.assertIn("Verified production release 9", result.stdout)
        self.assertEqual(self.calls().count("git push\n"), 1)

    def match_existing_alto_tree(self):
        writer = self.base / "initial-alto"
        (writer / "README.md").write_text("Reviewed site\n")
        self.git("-C", str(writer), "add", ".")
        self.git("-C", str(writer), "commit", "--quiet", "-m", "Previously pushed reviewed site")
        self.git("-C", str(writer), "push", "--quiet", str(self.alto), "HEAD:main")
        return self.git("--git-dir", str(self.alto), "rev-parse", "main").strip()

    def test_same_tree_default_does_not_claim_live_readiness(self):
        self.match_existing_alto_tree()
        result = self.run_script()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("Live deployment remains unverified", result.stdout)
        self.assertNotIn("alto deploy", self.calls())
        self.assertNotIn("git push", self.calls())

    def test_same_tree_publish_verifies_existing_commit_without_another_push(self):
        existing_sha = self.match_existing_alto_tree()
        result = self.run_script(publish=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn(f"alto deploy stopwatch --sha {existing_sha} --no-push --json", self.calls())
        self.assertIn("Verified production release 9", result.stdout)
        self.assertNotIn("git push", self.calls())
        self.assertEqual(self.git("--git-dir", str(self.alto), "rev-parse", "main").strip(), existing_sha)

    def test_same_tree_previous_failed_deploy_is_not_reported_as_success(self):
        existing_sha = self.match_existing_alto_tree()
        result = self.run_script("deploy-failed", publish=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Mock previous build failed", result.stderr)
        self.assertNotIn("Verified production release", result.stdout)
        self.assertNotIn("git push", self.calls())
        self.assertEqual(self.git("--git-dir", str(self.alto), "rev-parse", "main").strip(), existing_sha)

    def test_same_tree_concurrent_alto_change_prevents_promotion(self):
        self.match_existing_alto_tree()
        result = self.run_script("stale-alto", publish=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Alto main changed", result.stderr)
        self.assertNotIn("alto deploy", self.calls())

if __name__ == "__main__":
    unittest.main()
