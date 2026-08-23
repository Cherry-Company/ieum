# SPDX-FileCopyrightText: (C) 2026 Ieum contributors
# SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception

from __future__ import annotations

import re
import unittest
from pathlib import Path

REPOSITORY_ROOT = Path(__file__).resolve().parents[3]
WORKFLOW_DIRECTORY = REPOSITORY_ROOT / ".github" / "workflows"


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


if __name__ == "__main__":
    unittest.main()
