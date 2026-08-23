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


if __name__ == "__main__":
    unittest.main()
