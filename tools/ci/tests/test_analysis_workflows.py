# SPDX-FileCopyrightText: (C) 2026 Ieum contributors
# SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception

from __future__ import annotations

import re
import unittest
from pathlib import Path

REPOSITORY_ROOT = Path(__file__).resolve().parents[3]
WORKFLOW_DIRECTORY = REPOSITORY_ROOT / ".github" / "workflows"
ACTION_DIRECTORY = REPOSITORY_ROOT / ".github" / "actions"


def read_workflow(name: str) -> str:
    return (WORKFLOW_DIRECTORY / name).read_text(encoding="utf-8")


def read_repository_file(path: str) -> str:
    return (REPOSITORY_ROOT / path).read_text(encoding="utf-8")


def event_block(workflow: str, event: str) -> str:
    lines = workflow.splitlines()
    marker = f"  {event}:"
    try:
        start = lines.index(marker)
    except ValueError as error:
        raise AssertionError(f"workflow is missing the {event!r} event") from error

    block: list[str] = []
    for line in lines[start + 1 :]:
        if re.match(r"^  [A-Za-z_][A-Za-z0-9_-]*:", line):
            break
        if line and not line.startswith(" "):
            break
        block.append(line)
    return "\n".join(block)


def inline_list(block: str, key: str) -> list[str]:
    match = re.search(rf"^\s*{re.escape(key)}:\s*\[([^]]*)\]", block, re.MULTILINE)
    if match is None:
        raise AssertionError(f"event block is missing an inline {key!r} list")
    return [item.strip().strip("\"'") for item in match.group(1).split(",")]


def job_block(workflow: str, job: str) -> str:
    lines = workflow.splitlines()
    marker = f"  {job}:"
    try:
        start = lines.index(marker)
    except ValueError as error:
        raise AssertionError(f"workflow is missing the {job!r} job") from error

    block = [lines[start]]
    for line in lines[start + 1 :]:
        if re.match(r"^  [A-Za-z_][A-Za-z0-9_-]*:", line):
            break
        block.append(line)
    return "\n".join(block)


def workflow_job_names(workflow: str) -> list[str]:
    try:
        jobs = workflow.split("\njobs:\n", maxsplit=1)[1]
    except IndexError as error:
        raise AssertionError("workflow is missing its jobs block") from error
    return re.findall(r"^  ([A-Za-z_][A-Za-z0-9_-]*):\s*$", jobs, re.MULTILINE)


def action_metadata_files() -> list[Path]:
    files = list(WORKFLOW_DIRECTORY.glob("*.yml"))
    files.extend(WORKFLOW_DIRECTORY.glob("*.yaml"))
    files.extend(ACTION_DIRECTORY.glob("**/action.yml"))
    files.extend(ACTION_DIRECTORY.glob("**/action.yaml"))
    return sorted(files)


def upload_artifact_blocks(source: str) -> list[str]:
    """Return each upload-artifact step, independent of its YAML nesting depth."""
    lines = source.splitlines()
    blocks: list[str] = []
    for index, line in enumerate(lines):
        if "uses: actions/upload-artifact@" not in line:
            continue
        action_indent = len(line) - len(line.lstrip())
        step_indent = action_indent - 2
        block_start = index
        while block_start > 0:
            candidate = lines[block_start - 1]
            if (
                candidate.strip()
                and len(candidate) - len(candidate.lstrip()) <= step_indent
            ):
                break
            block_start -= 1
        block_end = index + 1
        while block_end < len(lines):
            candidate = lines[block_end]
            if (
                candidate.strip()
                and len(candidate) - len(candidate.lstrip()) <= step_indent
            ):
                break
            block_end += 1
        blocks.append("\n".join(lines[block_start:block_end]))
    return blocks


