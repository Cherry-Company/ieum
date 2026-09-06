# SPDX-FileCopyrightText: (C) 2026 Ieum contributors
# SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception

import copy
import unittest

from tools.ci.validate_release_source import validate


class ReleaseSourceTests(unittest.TestCase):
    def setUp(self):
        self.run = dict(id=42, run_attempt=1, path=".github/workflows/continuous-integration.yml",
                        head_sha="a" * 40, head_branch="v0.1.0-alpha.26", event="push",
                        status="completed", conclusion="failure")
        self.jobs = [dict(name=name, run_id=42, run_attempt=1, head_sha="a" * 40,
                          status="completed", conclusion=result) for name, result in (
            ("privacy-guard", "success"), ("windows-2022-x64", "success"),
            ("ci-passed", "success"), ("analyze-valgrind", "skipped"),
            ("publish-development-docs", "skipped"), ("release", "failure"),
            ("privacy-post-fetch", "skipped"))]

    def check(self, run=None, jobs=None):
        validate(run or self.run, [{"jobs": self.jobs if jobs is None else jobs}],
                 "v0.1.0-alpha.26", "a" * 40)

    def test_publish_only_failure_is_recoverable(self):
        self.check()

    def test_successful_run_is_recoverable(self):
        self.run["conclusion"] = "success"
        self.jobs[-2]["conclusion"] = "success"
        self.jobs[-1]["conclusion"] = "success"
        self.check()

    def test_wrong_source_identity_is_rejected(self):
        for key, value in (("head_sha", "b" * 40), ("head_branch", "main"),
                           ("event", "workflow_dispatch"), ("path", "other.yml"),
                           ("status", "in_progress"), ("conclusion", "cancelled")):
            with self.subTest(key=key), self.assertRaises(ValueError):
                self.check(run={**self.run, key: value})

    def test_gate_must_exist_once_and_pass(self):
        for jobs in (self.jobs[:2] + self.jobs[3:], self.jobs + [self.jobs[2]],
                     [{**job, "conclusion": "failure"} if job["name"] == "ci-passed"
                      else job for job in self.jobs]):
            with self.subTest(jobs=jobs), self.assertRaises(ValueError):
                self.check(jobs=jobs)

    def test_other_failures_or_skipped_builds_are_rejected(self):
        for result in ("failure", "cancelled", "timed_out", "skipped", "neutral", None):
            jobs = copy.deepcopy(self.jobs)
            jobs[1]["conclusion"] = result
            with self.subTest(result=result), self.assertRaises(ValueError):
                self.check(jobs=jobs)

    def test_incomplete_or_mixed_attempt_jobs_are_rejected(self):
        for key, value in (("run_attempt", 2), ("run_id", 43),
                           ("head_sha", "b" * 40), ("status", "in_progress")):
            jobs = copy.deepcopy(self.jobs)
            jobs[1][key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):
                self.check(jobs=jobs)

    def test_all_pages_are_checked(self):
        with self.assertRaises(ValueError):
            validate(self.run, [{"jobs": self.jobs}, {"jobs": [
                {**self.jobs[1], "name": "macos-arm64", "conclusion": "failure"}]}],
                "v0.1.0-alpha.26", "a" * 40)


if __name__ == "__main__":
    unittest.main()
