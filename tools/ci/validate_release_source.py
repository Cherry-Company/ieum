# SPDX-FileCopyrightText: (C) 2026 Ieum contributors
# SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception

"""Bind release recovery to a completed tag CI attempt whose build gate passed."""

import json
import sys
from pathlib import Path


def validate(run, pages, tag, sha):
    expected = {
        "path": ".github/workflows/continuous-integration.yml",
        "head_sha": sha,
        "head_branch": tag,
        "event": "push",
        "status": "completed",
    }
    if any(run.get(key) != value for key, value in expected.items()):
        raise ValueError("source must be the completed CI workflow for the exact pushed tag")
    if run.get("conclusion") not in ("success", "failure"):
        raise ValueError("source run did not finish normally")
    jobs = [job for page in pages for job in page["jobs"]]
    gates = [job for job in jobs if job.get("name") == "ci-passed"]
    if len(gates) != 1 or gates[0].get("conclusion") != "success":
        raise ValueError("source must contain exactly one successful ci-passed gate")
    for job in jobs:
        if (job.get("run_id") != run["id"]
                or job.get("run_attempt") != run["run_attempt"]
                or job.get("head_sha") != sha
                or job.get("status") != "completed"):
            raise ValueError("source jobs are incomplete or from a different CI attempt")
        if job["name"] in ("release", "privacy-post-fetch"):
            continue
        if job["name"] in ("analyze-valgrind", "publish-development-docs"):
            if job.get("conclusion") == "skipped":
                continue
        if job.get("conclusion") != "success":
            raise ValueError("source contains an unsuccessful build or validation job")


if __name__ == "__main__":
    run_path, jobs_path, tag, sha = sys.argv[1:]
    try:
        validate(json.loads(Path(run_path).read_text(encoding="utf-8-sig")),
                 json.loads(Path(jobs_path).read_text(encoding="utf-8-sig")), tag, sha)
    except (ValueError, KeyError, TypeError) as error:
        raise SystemExit(str(error)) from error
    print("Source tag, workflow, completed attempt, and required CI gate verified")