class AnalysisWorkflowPolicyTests(unittest.TestCase):
    def test_codeql_push_targets_default_branch(self) -> None:
        push = event_block(read_workflow("codeql-analysis.yml"), "push")

        self.assertEqual(inline_list(push, "branches"), ["ieum/main"])

    def test_sonarcloud_push_targets_default_branch_and_configuration_files(
        self,
    ) -> None:
        push = event_block(read_workflow("sonarcloud-analysis.yml"), "push")

        self.assertEqual(inline_list(push, "branches"), ["ieum/main"])
        self.assertIn("- '.github/workflows/sonarcloud-analysis.yml'", push)
        self.assertIn("- 'sonar-project.properties'", push)
        self.assertNotIn("- '.github/workflows/codeql-analysis.yml'", push)

    def test_both_analysis_workflows_support_safe_manual_dispatch(self) -> None:
        for name in ("codeql-analysis.yml", "sonarcloud-analysis.yml"):
            with self.subTest(workflow=name):
                workflow = read_workflow(name)
                self.assertIn("  workflow_dispatch:", workflow)

    def test_sonarcloud_reports_why_analysis_is_not_configured(self) -> None:
        workflow = read_workflow("sonarcloud-analysis.yml")

        self.assertIn("id: configuration", workflow)
        self.assertIn(
            "SONAR_SCANNER_ENABLED: ${{ vars.SONAR_SCANNER_ENABLED }}", workflow
        )
        self.assertIn("SONAR_TOKEN: ${{ secrets.SONAR_TOKEN }}", workflow)
        self.assertIn("$GITHUB_STEP_SUMMARY", workflow)
        self.assertIn("steps.configuration.outputs.run_analysis", workflow)

    def test_required_ci_runs_workflow_policy_regressions(self) -> None:
        workflow = read_workflow("continuous-integration.yml")

        self.assertIn("workflow-policy", workflow)
        self.assertIn(
            "python3 -m unittest discover -s tools/ci/tests -p 'test_*.py' -v",
            workflow,
        )

    def test_existing_tag_release_recovery_is_bound_and_privacy_gated(self) -> None:
        workflow = read_workflow("release-existing-run.yml")

        self.assertIn("  workflow_dispatch:", workflow)
        self.assertIn("      source_run_id:", workflow)
        self.assertIn("run-id: ${{ inputs.source_run_id }}", workflow)
        self.assertIn('[[ "${source_run[0]}" == "$release_sha" ]]', workflow)
        self.assertIn('[[ "${source_run[1]}" == "$RELEASE_TAG" ]]', workflow)
        self.assertIn('[[ "${source_run[3]}" == "success" ]]', workflow)
        self.assertIn(
            "target_commitish: ${{ steps.bind.outputs.release_sha }}", workflow
        )
        self.assertIn("Run exact release privacy gate", workflow)
        self.assertIn("Run post-fetch ref and byte verification", workflow)
        self.assertIn('--ref "refs/tags/$RELEASE_TAG"', workflow)
        self.assertIn("persist-credentials: false", workflow)

    def test_contributor_docs_explain_sonarcloud_configuration(self) -> None:
        documentation = (
            REPOSITORY_ROOT / "docs" / "dev" / "contributing.md"
        ).read_text(encoding="utf-8")

        self.assertIn("SONAR_SCANNER_ENABLED", documentation)
        self.assertIn("SONAR_TOKEN", documentation)
        self.assertIn("workflow_dispatch", documentation)

    def test_valgrind_action_preserves_failures_and_defines_leak_policy(self) -> None:
        action = read_repository_file(".github/actions/run-valgrind/action.yml")

        self.assertIn("set -o pipefail", action)
        self.assertIn("--error-exitcode=97", action)
        self.assertIn("--errors-for-leak-kinds=definite,indirect", action)
        self.assertIn("Ambiguous possible leaks", action)
        self.assertIn("--default-suppressions=yes", action)
        self.assertIn('exit "$VALGRIND_EXIT_CODE"', action)
        self.assertNotIn("continue-on-error: true", action)

    def test_valgrind_is_part_of_the_required_ci_result(self) -> None:
        workflow = read_workflow("continuous-integration.yml")

        self.assertRegex(workflow, r"needs:\s*\[[^]]*analyze-valgrind[^]]*\]")
        self.assertIn("needs.analyze-valgrind.result", workflow)

    def test_release_chain_runs_after_the_gate_accepts_expected_skips(self) -> None:
        workflow = read_workflow("continuous-integration.yml")

        release = job_block(workflow, "release")
        self.assertIn("always() && needs.ci-passed.result == 'success'", release)
        self.assertIn("startsWith(github.ref, 'refs/tags/v')", release)

        post_fetch = job_block(workflow, "privacy-post-fetch")
        self.assertIn("always() && needs.release.result == 'success'", post_fetch)
        self.assertIn("startsWith(github.ref, 'refs/tags/v')", post_fetch)

    def test_release_privacy_failures_preserve_actionable_evidence(self) -> None:
        for name in ("continuous-integration.yml", "release-existing-run.yml"):
            with self.subTest(workflow=name, phase="pre-release"):
                release = job_block(read_workflow(name), "release")
                self.assertIn("id: release_privacy_gate", release)
                self.assertIn("continue-on-error: true", release)
                self.assertIn("steps.release_privacy_gate.outcome", release)
                self.assertIn("tools/privacy/report_public_manifest.py", release)
                self.assertIn("Enforce pre-release privacy gate", release)

            with self.subTest(workflow=name, phase="post-fetch"):
                post_fetch = job_block(read_workflow(name), "privacy-post-fetch")
                self.assertIn("id: post_fetch_privacy_gate", post_fetch)
                self.assertIn("continue-on-error: true", post_fetch)
                self.assertIn("steps.post_fetch_privacy_gate.outcome", post_fetch)
                self.assertIn("tools/privacy/report_public_manifest.py", post_fetch)
                self.assertIn("Enforce post-fetch privacy gate", post_fetch)

    def test_windows_upgrade_uses_the_previous_published_release(self) -> None:
        script = read_repository_file(
            ".github/actions/test-package/test-windows-msi-upgrade.ps1"
        )

        self.assertIn("gh release view $candidateTag", script)
        self.assertIn("Skipping unpublished tag $candidateTag", script)
        self.assertIn('$searchRevision = "$candidateTag^"', script)

    def test_valgrind_workflow_proves_invalid_access_fails(self) -> None:
        workflow = read_workflow("valgrind-analysis.yml")
        fixture_path = "tools/ci/fixtures/valgrind-invalid-access.c"

        self.assertIn(fixture_path, workflow)
        self.assertIn("id: invalid-access", workflow)
        self.assertIn("continue-on-error: true", workflow)
        self.assertIn(
            "INVALID_ACCESS_OUTCOME: ${{ steps.invalid-access.outcome }}", workflow
        )
        self.assertIn('[[ "$INVALID_ACCESS_OUTCOME" != "failure" ]]', workflow)

        fixture = REPOSITORY_ROOT / fixture_path
        self.assertTrue(fixture.is_file(), f"missing Valgrind fixture: {fixture_path}")
        fixture_source = fixture.read_text(encoding="utf-8")
        self.assertIn("volatile", fixture_source)
        self.assertIn("free(", fixture_source)

    def test_valgrind_workflow_runs_selected_core_tests(self) -> None:
        workflow = read_workflow("valgrind-analysis.yml")

        selected_executables = (
            "./build/bin/legacytests",
            "./build/src/unittests/net/SecureSocketWriteBufferTests",
            "./build/src/unittests/deskflow/Protocol19Tests",
            "./build/src/unittests/server/ServerConfigTests",
        )
        for executable in selected_executables:
            with self.subTest(executable=executable):
                self.assertIn(executable, workflow)

        self.assertIn("if: ${{ always() }}", workflow)

    def test_all_external_actions_are_pinned_to_reviewed_commits(self) -> None:
        violations: list[str] = []
        for path in action_metadata_files():
            source = path.read_text(encoding="utf-8")
            for line_number, line in enumerate(source.splitlines(), start=1):
                match = re.match(
                    r"^\s*(?:-\s+)?uses:\s*([^\s#]+)(?:\s+#\s*(.+))?$", line
                )
                if match is None:
                    continue

                action, version_comment = match.groups()
                if action.startswith("./"):
                    continue
                if action.startswith("docker://"):
                    if "@sha256:" not in action:
                        violations.append(
                            f"{path.relative_to(REPOSITORY_ROOT)}:{line_number}: "
                            "container action is not digest-pinned"
                        )
                    continue

                _, separator, revision = action.rpartition("@")
                if separator != "@" or re.fullmatch(r"[0-9a-f]{40}", revision) is None:
                    violations.append(
                        f"{path.relative_to(REPOSITORY_ROOT)}:{line_number}: "
                        f"external action is not pinned to a full commit: {action}"
                    )
                if (
                    version_comment is None
                    or re.match(r"^v\d", version_comment) is None
                ):
                    violations.append(
                        f"{path.relative_to(REPOSITORY_ROOT)}:{line_number}: "
                        "pinned action is missing a readable version comment"
                    )

        self.assertEqual(violations, [], "\n".join(violations))

    def test_artifact_uploads_have_bounded_purpose_specific_retention(self) -> None:
        violations: list[str] = []
        for path in action_metadata_files():
            source = path.read_text(encoding="utf-8")
            for block in upload_artifact_blocks(source):
                expected = 30 if "name: privacy-release-" in block else 1
                if f"retention-days: {expected}" not in block:
                    violations.append(
                        f"{path.relative_to(REPOSITORY_ROOT)}: expected "
                        f"retention-days: {expected}\n{block}"
                    )

        self.assertEqual(violations, [], "\n\n".join(violations))

    def test_workflows_declare_explicit_default_permissions(self) -> None:
        for path in sorted(WORKFLOW_DIRECTORY.glob("*.yml")):
            with self.subTest(workflow=path.name):
                workflow = path.read_text(encoding="utf-8")
                header = workflow.split("\njobs:\n", maxsplit=1)[0]
                self.assertIn("\npermissions:\n", header)
                self.assertNotIn("write-all", header)
                self.assertNotIn("contents: write", header)
                if path.name == "ci-comment.yml":
                    self.assertIn("actions: read", header)
                    self.assertIn("pull-requests: write", header)
                else:
                    self.assertIn("contents: read", header)

    def test_only_publish_and_release_jobs_can_write_contents(self) -> None:
        workflow = read_workflow("continuous-integration.yml")
        writers = {
            name
            for name in workflow_job_names(workflow)
            if "contents: write" in job_block(workflow, name)
        }

        self.assertEqual(writers, {"publish-development-docs", "release"})
        self.assertNotIn("actions-gh-pages", job_block(workflow, "main-build"))
        publisher = job_block(workflow, "publish-development-docs")
        self.assertIn("needs: main-build", publisher)
        self.assertIn("actions-gh-pages", publisher)
        self.assertIn("persist-credentials: false", publisher)

    def test_dependabot_updates_actions_on_the_default_branch(self) -> None:
        path = REPOSITORY_ROOT / ".github" / "dependabot.yml"
        self.assertTrue(path.is_file(), "missing .github/dependabot.yml")
        configuration = path.read_text(encoding="utf-8")

        self.assertIn('package-ecosystem: "github-actions"', configuration)
        self.assertIn('directory: "/"', configuration)
        self.assertIn('interval: "weekly"', configuration)
        self.assertIn('target-branch: "ieum/main"', configuration)


if __name__ == "__main__":
    unittest.main()
