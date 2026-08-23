# Contributing to Ieum {#contributing_guide}

Thanks for your interest in contributing to Ieum! We welcome all kinds of contributions — bug reports, feature suggestions, documentation improvements, and code.

## Read the Full Guidelines

To keep this repository clean and contribution-friendly, we've outlined our full contributing guidelines on the Ieum Wiki:

👉 [How to Contribute to Ieum](https://github.com/Cherry-Company/ieum/wiki/Contributing)

Please take a moment to read through the page before opening an issue or submitting a pull request.

## Static Analysis Workflows

CodeQL and SonarCloud run automatically when relevant files reach the
`ieum/main` branch. Maintainers can also verify either workflow safely with its
`workflow_dispatch` action before a release.

SonarCloud analysis requires both of these repository settings:

- Actions variable `SONAR_SCANNER_ENABLED` set to `true`.
- Actions secret `SONAR_TOKEN` containing the project analysis token.

When either setting is unavailable, including on pull requests from forks, the
SonarCloud job succeeds without uploading an analysis and records an explicit
"Skipped intentionally" reason in the workflow summary. Contributors do not
need access to either setting.

Thanks again for helping make Ieum better!
