#!/bin/sh
# SPDX-FileCopyrightText: (C) 2026 Ieum contributors
# SPDX-License-Identifier: MIT

set -eu

usage() {
  printf '%s\n' "usage: tools/privacy/install_hook.sh --install|--check" >&2
  exit 2
}

[ "$#" -eq 1 ] || usage
mode=$1
case "$mode" in
  --install|--check) ;;
  *) usage ;;
esac

root=$(git rev-parse --show-toplevel)
cd "$root"

git ls-files --error-unmatch .githooks/pre-push >/dev/null 2>&1 || {
  printf '%s\n' "tracked .githooks/pre-push is missing" >&2
  exit 1
}
[ -f .githooks/pre-push ] && [ -x .githooks/pre-push ] || {
  printf '%s\n' ".githooks/pre-push must be a regular executable file" >&2
  exit 1
}

if [ "$mode" = "--install" ]; then
  git config --local core.hooksPath .githooks
fi

configured=$(git config --local --get core.hooksPath || true)
[ "$configured" = ".githooks" ] || {
  printf '%s\n' "core.hooksPath is not exactly .githooks" >&2
  exit 1
}

printf '%s\n' "Ieum privacy hook active: core.hooksPath=.githooks"
